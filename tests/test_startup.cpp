#include "app_fixture.hpp"
#include "../firmware/main/app_main.cpp"

namespace {
using namespace app_test;

void startup_order_and_quiet_outputs()
{
    Fixture fixture;
    app_main();
    const auto &trace = app_fake::state.trace;
    assert(trace.size() >= 7);
    assert((std::vector<std::string>(trace.begin(), trace.begin() + 6) ==
            std::vector<std::string>{"nvs_init", "ir_init", "x0a_init", "gpio_config", "gpio_config", "node"}));
    assert((std::vector<std::string>(trace.end() - 3, trace.end()) ==
            std::vector<std::string>{"matter_start", "terminal", "button_task"}));
    assert(app_fake::state.gpio_configs.size() == 2);
    const auto &button = app_fake::state.gpio_configs[0];
    const auto &indicator = app_fake::state.gpio_configs[1];
    assert(button.pin_bit_mask == (1ULL << 14) && button.mode == GPIO_MODE_INPUT && button.pull_up_en == GPIO_PULLUP_ENABLE);
    assert(indicator.pin_bit_mask == (1ULL << 21) && indicator.mode == GPIO_MODE_OUTPUT);
    assert(app_fake::state.pins.at(21) == 0 && app_fake::state.button_task);
    assert(app_fake::state.scheduled.empty());
    assert_no_replay_or_erase();
}

// ESP_ERROR_CHECK is represented by a caught fatal error, preserving its
// stop-on-first-error control flow without terminating the whole test binary.
void startup_stops_on_initialization_failure()
{
    struct Row { Op operation; unsigned call; esp_err_t error; const char *last; };
    const Row rows[] = {
        {Op::NvsInit, 1, ESP_ERR_NVS_NO_FREE_PAGES, "nvs_init"},
        {Op::IrInit, 1, app_fake::kError, "ir_init"},
        {Op::GpioConfig, 1, app_fake::kError, "gpio_config"},
        {Op::GpioConfig, 2, app_fake::kError, "gpio_config"},
        {Op::Start, 1, app_fake::kError, "matter_start"},
        {Op::Terminal, 1, app_fake::kError, "terminal"},
    };
    for (const auto &row : rows) {
        Fixture fixture;
        app_fake::fail(row.operation, row.call, row.error);
        bool stopped = false;
        try { app_main(); }
        catch (const app_fake::FatalError &error) { stopped = true; assert(error.error == row.error); }
        assert(stopped && app_fake::state.trace.back() == row.last);
        assert(!app_fake::state.button_task && app_fake::calls(Op::Task) == 0);
        assert(app_fake::state.scheduled.empty());
        assert_no_replay_or_erase();
    }
}

void button_task_failure_preserves_console_recovery()
{
    Fixture fixture;
    app_fake::fail(Op::Task);
    app_main();
    assert(!app_fake::state.button_task && app_fake::calls(Op::Terminal) == 1);
    assert(app_fake::logged("button task unavailable; use the console command"));
    assert(matter_app::open_commissioning() == ESP_OK);
    assert(app_fake::state.scheduled.size() == 1 && app_fake::calls(Op::Window) == 0);
    app_fake::run_scheduled();
    assert(app_fake::calls(Op::Window) == 1);
    assert_no_replay_or_erase();
}

void commissioning_advertisement_and_schedule_failures()
{
    for (bool wifi : {false, true}) {
        for (bool ble : {false, true}) {
            Fixture fixture;
            assert(matter_app::init() == ESP_OK);
            app_fake::state.wifi = wifi;
            app_fake::state.ble = ble;
            app_fake::fail(Op::Schedule);
            assert(matter_app::open_commissioning() == ESP_FAIL);
            assert(app_fake::state.scheduled.empty() && app_fake::calls(Op::Window) == 0);
            assert(matter_app::open_commissioning() == ESP_OK);
            assert(app_fake::state.scheduled.size() == 1 && app_fake::state.onboarding.empty());
            app_fake::run_scheduled();
            assert(app_fake::state.window_seconds == 300);
            assert(app_fake::state.advertisement == (wifi ? chip::CommissioningWindowAdvertisement::kDnssdOnly
                                                          : chip::CommissioningWindowAdvertisement::kAllSupported));
            assert((app_fake::state.onboarding == std::vector<unsigned>{(wifi ? 4U : 0U) | (ble ? 2U : 0U)}));
            assert_no_replay_or_erase();
        }
    }
}

void existing_windows_and_window_failure()
{
    for (bool enhanced : {false, true}) {
        Fixture fixture;
        assert(matter_app::init() == ESP_OK);
        app_fake::state.window_open = true;
        app_fake::state.window_seconds = 73;
        app_fake::state.mode = enhanced ? chip::Dnssd::CommissioningMode::kEnabledEnhanced
                                        : chip::Dnssd::CommissioningMode::kEnabledBasic;
        assert(matter_app::open_commissioning() == ESP_OK);
        app_fake::run_scheduled();
        assert(app_fake::calls(Op::Window) == 0 && app_fake::state.window_seconds == 73);
        assert(app_fake::state.onboarding.size() == (enhanced ? 0U : 1U));
        if (enhanced) assert(app_fake::logged("Enhanced window already open"));
        assert_no_replay_or_erase();
    }
    Fixture fixture;
    assert(matter_app::init() == ESP_OK);
    app_fake::fail(Op::Window);
    assert(matter_app::open_commissioning() == ESP_OK);
    app_fake::run_scheduled();
    assert(!app_fake::state.window_open && app_fake::state.onboarding.empty());
    assert(app_fake::logged("Cannot open commissioning window"));
    assert(app_fake::state.scheduled.empty());
    assert_no_replay_or_erase();
}

void reset_gating_scheduling_and_failure()
{
    Fixture fixture;
    assert(matter_app::reset_fabrics() == ESP_ERR_INVALID_STATE);
    assert(matter_app::init() == ESP_OK);
    app_fake::state.ir_busy = true;
    assert(matter_app::reset_fabrics() == ESP_ERR_INVALID_STATE);
    assert(app_fake::calls(Op::Schedule) == 0);
    app_fake::state.ir_busy = false;
    app_fake::state.x0a_busy = true;
    assert(matter_app::reset_fabrics() == ESP_ERR_INVALID_STATE);
    assert(!app_fake::state.x0a_quiesced && app_fake::calls(Op::Schedule) == 0);
    app_fake::state.x0a_busy = false;
    app_fake::fail(Op::Schedule);
    assert(matter_app::reset_fabrics() == ESP_FAIL);
    assert(app_fake::state.x0a_quiesced);
    assert(app_fake::state.trace[app_fake::state.trace.size()-2] == "x0a_quiesce");
    assert(app_fake::state.scheduled.empty());
    app_fake::fail(Op::Reset);
    assert(matter_app::reset_fabrics() == ESP_OK);
    assert(app_fake::calls(Op::Reset) == 0);
    app_fake::run_scheduled();
    assert(app_fake::calls(Op::Reset) == 1 && app_fake::logged("Matter reset failed:"));
    assert(app_fake::state.scheduled.empty());
    assert(matter_app::reset_fabrics() == ESP_OK);
    app_fake::run_scheduled();
    assert(app_fake::calls(Op::Reset) == 2);
    assert_no_replay_or_erase();
}

void button_debounce_hold_and_rearm()
{
    Fixture fixture;
    app_main();
    // Short press, release, long hold, release, second qualifying press.
    std::vector<int> levels(49, 0);
    levels.push_back(1);
    levels.insert(levels.end(), 160, 0);
    levels.push_back(1);
    levels.insert(levels.end(), 50, 0);
    app_fake::run_button(levels);
    assert(app_fake::state.button_delays == levels.size());
    assert(app_fake::state.scheduled.size() == 2 && app_fake::calls(Op::Schedule) == 2);
    assert(app_fake::calls(Op::Window) == 0);
    app_fake::run_scheduled();
    assert(app_fake::calls(Op::Window) == 1); // Second request retains the existing window.
    assert_no_replay_or_erase();
}

void optional_x0a_failure_preserves_other_services()
{
    Fixture fixture;
    app_fake::fail(Op::X0aInit);
    app_main();
    assert(app_fake::calls(Op::Terminal) == 1 && app_fake::state.button_task);
    assert(app_fake::logged("X0A unavailable"));
    assert_no_replay_or_erase();
}

void button_failure_waits_for_release()
{
    Fixture fixture;
    app_main();
    app_fake::fail(Op::Schedule);
    std::vector<int> levels(150, 0);
    levels.push_back(1);
    levels.insert(levels.end(), 50, 0);
    app_fake::run_button(levels);
    assert(app_fake::calls(Op::Schedule) == 2 && app_fake::state.scheduled.size() == 1);
    assert(app_fake::logged("Commissioning unavailable:"));
    assert_no_replay_or_erase();
}
}

int main(int argc, char **argv)
{
    return app_test::run_cases(argc, argv, {
        {"startup_order_and_quiet_outputs", startup_order_and_quiet_outputs},
        {"optional_x0a_failure_preserves_other_services", optional_x0a_failure_preserves_other_services},
        {"startup_stops_on_initialization_failure", startup_stops_on_initialization_failure},
        {"button_task_failure_preserves_console_recovery", button_task_failure_preserves_console_recovery},
        {"commissioning_advertisement_and_schedule_failures", commissioning_advertisement_and_schedule_failures},
        {"existing_windows_and_window_failure", existing_windows_and_window_failure},
        {"reset_gating_scheduling_and_failure", reset_gating_scheduling_and_failure},
        {"button_debounce_hold_and_rearm", button_debounce_hold_and_rearm},
        {"button_failure_waits_for_release", button_failure_waits_for_release},
    });
}
