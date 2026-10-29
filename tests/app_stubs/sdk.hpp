#pragma once
// Host projections of the ESP-IDF 5.4.1 / pinned ESP-Matter interfaces used by
// matter_app.cpp and app_main.cpp. These declarations do not model target ABI.
#include "../service_stubs/sdk.hpp"
#include <chrono>
#include <optional>

#define ESP_LOGW(...) host_log("W", __VA_ARGS__)
void host_error_check(esp_err_t error);
#define ESP_ERROR_CHECK(expression) host_error_check(expression)
constexpr int GPIO_MODE_INPUT = 1, GPIO_PULLUP_ENABLE = 1;
esp_err_t nvs_flash_init();
std::int64_t esp_timer_get_time();

enum esp_matter_val_type_t {
    ESP_MATTER_VAL_TYPE_BOOLEAN = 1, ESP_MATTER_VAL_TYPE_UINT8 = 8,
    ESP_MATTER_VAL_TYPE_ENUM8 = 15, ESP_MATTER_VAL_TYPE_NULLABLE_UINT8 = 136
};
union esp_matter_val_t {
    bool b; int i; float f;
    std::int8_t i8; std::uint8_t u8; std::int16_t i16; std::uint16_t u16;
    std::int32_t i32; std::uint32_t u32; std::int64_t i64; std::uint64_t u64;
    struct { std::uint8_t *b; std::uint16_t s, n, t; } a;
    void *p;
};
struct esp_matter_attr_val_t { esp_matter_val_type_t type; esp_matter_val_t val; };
esp_matter_attr_val_t esp_matter_bool(bool value);
esp_matter_attr_val_t esp_matter_enum8(std::uint8_t value);
esp_matter_attr_val_t esp_matter_uint8(std::uint8_t value);
esp_matter_attr_val_t esp_matter_nullable_uint8(std::optional<std::uint8_t> value);
constexpr int CHIP_NO_ERROR = 0;

namespace chip {
namespace app::Clusters {
namespace OnOff {
constexpr std::uint32_t Id = 0x0006;
namespace Attributes::OnOff { constexpr std::uint32_t Id = 0; }
}
namespace FanControl {
constexpr std::uint32_t Id = 0x0202;
namespace Attributes {
namespace FanMode { constexpr std::uint32_t Id = 0; }
namespace PercentSetting { constexpr std::uint32_t Id = 2; }
namespace PercentCurrent { constexpr std::uint32_t Id = 3; }
}
}
namespace BridgedDeviceBasicInformation { constexpr std::uint32_t Id = 0x0039; }
}
namespace DeviceLayer {
namespace DeviceEventType { constexpr int kCommissioningComplete = 1; }
struct ChipDeviceEvent { int Type; };
using Work = void (*)(intptr_t);
class PlatformManager { public: int ScheduleWork(Work work, intptr_t argument = 0); };
class ConnectivityManager {
public:
    bool IsWiFiStationProvisioned();
    bool IsBLEAdvertisingEnabled();
};
PlatformManager &PlatformMgr();
ConnectivityManager &ConnectivityMgr();
}
namespace System::Clock { using Seconds32 = std::chrono::duration<std::uint32_t>; }
namespace Dnssd { enum class CommissioningMode { kDisabled, kEnabledBasic, kEnabledEnhanced }; }
enum class CommissioningWindowAdvertisement { kDnssdOnly, kAllSupported };
enum class RendezvousInformationFlag { kBLE = 2, kOnNetwork = 4 };
struct RendezvousInformationFlags {
    unsigned bits = 0;
    void Set(RendezvousInformationFlag flag) { bits |= static_cast<unsigned>(flag); }
};
class CommissioningWindowManager {
public:
    bool IsCommissioningWindowOpen();
    Dnssd::CommissioningMode GetCommissioningMode();
    int OpenBasicCommissioningWindow(System::Clock::Seconds32 duration,
                                     CommissioningWindowAdvertisement advertisement);
};
class Server {
public:
    static Server &GetInstance();
    CommissioningWindowManager &GetCommissioningWindowManager();
};
}
void PrintOnboardingCodes(chip::RendezvousInformationFlags flags);

namespace esp_matter {
constexpr std::uint8_t ENDPOINT_FLAG_NONE = 0, ENDPOINT_FLAG_BRIDGE = 1;
struct node_t {};
struct endpoint_t;
struct cluster_t { endpoint_t *endpoint; };
struct attribute_t {};
namespace attribute {
enum callback_type_t { PRE_UPDATE, POST_UPDATE, READ, WRITE };
using callback_t = esp_err_t (*)(callback_type_t, std::uint16_t, std::uint32_t,
                                std::uint32_t, esp_matter_attr_val_t *, void *);
esp_err_t update(std::uint16_t endpoint, std::uint32_t cluster, std::uint32_t attribute,
                 esp_matter_attr_val_t *value);
}
namespace identification {
enum callback_type_t { START, STOP };
using callback_t = esp_err_t (*)(callback_type_t, std::uint16_t, std::uint8_t, std::uint8_t, void *);
}
namespace node {
struct config_t {};
node_t *create(config_t *, attribute::callback_t, identification::callback_t);
}
namespace endpoint {
namespace aggregator {
struct config_t {};
endpoint_t *create(node_t *, config_t *, std::uint8_t flags, void *context);
}
namespace bridged_node {
struct config_t {};
endpoint_t *create(node_t *, config_t *, std::uint8_t flags, void *context);
}
namespace on_off_light {
struct config_t {
    struct { bool on_off = false; } on_off;
    struct { std::nullptr_t start_up_on_off = nullptr; } on_off_lighting;
};
esp_err_t add(endpoint_t *, config_t *);
}
namespace fan {
struct config_t {
    struct {
        std::uint8_t fan_mode = 0, fan_mode_sequence = 0;
        std::optional<std::uint8_t> percent_setting;
        std::uint8_t percent_current = 0;
    } fan_control;
};
esp_err_t add(endpoint_t *, config_t *);
}
esp_err_t set_parent_endpoint(endpoint_t *, endpoint_t *);
std::uint16_t get_id(endpoint_t *);
}
namespace cluster {
cluster_t *get(endpoint_t *, std::uint32_t cluster);
namespace bridged_device_basic_information::attribute {
attribute_t *create_node_label(cluster_t *, char *label, std::uint16_t length);
}
}
using event_callback_t = void (*)(const chip::DeviceLayer::ChipDeviceEvent *, intptr_t);
esp_err_t start(event_callback_t callback);
esp_err_t factory_reset();
}
