#include "ir_core.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    std::string error;
    ir::Waveform original{38000, 33, {9000, 4500, 560, 560, 560}};
    assert(ir::validate(original, error));
    assert(ir::valid_name("fan_low"));
    assert(!ir::valid_name("../bad"));
    assert(!ir::valid_name("name_is_too_long_here"));
    assert(!ir::valid_name(""));
    auto encoded = ir::serialize(original);
    ir::Waveform decoded;
    assert(ir::deserialize(encoded, decoded, error));
    assert(decoded.durations_us == original.durations_us);
    assert(decoded.carrier_hz == original.carrier_hz);
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        auto damaged = encoded;
        damaged[i] ^= 1;
        assert(!ir::deserialize(damaged, decoded, error));
    }
    auto shortened = encoded;
    shortened.pop_back();
    assert(!ir::deserialize(shortened, decoded, error));
    assert(!ir::deserialize({}, decoded, error));
    auto invalid = original;
    invalid.durations_us = {500001};
    assert(!ir::validate(invalid, error));
    invalid.durations_us = {1, 1};
    assert(!ir::validate(invalid, error));
    invalid.durations_us = {0};
    assert(!ir::validate(invalid, error));
    invalid = original;
    invalid.duty_percent = 99;
    assert(!ir::validate(invalid, error));

    // A synthetic 40 kHz carrier at a 2 MHz timebase contains 50 ticks/cycle.
    // Two 1 ms marks separated by 500 us exercise carrier-envelope recovery.
    std::vector<ir::Sample> capture{{true, 200}};
    for (int mark = 0; mark < 2; ++mark) {
        for (int cycle = 0; cycle < 40; ++cycle) {
            capture.push_back({false, 17});
            capture.push_back({true, static_cast<std::uint32_t>(cycle == 39 ? 1033 : 33)});
        }
    }
    assert(ir::decode_capture(capture, 2000000, true, 0, decoded, error));
    assert(decoded.carrier_hz == 40000);
    assert((decoded.durations_us == std::vector<std::uint32_t>{1000, 500, 1000}));
    // A capture may omit the long idle marker. A final short HIGH already
    // contains the last carrier OFF phase; a final LOW still needs that phase.
    std::vector<ir::Sample> no_idle;
    for (unsigned cycle = 0; cycle < 40; ++cycle) {
        no_idle.push_back({false, 17});
        no_idle.push_back({true, 33});
    }
    assert(ir::decode_capture(no_idle, 2000000, true, 0, decoded, error));
    assert((decoded.durations_us == std::vector<std::uint32_t>{1000}));
    no_idle.pop_back();
    assert(ir::decode_capture(no_idle, 2000000, true, 0, decoded, error));
    assert((decoded.durations_us == std::vector<std::uint32_t>{1000}));
    // Quantized periods at the receiver band edges must remain usable. Periods
    // beyond that band must fail rather than becoming an arbitrary default.
    for (unsigned period : {67U, 53U, 36U, 33U, 70U, 32U}) {
        std::vector<ir::Sample> quantized;
        const unsigned low = period / 3;
        for (unsigned cycle = 0; cycle < 40; ++cycle) {
            quantized.push_back({false, low});
            quantized.push_back({true, period - low + (cycle == 39 ? 30000U : 0U)});
        }
        const bool accepted = ir::decode_capture(quantized, 2000000, true, 0, decoded, error);
        if (period == 70 || period == 32) {
            assert(!accepted);
        } else {
            assert(accepted);
            assert(decoded.durations_us.size() == 1);
            assert(decoded.durations_us[0] == period * 20);
            assert(decoded.carrier_hz >= 30000 && decoded.carrier_hz <= 60000);
            if (period == 67) assert(decoded.carrier_hz == 30000);
            if (period == 33) assert(decoded.carrier_hz == 60000);
        }
    }
    for (const auto &widths : {std::pair<unsigned, unsigned>{35, 33}, {1000, 33}, {1, 1}, {4, 96}}) {
        auto noisy = capture;
        noisy[5].ticks = widths.first;
        noisy[6].ticks = widths.second;
        assert(!ir::decode_capture(noisy, 2000000, true, 0, decoded, error));
    }
    assert(!ir::decode_capture({{true, 2000}}, 2000000, true, 0, decoded, error));
    assert(!ir::decode_capture({{false, 10}}, 2000000, true, 0, decoded, error));
    assert(ir::decode_capture({{true, 100}, {false, 1120}, {true, 1120},
                               {false, 1120}, {true, 30000}},
                              2000000, false, 38000, decoded, error));
    assert((decoded.durations_us == std::vector<std::uint32_t>{560, 560, 560}));

    const ir::Profile fan{"fan", ir::Kind::Fan, true, 0, "on", "off", "low", "med", "high"};
    assert(std::string(ir::fan_command(fan, 0)) == "off");
    assert(std::string(ir::fan_command(fan, 33)) == "low");
    assert(std::string(ir::fan_command(fan, 34)) == "med");
    assert(std::string(ir::fan_command(fan, 66)) == "med");
    assert(std::string(ir::fan_command(fan, 67)) == "high");
    assert(ir::fan_command(fan, 101) == nullptr);
    assert(ir::fan_mode_percent(5) == 101);
    assert(ir::fan_mode_percent(4) == 100);
    assert(ir::fan_mode_percent(6) == 100);
    std::cout << "IR core: capture, bounds, persistence corruption, and fan mapping passed\n";
}
