#pragma once
#include "sdk.hpp"
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace esp_matter {
struct endpoint_t {
    std::uint16_t id;
    std::string kind, label;
    void *context;
    endpoint_t *parent = nullptr;
    cluster_t information{this};
};
}

namespace app_fake {
enum class Op {
    Node, Aggregator, Bridge, Light, Fan, Parent, Label, Start, Report,
    NvsInit, IrInit, GpioConfig, Terminal, Task, Schedule, Window, Reset, IrSend, Erase,
    X0aInit, X0aQuiesce, X0aRequest
};
constexpr esp_err_t kError = 0x7011;
struct FatalError { esp_err_t error; };
struct Send { std::string name; unsigned zone, repeats, gap; };
struct Update {
    std::uint16_t endpoint;
    std::uint32_t cluster, attribute;
    esp_matter_attr_val_t value;
};
using Key = std::tuple<std::uint16_t, std::uint32_t, std::uint32_t>;
struct State {
    std::map<Op, unsigned> calls;
    struct Failure { unsigned call; esp_err_t error; };
    std::map<Op, Failure> failures;
    std::vector<std::string> trace, logs;
    std::vector<std::unique_ptr<esp_matter::endpoint_t>> endpoints;
    esp_matter::attribute::callback_t attribute_callback = nullptr;
    esp_matter::identification::callback_t identify_callback = nullptr;
    esp_matter::event_callback_t event_callback = nullptr;
    std::function<void()> during_start;
    std::vector<Send> attempted, accepted;
    std::vector<Update> reports;
    std::map<Key, esp_matter_attr_val_t> values;
    std::map<int, int> pins;
    std::vector<gpio_config_t> gpio_configs;
    std::int64_t now_us = 1000000;
    bool ir_busy = false, wifi = false, ble = true, window_open = false;
    bool x0a_busy = false, x0a_quiesced = false;
    chip::Dnssd::CommissioningMode mode = chip::Dnssd::CommissioningMode::kDisabled;
    chip::CommissioningWindowAdvertisement advertisement = chip::CommissioningWindowAdvertisement::kAllSupported;
    unsigned window_seconds = 0;
    std::vector<unsigned> onboarding;
    std::deque<std::pair<chip::DeviceLayer::Work, intptr_t>> scheduled;
    TaskFunction_t button_task = nullptr;
    void *button_argument = nullptr;
    std::deque<int> button_levels;
    unsigned button_delays = 0;
};
extern State state;
void fail(Op operation, unsigned call = 1, esp_err_t error = kError);
unsigned calls(Op operation);
bool logged(const std::string &fragment);
esp_matter::endpoint_t &endpoint(const char *label);
esp_err_t write(std::uint16_t endpoint, std::uint32_t cluster, std::uint32_t attribute,
                 esp_matter_attr_val_t value);
void run_scheduled();
void run_button(const std::vector<int> &levels);
}
