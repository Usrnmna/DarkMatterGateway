#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ir {

// These bounds define the persisted waveform contract and the transmitter budget.
constexpr std::size_t kMaxDurations = 4096;
constexpr std::uint32_t kMaxFrameUs = 500000;
constexpr std::uint32_t kMinCarrierHz = 30000;
constexpr std::uint32_t kMaxCarrierHz = 60000;

struct Waveform {
    std::uint32_t carrier_hz = 38000;
    std::uint8_t duty_percent = 33;
    // Alternating mark/space lengths, beginning and ending with a mark.
    std::vector<std::uint32_t> durations_us;
};

struct Sample {
    bool high;
    std::uint32_t ticks;
};

// Validate names before using them as NVS keys or console arguments.
bool valid_name(const std::string &name);
// Check carrier, duty, duration count, and total time; provide a rejection reason.
bool validate(const Waveform &waveform, std::string &error);

// Decode active-low carrier samples from TSMP98000, or a demodulated receiver.
// Carrier capture estimates frequency; demodulated capture requires a known one.
bool decode_capture(const std::vector<Sample> &samples, std::uint32_t tick_hz,
                    bool carrier_samples, std::uint32_t known_carrier_hz,
                    Waveform &result, std::string &error);

// The storage format is explicit little-endian with version and CRC32; it does
// not depend on compiler structure packing or the target processor's ABI.
std::vector<std::uint8_t> serialize(const Waveform &waveform);
// Reject unknown versions or corrupt contents without changing result on failure.
bool deserialize(const std::vector<std::uint8_t> &bytes, Waveform &result,
                 std::string &error);

enum class Kind { Light, Fan, GasFireplace, PortableAc, Raw };
struct Profile {
    const char *id;
    Kind kind;
    bool enabled;
    std::uint8_t zone; // 0: onboard array, 1/2: external emitter ports.
    const char *on;
    const char *off;
    const char *low;
    const char *medium;
    const char *high;
};

// Fan control uses discrete learned speed commands. Zero means off; nonzero
// percentages select one of three explicitly mapped speeds.
const char *fan_command(const Profile &profile, unsigned percent);
// Normalize supported Matter fan modes; 101 denotes an unsupported mode.
unsigned fan_mode_percent(unsigned mode);

} // namespace ir
