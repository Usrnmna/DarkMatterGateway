#pragma once
#include "service_stubs/support.hpp"
#include <cassert>
#include <cmath>
#include <cstring>

// Include the production implementation, as in test_terminal.cpp. Private state
// is accessed only for simulated power-off between independent cases.
#include "../firmware/main/ir_service.cpp"

namespace service_test {
using fake::Op;
using ir_service::Receiver;

// Independent IRW1 fixtures: little-endian fields and CRC-32 computed with
// Python struct/zlib, rather than the serializer under test.
inline const fake::Bytes kRecord{
    0x49,0x52,0x57,0x31,0x70,0x94,0,0,0x21,0,0,0,3,0,0,0,
    0x30,2,0,0,0x30,2,0,0,0x30,2,0,0,0xf3,0xfd,0x13,0x6d};
inline const fake::Bytes kOtherRecord{
    0x49,0x52,0x57,0x31,0x40,0x9c,0,0,0x21,0,0,0,3,0,0,0,
    0xe8,3,0,0,0xf4,1,0,0,0xe8,3,0,0,0x51,0x0b,0x0f,0x5a};

// Each case asserts cleanup before teardown. Reclaiming fault-retained driver
// memory here models power-off; it is not an available service recovery API.
struct Fixture {
    explicit Fixture(bool initialize = true)
    {
        assert(!ir_service::initialized && !fake::state.worker);
        if (initialize) assert(ir_service::init() == ESP_OK);
    }
    ~Fixture()
    {
        assert(ir_service::outstanding_jobs == 0);
        if (fake::state.storage_open) nvs_close(ir_service::storage);
        fake::shutdown();
        ir_service::service_mutex = nullptr;
        ir_service::send_queue = nullptr;
        ir_service::storage = 0;
        ir_service::initialized = false;
        ir_service::hardware_fault = false;
        ir_service::pending = {};
        ir_service::pending_valid = false;
    }
};

inline void seed(const char *name = "command", const fake::Bytes &record = kRecord)
{
    fake::state.visible[name] = record;
    fake::state.committed[name] = record;
}

inline void assert_idle()
{
    assert(!ir_service::busy());
    assert(fake::allocations() == 0 && fake::channels() == 0 && fake::encoders() == 0);
    assert(fake::semaphores() == 1 && fake::queues() == 1);
    assert(fake::state.pins.at(18) == 1);
    for (int pin : {4, 5, 6}) assert(fake::state.pins.at(pin) == 0);
}

inline void assert_waveform(const ir::Waveform &waveform)
{
    assert(waveform.carrier_hz == 38000 && waveform.duty_percent == 33);
    assert((waveform.durations_us == std::vector<std::uint32_t>{560, 560, 560}));
}

inline void assert_symbols(const std::vector<rmt_symbol_word_t> &actual,
                           const std::vector<rmt_symbol_word_t> &expected)
{
    assert(actual.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        assert(actual[i].duration0 == expected[i].duration0 && actual[i].level0 == expected[i].level0);
        assert(actual[i].duration1 == expected[i].duration1 && actual[i].level1 == expected[i].level1);
    }
}

struct Case { const char *name; void (*run)(); };

// An optional case name permits focused failure reproduction and mutation checks.
inline int run_cases(int argc, char **argv, const std::vector<Case> &cases)
{
    unsigned count = 0;
    for (const auto &test : cases) {
        if (argc > 1 && std::strcmp(argv[1], test.name) != 0) continue;
        std::printf("RUN %s\n", test.name);
        std::fflush(stdout);
        test.run();
        ++count;
    }
    assert(count != 0);
    std::printf("PASS: %u service cases\n", count);
    return 0;
}
} // namespace service_test
