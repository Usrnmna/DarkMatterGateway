#include "matter_app.hpp"

#include <atomic>
#include <cstring>
#include "board_config.hpp"
#include "ir_service.hpp"
#include "proflame2_service.hpp"
#include "profiles.hpp"
#include "esp_log.h"
#include "esp_matter.h"
#include "esp_timer.h"
#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/OnboardingCodesUtil.h>

namespace matter_app {
namespace {
using namespace esp_matter;
using namespace chip::app::Clusters;
constexpr const char *kTag = "matter_ir";

struct Context {
    const ir::Profile *profile = nullptr;
    bool synchronizing = false;
    const char *last_command = nullptr;
    std::int64_t last_queued_us = 0;
};
Context contexts[8];
std::atomic<bool> ready{false};

// Dependent fan attributes describe the selected discrete speed, not measured
// motion. The guard prevents reporting changes from causing additional IR jobs.
void synchronize_fan(Context &context, std::uint16_t endpoint_id, unsigned percent)
{
    unsigned mode = percent == 0 ? 0 : percent <= 33 ? 1 : percent <= 66 ? 2 : 3;
    unsigned selected = ir::fan_mode_percent(mode);
    context.synchronizing = true;
    auto mode_value = esp_matter_enum8(mode);
    auto setting_value = esp_matter_nullable_uint8(static_cast<std::uint8_t>(selected));
    auto current_value = esp_matter_uint8(selected);
    const esp_err_t errors[] = {
        attribute::update(endpoint_id, FanControl::Id, FanControl::Attributes::FanMode::Id, &mode_value),
        attribute::update(endpoint_id, FanControl::Id, FanControl::Attributes::PercentSetting::Id, &setting_value),
        attribute::update(endpoint_id, FanControl::Id, FanControl::Attributes::PercentCurrent::Id, &current_value)
    };
    context.synchronizing = false;
    for (auto error : errors) {
        if (error != ESP_OK) ESP_LOGE(kTag, "Fan state reporting failed: %s", esp_err_to_name(error));
    }
}

// Matter requests enqueue immutable command copies. Queue acceptance changes
// requested state; transmission completion and appliance response are separate.
esp_err_t attribute_changed(attribute::callback_type_t type, std::uint16_t endpoint_id,
                            std::uint32_t cluster_id, std::uint32_t attribute_id,
                            esp_matter_attr_val_t *value, void *private_data)
{
    auto *context = static_cast<Context *>(private_data);
    if (!ready || !context || !context->profile || context->synchronizing) return ESP_OK;
    const auto &profile = *context->profile;
    const char *command = nullptr;
    unsigned percent = 0;
    if (profile.kind == ir::Kind::Light && cluster_id == OnOff::Id &&
        attribute_id == OnOff::Attributes::OnOff::Id) {
        command = value->val.b ? profile.on : profile.off;
    } else if (profile.kind == ir::Kind::Fan && cluster_id == FanControl::Id) {
        if (attribute_id == FanControl::Attributes::FanMode::Id) {
            percent = ir::fan_mode_percent(value->val.u8);
        } else if (attribute_id == FanControl::Attributes::PercentSetting::Id) {
            percent = value->val.u8; // Encoded nullable null (255) is rejected below.
        } else {
            return ESP_OK;
        }
        command = ir::fan_command(profile, percent);
    } else {
        return ESP_OK;
    }
    if (!command || !*command) return ESP_ERR_INVALID_ARG;
    if (type == attribute::PRE_UPDATE) {
        const auto now = esp_timer_get_time();
        // CHIP may translate one fan Off operation into nested percent updates.
        if (context->last_command && std::strcmp(context->last_command, command) == 0 &&
            now - context->last_queued_us < 100000) return ESP_OK;
        const auto error = ir_service::send(command, profile.zone);
        if (error != ESP_OK) return error;
        context->last_command = command;
        context->last_queued_us = now;
    } else if (type == attribute::POST_UPDATE && profile.kind == ir::Kind::Fan) {
        synchronize_fan(*context, endpoint_id, percent);
    }
    return ESP_OK;
}

esp_err_t identify(identification::callback_type_t type, std::uint16_t,
                   std::uint8_t, std::uint8_t, void *)
{
    // Identification operates the node indicator and never sends appliance IR.
    gpio_set_level(board::kStatusLed, type == identification::START ? 1 : 0);
    return ESP_OK;
}

void event_received(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t)
{
    if (event->Type == chip::DeviceLayer::DeviceEventType::kCommissioningComplete)
        ESP_LOGI(kTag, "Matter commissioning completed");
}

void commission_work(intptr_t)
{
    auto &manager = chip::Server::GetInstance().GetCommissioningWindowManager();
    const bool provisioned = chip::DeviceLayer::ConnectivityMgr().IsWiFiStationProvisioned();
    if (!manager.IsCommissioningWindowOpen()) {
        // BLE is released after initial commissioning by the selected SDK.
        // A configured Wi-Fi node therefore reopens discovery over its network.
        const auto advertisement = provisioned ? chip::CommissioningWindowAdvertisement::kDnssdOnly
                                              : chip::CommissioningWindowAdvertisement::kAllSupported;
        auto error = manager.OpenBasicCommissioningWindow(chip::System::Clock::Seconds32(300), advertisement);
        if (error != CHIP_NO_ERROR) {
            ESP_LOGE(kTag, "Cannot open commissioning window");
            return;
        }
    }
    if (manager.GetCommissioningMode() != chip::Dnssd::CommissioningMode::kEnabledBasic) {
        ESP_LOGI(kTag, "Enhanced window already open; use its initiating controller's pairing code");
        return;
    }
    chip::RendezvousInformationFlags rendezvous;
    if (provisioned) rendezvous.Set(chip::RendezvousInformationFlag::kOnNetwork);
    if (chip::DeviceLayer::ConnectivityMgr().IsBLEAdvertisingEnabled())
        rendezvous.Set(chip::RendezvousInformationFlag::kBLE);
    ::PrintOnboardingCodes(rendezvous);
}

void reset_work(intptr_t)
{
    // The dedicated ir_store partition is not part of Matter fabric storage.
    const auto error = esp_matter::factory_reset();
    if (error != ESP_OK) ESP_LOGE(kTag, "Matter reset failed: %s", esp_err_to_name(error));
}
} // namespace

esp_err_t init()
{
    node::config_t node_config;
    auto *node = node::create(&node_config, attribute_changed, identify);
    if (!node) return ESP_ERR_NO_MEM;
    endpoint::aggregator::config_t aggregator_config;
    auto *aggregator = endpoint::aggregator::create(node, &aggregator_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!aggregator) return ESP_ERR_NO_MEM;
    unsigned index = 0;
    for (const auto &profile : configured::profiles) {
        if (!profile.enabled || (profile.kind != ir::Kind::Light && profile.kind != ir::Kind::Fan)) continue;
        auto &context = contexts[index++];
        context.profile = &profile;
        endpoint::bridged_node::config_t bridge_config;
        auto *endpoint = endpoint::bridged_node::create(node, &bridge_config, ENDPOINT_FLAG_BRIDGE, &context);
        if (!endpoint) return ESP_ERR_NO_MEM;
        esp_err_t error;
        if (profile.kind == ir::Kind::Light) {
            endpoint::on_off_light::config_t config;
            config.on_off.on_off = false;
            config.on_off_lighting.start_up_on_off = nullptr;
            error = endpoint::on_off_light::add(endpoint, &config);
        } else {
            endpoint::fan::config_t config;
            config.fan_control.fan_mode = 0;
            config.fan_control.fan_mode_sequence = 0;
            config.fan_control.percent_setting = std::uint8_t{0};
            config.fan_control.percent_current = 0;
            error = endpoint::fan::add(endpoint, &config);
        }
        if (error != ESP_OK) return error;
        error = endpoint::set_parent_endpoint(endpoint, aggregator);
        if (error != ESP_OK) return error;
        auto *information = cluster::get(endpoint, BridgedDeviceBasicInformation::Id);
        if (!cluster::bridged_device_basic_information::attribute::create_node_label(
                information, const_cast<char *>(profile.id), std::strlen(profile.id))) return ESP_ERR_NO_MEM;
        ESP_LOGI(kTag, "Profile %s uses endpoint %u", profile.id, endpoint::get_id(endpoint));
    }
    auto error = esp_matter::start(event_received);
    if (error == ESP_OK) ready = true;
    return error;
}

esp_err_t open_commissioning()
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    return chip::DeviceLayer::PlatformMgr().ScheduleWork(commission_work) == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
}

esp_err_t reset_fabrics()
{
    if (!ready || ir_service::busy()) return ESP_ERR_INVALID_STATE;
    // Check and quiesce atomically: no X0A request can slip between an idle
    // check and reset scheduling. A scheduling failure leaves X0A inhibited.
    if (proflame2_service::prepare_reset() != ESP_OK) return ESP_ERR_INVALID_STATE;
    return chip::DeviceLayer::PlatformMgr().ScheduleWork(reset_work) == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
}
} // namespace matter_app
