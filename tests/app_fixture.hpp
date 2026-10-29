#pragma once
#include "app_stubs/support.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../firmware/main/matter_app.cpp"

namespace app_test {
using app_fake::Op;
using namespace chip::app::Clusters;

// SDK-owned objects are reclaimed only at simulated power-off between cases.
// No test reset hook is added to the production adapter.
struct Fixture {
    Fixture()
    {
        assert(!matter_app::ready);
        for (auto &profile : configured::profiles) profile = {};
        configured::profiles[0] = {"light", ir::Kind::Light, true, 1, "lamp_on", "lamp_off", "", "", ""};
        configured::profiles[1] = {"fan", ir::Kind::Fan, true, 2, "", "fan_off", "fan_low", "fan_mid", "fan_high"};
        configured::profiles[2] = {"disabled", ir::Kind::Light, false, 0, "unused_on", "unused_off", "", "", ""};
        configured::profiles[3] = {"fire", ir::Kind::GasFireplace, true, 0, "fire_on", "fire_off", "", "", ""};
        configured::profiles[4] = {"ac", ir::Kind::PortableAc, true, 0, "ac_on", "ac_off", "", "", ""};
        configured::profiles[5] = {"raw", ir::Kind::Raw, true, 0, "raw_on", "raw_off", "", "", ""};
        configured::profiles[6] = {"disabled_fan", ir::Kind::Fan, false, 0, "", "spare_off", "spare_low", "spare_mid", "spare_high"};
        configured::profiles[7] = {"disabled_raw", ir::Kind::Raw, false, 0, "spare_on", "spare_off", "", "", ""};
    }
    ~Fixture()
    {
        app_fake::state = {};
        matter_app::ready = false;
        for (auto &context : matter_app::contexts) context = {};
    }
};

inline void assert_no_replay_or_erase()
{
    assert(app_fake::state.attempted.empty());
    assert(app_fake::calls(Op::Erase) == 0);
    assert(app_fake::calls(Op::X0aRequest) == 0);
}

inline esp_err_t fan_write(std::uint32_t attribute, std::uint8_t value)
{
    auto &fan = app_fake::endpoint("fan");
    return app_fake::write(fan.id, FanControl::Id, attribute,
        attribute == FanControl::Attributes::FanMode::Id ? esp_matter_enum8(value) : esp_matter_nullable_uint8(value));
}

inline void assert_fan_state(unsigned mode, unsigned percent)
{
    auto id = app_fake::endpoint("fan").id;
    assert(app_fake::state.values.at({id, 0x0202, 0}).val.u8 == mode);
    const auto setting = app_fake::state.values.at({id, 0x0202, 2});
    assert(setting.type == ESP_MATTER_VAL_TYPE_NULLABLE_UINT8 && setting.val.u8 == percent);
    assert(app_fake::state.values.at({id, 0x0202, 3}).val.u8 == percent);
}

struct Case { const char *name; void (*run)(); };
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
    std::printf("PASS: %u app cases\n", count);
    return 0;
}
}
