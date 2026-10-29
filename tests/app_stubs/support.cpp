#include "support.hpp"
#include "../../firmware/main/ir_service.hpp"
#include "../../firmware/main/proflame2_service.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace {
esp_matter::node_t node;
esp_matter::attribute_t label_attribute;
struct ButtonStopped {};

esp_err_t result(app_fake::Op operation, const char *name)
{
    app_fake::state.trace.emplace_back(name);
    const auto call = ++app_fake::state.calls[operation];
    const auto failure = app_fake::state.failures.find(operation);
    return failure != app_fake::state.failures.end() && failure->second.call == call
               ? failure->second.error : ESP_OK;
}

esp_matter::endpoint_t &by_id(std::uint16_t id)
{
    for (const auto &endpoint : app_fake::state.endpoints) if (endpoint->id == id) return *endpoint;
    assert(false && "Unknown endpoint");
    std::abort();
}

esp_matter::endpoint_t *create_endpoint(const char *kind, void *context)
{
    auto endpoint = std::make_unique<esp_matter::endpoint_t>();
    endpoint->id = static_cast<std::uint16_t>(app_fake::state.endpoints.size() + 1);
    endpoint->kind = kind;
    endpoint->context = context;
    auto *pointer = endpoint.get();
    app_fake::state.endpoints.push_back(std::move(endpoint));
    return pointer;
}
}

namespace app_fake {
State state;
void fail(Op operation, unsigned call, esp_err_t error) { state.failures[operation] = {call, error}; }
unsigned calls(Op operation) { return state.calls[operation]; }
bool logged(const std::string &fragment)
{
    return std::any_of(state.logs.begin(), state.logs.end(), [&](const auto &line) {
        return line.find(fragment) != std::string::npos;
    });
}
esp_matter::endpoint_t &endpoint(const char *label)
{
    for (const auto &entry : state.endpoints) if (entry->label == label) return *entry;
    assert(false && "Missing configured endpoint");
    std::abort();
}

// Model the documented PRE -> database write -> POST contract. A rejected PRE
// preserves the previous value and prevents POST, including dependent reports.
esp_err_t write(std::uint16_t id, std::uint32_t cluster, std::uint32_t attribute,
                 esp_matter_attr_val_t value)
{
    auto &entry = by_id(id);
    assert(state.attribute_callback);
    auto error = state.attribute_callback(esp_matter::attribute::PRE_UPDATE,
                                          id, cluster, attribute, &value, entry.context);
    if (error != ESP_OK) return error;
    state.values[{id, cluster, attribute}] = value;
    return state.attribute_callback(esp_matter::attribute::POST_UPDATE,
                                     id, cluster, attribute, &value, entry.context);
}

// Scheduled stack operations run only when explicitly dispatched by a test.
void run_scheduled()
{
    while (!state.scheduled.empty()) {
        const auto work = state.scheduled.front();
        state.scheduled.pop_front();
        work.first(work.second);
    }
}

// Feed a complete deterministic button timeline to the real infinite task.
// Stopping at vTaskDelay leaves no firmware lock or allocated task resource.
void run_button(const std::vector<int> &levels)
{
    assert(state.button_task && !levels.empty());
    state.button_levels = {levels.begin(), levels.end()};
    try { state.button_task(state.button_argument); }
    catch (const ButtonStopped &) { return; }
    assert(false && "Button task unexpectedly returned");
}
}

const char *esp_err_to_name(esp_err_t) { return "HOST_APP_ERROR"; }
void host_error_check(esp_err_t error) { if (error != ESP_OK) throw app_fake::FatalError{error}; }
void host_log(const char *level, const char *tag, const char *format, ...)
{
    char message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    app_fake::state.logs.push_back(std::string(level) + ":" + tag + ":" + message);
}
std::int64_t esp_timer_get_time() { return app_fake::state.now_us; }
esp_err_t gpio_config(const gpio_config_t *config)
{
    app_fake::state.gpio_configs.push_back(*config);
    return result(app_fake::Op::GpioConfig, "gpio_config");
}
esp_err_t gpio_set_level(gpio_num_t pin, std::uint32_t level)
{
    app_fake::state.pins[pin] = static_cast<int>(level);
    return ESP_OK;
}
int gpio_get_level(gpio_num_t pin)
{
    assert(pin == 14 && !app_fake::state.button_levels.empty());
    const int level = app_fake::state.button_levels.front();
    app_fake::state.button_levels.pop_front();
    return level;
}
BaseType_t xTaskCreate(TaskFunction_t task, const char *name, unsigned stack,
                      void *argument, unsigned priority, void *)
{
    assert(std::string(name) == "commission_button" && stack >= 3072 && priority > 0);
    if (result(app_fake::Op::Task, "button_task") != ESP_OK) return pdFALSE;
    app_fake::state.button_task = task;
    app_fake::state.button_argument = argument;
    return pdPASS;
}
void vTaskDelay(TickType_t ticks)
{
    assert(ticks == 20);
    ++app_fake::state.button_delays;
    app_fake::state.now_us += ticks * 1000;
    if (app_fake::state.button_levels.empty()) throw ButtonStopped{};
}
esp_err_t nvs_flash_init() { return result(app_fake::Op::NvsInit, "nvs_init"); }
esp_err_t nvs_flash_erase() { return result(app_fake::Op::Erase, "erase"); }
esp_err_t nvs_flash_erase_partition(const char *) { return nvs_flash_erase(); }
esp_err_t nvs_erase_all(nvs_handle_t) { return nvs_flash_erase(); }
esp_err_t start_terminal() { return result(app_fake::Op::Terminal, "terminal"); }

namespace proflame2_service {
esp_err_t init() { return result(app_fake::Op::X0aInit, "x0a_init"); }
bool busy() { return app_fake::state.x0a_busy; }
void quiesce() {
    app_fake::state.x0a_quiesced = true;
    result(app_fake::Op::X0aQuiesce, "x0a_quiesce");
}
esp_err_t prepare_reset() {
    if (busy()) return ESP_ERR_INVALID_STATE;
    quiesce();
    return ESP_OK;
}
esp_err_t refresh() { return result(app_fake::Op::X0aRequest, "x0a_refresh"); }
esp_err_t request(proflame2::Change) { return result(app_fake::Op::X0aRequest, "x0a_request"); }
}

namespace ir_service {
esp_err_t init() { return result(app_fake::Op::IrInit, "ir_init"); }
bool busy() { return app_fake::state.ir_busy; }
esp_err_t send(const char *name, unsigned zone, unsigned repeats, unsigned gap)
{
    app_fake::state.attempted.push_back({name, zone, repeats, gap});
    const auto error = result(app_fake::Op::IrSend, "ir_send");
    if (error == ESP_OK) app_fake::state.accepted.push_back({name, zone, repeats, gap});
    return error;
}
esp_err_t send_pending(unsigned zone, unsigned repeats, unsigned gap)
{
    return send("pending", zone, repeats, gap);
}
esp_err_t remove(const char *) { return result(app_fake::Op::Erase, "ir_remove"); }
}

esp_matter_attr_val_t esp_matter_bool(bool value)
{
    esp_matter_attr_val_t result{};
    result.type = ESP_MATTER_VAL_TYPE_BOOLEAN;
    result.val.b = value;
    return result;
}
esp_matter_attr_val_t esp_matter_uint8(std::uint8_t value)
{
    esp_matter_attr_val_t result{};
    result.type = ESP_MATTER_VAL_TYPE_UINT8;
    result.val.u8 = value;
    return result;
}
esp_matter_attr_val_t esp_matter_enum8(std::uint8_t value)
{
    auto result = esp_matter_uint8(value);
    result.type = ESP_MATTER_VAL_TYPE_ENUM8;
    return result;
}
esp_matter_attr_val_t esp_matter_nullable_uint8(std::optional<std::uint8_t> value)
{
    auto result = esp_matter_uint8(value.value_or(255));
    result.type = ESP_MATTER_VAL_TYPE_NULLABLE_UINT8;
    return result;
}

namespace esp_matter {
node_t *node::create(config_t *, attribute::callback_t attributes, identification::callback_t identify)
{
    if (result(app_fake::Op::Node, "node") != ESP_OK) return nullptr;
    app_fake::state.attribute_callback = attributes;
    app_fake::state.identify_callback = identify;
    return &::node;
}
endpoint_t *endpoint::aggregator::create(node_t *owner, config_t *, std::uint8_t flags, void *context)
{
    assert(owner == &::node && flags == ENDPOINT_FLAG_NONE && !context);
    return result(app_fake::Op::Aggregator, "aggregator") == ESP_OK ? create_endpoint("aggregator", context) : nullptr;
}
endpoint_t *endpoint::bridged_node::create(node_t *owner, config_t *, std::uint8_t flags, void *context)
{
    assert(owner == &::node && flags == ENDPOINT_FLAG_BRIDGE && context);
    return result(app_fake::Op::Bridge, "bridge") == ESP_OK ? create_endpoint("bridge", context) : nullptr;
}
esp_err_t endpoint::on_off_light::add(endpoint_t *entry, config_t *config)
{
    const auto error = result(app_fake::Op::Light, "light");
    if (error != ESP_OK) return error;
    entry->kind = "light";
    assert(config->on_off_lighting.start_up_on_off == nullptr);
    app_fake::state.values[{entry->id, 0x0006, 0}] = esp_matter_bool(config->on_off.on_off);
    return ESP_OK;
}
esp_err_t endpoint::fan::add(endpoint_t *entry, config_t *config)
{
    const auto error = result(app_fake::Op::Fan, "fan");
    if (error != ESP_OK) return error;
    entry->kind = "fan";
    assert(config->fan_control.fan_mode_sequence == 0);
    app_fake::state.values[{entry->id, 0x0202, 0}] = esp_matter_enum8(config->fan_control.fan_mode);
    app_fake::state.values[{entry->id, 0x0202, 2}] = esp_matter_nullable_uint8(config->fan_control.percent_setting);
    app_fake::state.values[{entry->id, 0x0202, 3}] = esp_matter_uint8(config->fan_control.percent_current);
    return ESP_OK;
}
esp_err_t endpoint::set_parent_endpoint(endpoint_t *entry, endpoint_t *parent)
{
    const auto error = result(app_fake::Op::Parent, "parent");
    if (error == ESP_OK) entry->parent = parent;
    return error;
}
std::uint16_t endpoint::get_id(endpoint_t *entry) { return entry->id; }
cluster_t *cluster::get(endpoint_t *entry, std::uint32_t cluster)
{
    assert(cluster == 0x0039);
    return &entry->information;
}
attribute_t *cluster::bridged_device_basic_information::attribute::create_node_label(
    cluster_t *cluster, char *label, std::uint16_t length)
{
    if (result(app_fake::Op::Label, "label") != ESP_OK) return nullptr;
    cluster->endpoint->label.assign(label, length);
    return &label_attribute;
}
esp_err_t attribute::update(std::uint16_t id, std::uint32_t cluster, std::uint32_t attribute,
                            esp_matter_attr_val_t *value)
{
    app_fake::state.reports.push_back({id, cluster, attribute, *value});
    const auto error = result(app_fake::Op::Report, "report");
    return error == ESP_OK ? app_fake::write(id, cluster, attribute, *value) : error;
}
esp_err_t start(event_callback_t callback)
{
    app_fake::state.event_callback = callback;
    if (app_fake::state.during_start) app_fake::state.during_start();
    return result(app_fake::Op::Start, "matter_start");
}
esp_err_t factory_reset() { return result(app_fake::Op::Reset, "factory_reset"); }
}

namespace chip {
DeviceLayer::PlatformManager &DeviceLayer::PlatformMgr() { static PlatformManager value; return value; }
DeviceLayer::ConnectivityManager &DeviceLayer::ConnectivityMgr() { static ConnectivityManager value; return value; }
int DeviceLayer::PlatformManager::ScheduleWork(Work work, intptr_t argument)
{
    const auto error = result(app_fake::Op::Schedule, "schedule");
    if (error == ESP_OK) app_fake::state.scheduled.emplace_back(work, argument);
    return error;
}
bool DeviceLayer::ConnectivityManager::IsWiFiStationProvisioned() { return app_fake::state.wifi; }
bool DeviceLayer::ConnectivityManager::IsBLEAdvertisingEnabled() { return app_fake::state.ble; }
Server &Server::GetInstance() { static Server value; return value; }
CommissioningWindowManager &Server::GetCommissioningWindowManager()
{
    static CommissioningWindowManager value;
    return value;
}
bool CommissioningWindowManager::IsCommissioningWindowOpen() { return app_fake::state.window_open; }
Dnssd::CommissioningMode CommissioningWindowManager::GetCommissioningMode() { return app_fake::state.mode; }
int CommissioningWindowManager::OpenBasicCommissioningWindow(System::Clock::Seconds32 duration,
                                                              CommissioningWindowAdvertisement advertisement)
{
    const auto error = result(app_fake::Op::Window, "window");
    app_fake::state.window_seconds = duration.count();
    app_fake::state.advertisement = advertisement;
    if (error == ESP_OK) {
        app_fake::state.window_open = true;
        app_fake::state.mode = Dnssd::CommissioningMode::kEnabledBasic;
    }
    return error;
}
}
void PrintOnboardingCodes(chip::RendezvousInformationFlags flags) { app_fake::state.onboarding.push_back(flags.bits); }
