#include "ir_core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ir {
namespace {

std::uint32_t crc32(const std::uint8_t *data, std::size_t size)
{
    std::uint32_t crc = 0xFFFFFFFF;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

void put32(std::vector<std::uint8_t> &bytes, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint32_t get32(const std::vector<std::uint8_t> &bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte) {
        value |= static_cast<std::uint32_t>(bytes[offset + byte]) << (8 * byte);
    }
    return value;
}

std::uint32_t to_us(std::uint64_t ticks, std::uint32_t tick_hz)
{
    return static_cast<std::uint32_t>((ticks * 1000000 + tick_hz / 2) / tick_hz);
}

} // namespace

bool valid_name(const std::string &name)
{
    if (name.empty() || name.size() > 15) return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

bool validate(const Waveform &waveform, std::string &error)
{
    if (waveform.carrier_hz < kMinCarrierHz || waveform.carrier_hz > kMaxCarrierHz) {
        error = "carrier must be 30000..60000 Hz";
        return false;
    }
    if (waveform.duty_percent < 10 || waveform.duty_percent > 50) {
        error = "carrier duty must be 10..50 percent";
        return false;
    }
    if (waveform.durations_us.empty() || waveform.durations_us.size() > kMaxDurations ||
        waveform.durations_us.size() % 2 == 0) {
        error = "waveform must contain an odd, bounded count of mark/space durations";
        return false;
    }
    std::uint64_t total = 0;
    for (auto duration : waveform.durations_us) {
        if (duration == 0 || duration > kMaxFrameUs) {
            error = "zero or excessive duration";
            return false;
        }
        total += duration;
    }
    if (total > kMaxFrameUs) {
        error = "frame exceeds 500 ms";
        return false;
    }
    error.clear();
    return true;
}

bool decode_capture(const std::vector<Sample> &samples, std::uint32_t tick_hz,
                    bool carrier_samples, std::uint32_t known_carrier_hz,
                    Waveform &result, std::string &error)
{
    if (tick_hz == 0 || samples.empty() || samples.size() > 32768) {
        error = "empty or oversized capture, or invalid timebase";
        return false;
    }
    // Normalize adjacent equal levels and remove the leading idle interval.
    std::vector<Sample> levels;
    std::uint64_t capture_ticks = 0;
    for (const auto &sample : samples) {
        if (!sample.ticks) continue;
        capture_ticks += sample.ticks;
        if (capture_ticks > static_cast<std::uint64_t>(tick_hz) * 2) {
            error = "capture duration exceeds two seconds";
            return false;
        }
        if (levels.empty() && sample.high) continue;
        if (!levels.empty() && levels.back().high == sample.high) {
            if (sample.ticks > std::numeric_limits<std::uint32_t>::max() - levels.back().ticks) {
                error = "capture duration overflow";
                return false;
            }
            levels.back().ticks += sample.ticks;
        } else {
            levels.push_back(sample);
        }
    }
    if (levels.empty()) {
        error = "no active signal";
        return false;
    }

    Waveform decoded;
    if (!carrier_samples) {
        decoded.carrier_hz = known_carrier_hz;
        if (levels.back().high) levels.pop_back(); // Stop interval is not command data.
        for (const auto &level : levels) decoded.durations_us.push_back(to_us(level.ticks, tick_hz));
    } else {
        // Estimate periods from complete low/high cycles. Long high intervals
        // are envelope spaces, so they must not bias carrier measurement.
        std::vector<std::uint32_t> periods;
        std::vector<std::uint32_t> high_ticks;
        for (std::size_t i = 0; i + 1 < levels.size(); i += 2) {
            const auto period = levels[i].ticks + levels[i + 1].ticks;
            const double frequency = static_cast<double>(tick_hz) / period;
            if (frequency >= 28000 && frequency <= 63000) {
                periods.push_back(period);
                high_ticks.push_back(levels[i + 1].ticks);
            }
        }
        if (periods.size() < 16) {
            error = "insufficient carrier cycles; reposition the remote or use a known-frequency receiver";
            return false;
        }
        auto sorted = periods;
        std::sort(sorted.begin(), sorted.end());
        const auto median = sorted[sorted.size() / 2];
        std::uint64_t sum = 0;
        for (auto period : periods) {
            if (std::abs(static_cast<double>(period) - median) > median * 0.12) {
                error = "inconsistent carrier; possible noise or unsupported modulation";
                return false;
            }
            sum += period;
        }
        const double mean_period = static_cast<double>(sum) / periods.size();
        // Every active pulse must agree with the measured carrier. Filtering
        // estimator candidates alone would let long or short corrupt cycles
        // escape validation and silently alter the recovered envelope.
        for (std::size_t i = 0; i < levels.size(); i += 2) {
            if (levels[i].ticks < mean_period * 0.10 || levels[i].ticks > mean_period * 0.90) {
                error = "carrier pulse width inconsistent with the measured frequency";
                return false;
            }
            if (i + 1 < levels.size() && levels[i + 1].ticks <= mean_period * 1.75) {
                const auto period = levels[i].ticks + levels[i + 1].ticks;
                if (levels[i + 1].ticks < mean_period * 0.10 ||
                    std::abs(static_cast<double>(period) - mean_period) > mean_period * 0.12) {
                    error = "malformed carrier cycle";
                    return false;
                }
            }
        }
        decoded.carrier_hz = static_cast<std::uint32_t>(std::lround(tick_hz / mean_period));
        // Quantization near a supported band edge may cross the nominal bound.
        if (decoded.carrier_hz >= 29500 && decoded.carrier_hz < kMinCarrierHz)
            decoded.carrier_hz = kMinCarrierHz;
        if (decoded.carrier_hz <= 61000 && decoded.carrier_hz > kMaxCarrierHz)
            decoded.carrier_hz = kMaxCarrierHz;
        std::sort(high_ticks.begin(), high_ticks.end());
        const auto trailing_off = high_ticks[high_ticks.size() / 2];
        std::uint64_t mark_ticks = 0;
        for (std::size_t i = 0; i < levels.size(); ++i) {
            const auto &level = levels[i];
            if (!level.high || level.ticks <= mean_period * 1.75) {
                mark_ticks += level.ticks;
                continue;
            }
            // The final carrier OFF phase belongs to the mark; the remaining
            // high interval belongs to the envelope space.
            if (mark_ticks == 0 || level.ticks <= trailing_off) {
                error = "invalid carrier envelope";
                return false;
            }
            decoded.durations_us.push_back(to_us(mark_ticks + trailing_off, tick_hz));
            mark_ticks = 0;
            if (i + 1 < levels.size()) {
                decoded.durations_us.push_back(to_us(level.ticks - trailing_off, tick_hz));
            }
        }
        if (mark_ticks) {
            // A short terminal HIGH already contains the final carrier OFF
            // phase. Add an inferred phase only when capture ends on LOW.
            const auto missing_off = levels.back().high ? 0 : trailing_off;
            decoded.durations_us.push_back(to_us(mark_ticks + missing_off, tick_hz));
        }
    }
    if (!validate(decoded, error)) return false;
    result = std::move(decoded);
    return true;
}

std::vector<std::uint8_t> serialize(const Waveform &waveform)
{
    std::string error;
    if (!validate(waveform, error)) return {};
    std::vector<std::uint8_t> bytes{'I', 'R', 'W', '1'};
    put32(bytes, waveform.carrier_hz);
    put32(bytes, waveform.duty_percent);
    put32(bytes, static_cast<std::uint32_t>(waveform.durations_us.size()));
    for (auto duration : waveform.durations_us) put32(bytes, duration);
    put32(bytes, crc32(bytes.data(), bytes.size()));
    return bytes;
}

bool deserialize(const std::vector<std::uint8_t> &bytes, Waveform &result, std::string &error)
{
    if (bytes.size() < 24 || bytes.size() > 20 + 4 * kMaxDurations ||
        bytes[0] != 'I' || bytes[1] != 'R' || bytes[2] != 'W' || bytes[3] != '1') {
        error = "invalid waveform header or size";
        return false;
    }
    const auto count = get32(bytes, 12);
    if (count > kMaxDurations || bytes.size() != 20 + 4 * count ||
        crc32(bytes.data(), bytes.size() - 4) != get32(bytes, bytes.size() - 4)) {
        error = "invalid waveform length or CRC";
        return false;
    }
    Waveform decoded;
    decoded.carrier_hz = get32(bytes, 4);
    const auto duty = get32(bytes, 8);
    if (duty > 100) {
        error = "invalid persisted duty cycle";
        return false;
    }
    decoded.duty_percent = static_cast<std::uint8_t>(duty);
    for (std::size_t index = 0; index < count; ++index)
        decoded.durations_us.push_back(get32(bytes, 16 + 4 * index));
    if (!validate(decoded, error)) return false;
    result = std::move(decoded);
    return true;
}

const char *fan_command(const Profile &profile, unsigned percent)
{
    if (percent > 100) return nullptr;
    if (percent == 0) return profile.off;
    if (percent <= 33) return profile.low;
    if (percent <= 66) return profile.medium;
    return profile.high;
}

unsigned fan_mode_percent(unsigned mode)
{
    // Matter On and Smart normalize to High when this profile has no Auto mode.
    if (mode == 4 || mode == 6) return 100;
    constexpr unsigned percentages[] = {0, 33, 66, 100};
    return mode < 4 ? percentages[mode] : 101;
}

} // namespace ir
