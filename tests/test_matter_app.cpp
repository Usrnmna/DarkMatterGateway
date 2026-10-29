#include "app_fixture.hpp"
#include <set>

namespace {
using namespace app_test;

// Startup callbacks must not replay IR, and only enabled native kinds get endpoints.
void topology_and_startup_suppression()
{
    Fixture fixture;
    assert(matter_app::open_commissioning() == ESP_ERR_INVALID_STATE);
    assert(matter_app::reset_fabrics() == ESP_ERR_INVALID_STATE);
    app_fake::state.during_start = [] {
        auto &light = app_fake::endpoint("light");
        auto value = esp_matter_bool(true);
        assert(app_fake::state.attribute_callback(esp_matter::attribute::PRE_UPDATE,
            light.id, 6, 0, &value, light.context) == ESP_OK);
    };
    assert(matter_app::init() == ESP_OK);
    assert(app_fake::state.endpoints.size() == 3);
    auto &aggregator = *app_fake::state.endpoints[0];
    auto &light = app_fake::endpoint("light");
    auto &fan = app_fake::endpoint("fan");
    assert(aggregator.kind == "aggregator" && !aggregator.parent);
    assert(light.kind == "light" && fan.kind == "fan");
    assert(light.parent == &aggregator && fan.parent == &aggregator);
    assert(light.context && fan.context && light.context != fan.context);
    assert(!app_fake::state.values.at({light.id, 6, 0}).val.b);
    assert_fan_state(0, 0);
    assert_no_replay_or_erase();
}

void disabled_and_maximum_profiles()
{
    {
        Fixture fixture;
        for (auto &profile : configured::profiles) profile.enabled = false;
        assert(matter_app::init() == ESP_OK);
        assert(app_fake::state.endpoints.size() == 1 && app_fake::calls(Op::Start) == 1);
        assert_no_replay_or_erase();
    }
    Fixture fixture;
    const char *names[] = {"l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7"};
    for (unsigned i = 0; i < 8; ++i)
        configured::profiles[i] = {names[i], ir::Kind::Light, true, 2, "on", "off", "", "", ""};
    assert(matter_app::init() == ESP_OK);
    assert(app_fake::state.endpoints.size() == 9);
    std::set<void *> contexts;
    for (const auto *name : names) {
        auto &entry = app_fake::endpoint(name);
        contexts.insert(entry.context);
        assert(app_fake::write(entry.id, 6, 0, esp_matter_bool(true)) == ESP_OK);
    }
    assert(contexts.size() == 8 && app_fake::state.accepted.size() == 8);
}

void initialization_failure_propagation()
{
    for (auto operation : {Op::Node, Op::Aggregator, Op::Bridge, Op::Light, Op::Fan,
                           Op::Parent, Op::Label, Op::Start}) {
        Fixture fixture;
        app_fake::fail(operation);
        const auto expected = operation == Op::Node || operation == Op::Aggregator ||
                              operation == Op::Bridge || operation == Op::Label ? ESP_ERR_NO_MEM : app_fake::kError;
        assert(matter_app::init() == expected);
        assert(matter_app::open_commissioning() == ESP_ERR_INVALID_STATE);
        assert(matter_app::reset_fabrics() == ESP_ERR_INVALID_STATE);
        assert(app_fake::state.scheduled.empty());
        assert(app_fake::calls(Op::Start) == (operation == Op::Start ? 1U : 0U));
        assert_no_replay_or_erase();
    }
}

void light_mapping_and_missing_command()
{
    Fixture fixture;
    assert(matter_app::init() == ESP_OK);
    auto &light = app_fake::endpoint("light");
    assert(app_fake::write(light.id, 6, 0, esp_matter_bool(true)) == ESP_OK);
    assert(app_fake::write(light.id, 6, 0, esp_matter_bool(false)) == ESP_OK);
    assert(app_fake::state.accepted.size() == 2);
    assert(app_fake::state.accepted[0].name == "lamp_on" && app_fake::state.accepted[1].name == "lamp_off");
    for (const auto &job : app_fake::state.accepted) assert(job.zone == 1 && job.repeats == 1 && job.gap == 40);
    configured::profiles[0].on = "";
    assert(app_fake::write(light.id, 6, 0, esp_matter_bool(true)) == ESP_ERR_INVALID_ARG);
    assert(!app_fake::state.values.at({light.id, 6, 0}).val.b);
    assert(app_fake::state.attempted.size() == 2 && app_fake::state.reports.empty());
}

void fan_percentage_boundaries()
{
    Fixture fixture;
    assert(matter_app::init() == ESP_OK);
    struct Row { unsigned input, mode, percent; const char *command; };
    const Row rows[] = {{0,0,0,"fan_off"}, {1,1,33,"fan_low"}, {33,1,33,"fan_low"},
                        {34,2,66,"fan_mid"}, {66,2,66,"fan_mid"}, {67,3,100,"fan_high"},
                        {100,3,100,"fan_high"}};
    unsigned requests = 0;
    for (const auto &row : rows) {
        app_fake::state.now_us += 100000;
        assert(fan_write(2, row.input) == ESP_OK);
        assert(app_fake::state.accepted.size() == ++requests);
        assert(app_fake::state.accepted.back().name == row.command && app_fake::state.accepted.back().zone == 2);
        assert_fan_state(row.mode, row.percent);
        assert(app_fake::state.reports.size() == requests * 3);
    }
}

void fan_modes_and_null_rejection()
{
    Fixture fixture;
    assert(matter_app::init() == ESP_OK);
    const char *commands[] = {"fan_off", "fan_low", "fan_mid", "fan_high", "fan_high", nullptr, "fan_high"};
    const unsigned modes[] = {0, 1, 2, 3, 3, 0, 3};
    const unsigned percentages[] = {0, 33, 66, 100, 100, 0, 100};
    for (unsigned mode = 0; mode < 7; ++mode) {
        app_fake::state.now_us += 100000;
        const auto before = app_fake::state.accepted.size();
        assert(fan_write(0, mode) == (commands[mode] ? ESP_OK : ESP_ERR_INVALID_ARG));
        if (commands[mode]) {
            assert(app_fake::state.accepted.size() == before + 1);
            assert(app_fake::state.accepted.back().name == commands[mode]);
            assert_fan_state(modes[mode], percentages[mode]);
        } else assert(app_fake::state.accepted.size() == before);
    }
    const auto calls = app_fake::state.attempted.size();
    const auto reports = app_fake::state.reports.size();
    for (unsigned mode : {7U, 255U}) assert(fan_write(0, mode) == ESP_ERR_INVALID_ARG);
    for (unsigned percent : {101U, 255U}) assert(fan_write(2, percent) == ESP_ERR_INVALID_ARG);
    auto &fan = app_fake::endpoint("fan");
    assert(app_fake::write(fan.id, 0x0202, 2, esp_matter_nullable_uint8(std::nullopt)) == ESP_ERR_INVALID_ARG);
    assert(app_fake::state.attempted.size() == calls && app_fake::state.reports.size() == reports);
    assert_fan_state(3, 100);
}

void fan_deduplication_and_endpoint_isolation()
{
    Fixture fixture;
    configured::profiles[2] = {"fan2", ir::Kind::Fan, true, 0, "", "fan_off", "fan_low", "fan_mid", "fan_high"};
    assert(matter_app::init() == ESP_OK);
    assert(fan_write(0, 0) == ESP_OK);
    assert(fan_write(2, 0) == ESP_OK);
    app_fake::state.now_us += 99999;
    assert(fan_write(0, 0) == ESP_OK);
    assert(app_fake::state.accepted.size() == 1);
    ++app_fake::state.now_us;
    assert(fan_write(2, 0) == ESP_OK);
    assert(app_fake::state.accepted.size() == 2);
    assert(fan_write(0, 1) == ESP_OK); // A different action is not deduplicated.
    auto &fan2 = app_fake::endpoint("fan2");
    assert(app_fake::write(fan2.id, 0x0202, 0, esp_matter_enum8(1)) == ESP_OK);
    assert(app_fake::state.accepted.size() == 4);
    assert(app_fake::state.accepted.back().name == "fan_low" && app_fake::state.accepted.back().zone == 0);
}

void queue_rejection_preserves_state_and_retry()
{
    for (auto error : {ESP_ERR_INVALID_STATE, ESP_ERR_NVS_NOT_FOUND, ESP_ERR_NO_MEM, ESP_ERR_TIMEOUT}) {
        Fixture fixture;
        assert(matter_app::init() == ESP_OK);
        app_fake::fail(Op::IrSend, 1, error);
        assert(fan_write(0, 3) == error);
        assert(app_fake::state.accepted.empty() && app_fake::state.reports.empty());
        assert_fan_state(0, 0);
        // A rejected enqueue must not populate the duplicate-suppression cache.
        assert(fan_write(0, 3) == ESP_OK);
        assert(app_fake::state.attempted.size() == 2 && app_fake::state.accepted.size() == 1);
        assert_fan_state(3, 100);
    }
}

void reporting_failure_releases_recursion_guard()
{
    for (unsigned call : {1U, 2U, 3U}) {
        Fixture fixture;
        assert(matter_app::init() == ESP_OK);
        app_fake::fail(Op::Report, call);
        assert(fan_write(0, 3) == ESP_OK);
        assert(app_fake::state.accepted.size() == 1 && app_fake::state.reports.size() == 3);
        assert(app_fake::logged("Fan state reporting failed:"));
        assert(fan_write(0, 1) == ESP_OK);
        assert(app_fake::state.accepted.size() == 2);
        assert_fan_state(1, 33);
    }
}

void unrelated_callbacks_and_identification()
{
    Fixture fixture;
    assert(matter_app::init() == ESP_OK);
    auto &light = app_fake::endpoint("light");
    auto &fan = app_fake::endpoint("fan");
    auto value = esp_matter_uint8(0);
    auto callback = app_fake::state.attribute_callback;
    assert(callback(esp_matter::attribute::PRE_UPDATE, 0, 6, 0, nullptr, nullptr) == ESP_OK);
    assert(callback(esp_matter::attribute::PRE_UPDATE, fan.id, 0x0202, 3, &value, fan.context) == ESP_OK);
    assert(callback(esp_matter::attribute::PRE_UPDATE, light.id, 999, 0, &value, light.context) == ESP_OK);
    value = esp_matter_bool(true);
    assert(callback(esp_matter::attribute::READ, light.id, 6, 0, &value, light.context) == ESP_OK);
    assert(callback(esp_matter::attribute::WRITE, light.id, 6, 0, &value, light.context) == ESP_OK);
    assert(app_fake::state.identify_callback(esp_matter::identification::START, light.id, 0, 0, light.context) == ESP_OK);
    assert(app_fake::state.pins.at(21) == 1);
    assert(app_fake::state.identify_callback(esp_matter::identification::STOP, light.id, 0, 0, light.context) == ESP_OK);
    assert(app_fake::state.pins.at(21) == 0);
    chip::DeviceLayer::ChipDeviceEvent event{chip::DeviceLayer::DeviceEventType::kCommissioningComplete};
    app_fake::state.event_callback(&event, 0);
    assert(app_fake::logged("Matter commissioning completed"));
    assert_no_replay_or_erase();
}
}

int main(int argc, char **argv)
{
    return app_test::run_cases(argc, argv, {
        {"topology_and_startup_suppression", topology_and_startup_suppression},
        {"disabled_and_maximum_profiles", disabled_and_maximum_profiles},
        {"initialization_failure_propagation", initialization_failure_propagation},
        {"light_mapping_and_missing_command", light_mapping_and_missing_command},
        {"fan_percentage_boundaries", fan_percentage_boundaries},
        {"fan_modes_and_null_rejection", fan_modes_and_null_rejection},
        {"fan_deduplication_and_endpoint_isolation", fan_deduplication_and_endpoint_isolation},
        {"queue_rejection_preserves_state_and_retry", queue_rejection_preserves_state_and_retry},
        {"reporting_failure_releases_recursion_guard", reporting_failure_releases_recursion_guard},
        {"unrelated_callbacks_and_identification", unrelated_callbacks_and_identification},
    });
}
