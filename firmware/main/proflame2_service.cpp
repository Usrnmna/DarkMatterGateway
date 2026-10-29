#include "proflame2_service.hpp"
#include "proflame2_session.hpp"
#include "proflame2_config.hpp"
#include "board_config.hpp"
#include "driver/uart.h"
#include "hal/uart_ll.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstdio>

namespace proflame2_service {
namespace {
using namespace proflame2;
constexpr int kBaud = 9600; // Source-reported setting; not target qualification.
constexpr int kEventQueueSize = 16;

std::uint64_t now_ms() { return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000; }

bool available_pin(int pin)
{
    // Preserve IR, expansion, console, USB, strapping and N16R8 memory roles.
    constexpr int reserved[] = {0,3,4,5,6,7,8,9,10,11,12,13,14,18,19,20,21,43,44,45,46};
    if (pin < 0 || pin > 48 || (pin >= 26 && pin <= 37)) return false;
    for (int occupied : reserved) if (pin == occupied) return false;
    return true;
}

// The physical gate must be disabled independently through reset and power loss.
// Software GPIO setup alone does not establish that property.
class UartTransport final : public Transport {
public:
    bool qualified() const override
    {
        if (!board::kX0aHardwareQualified || !proflame2_config::kQualifiedForControl ||
            board::kX0aUart <= 0 || board::kX0aUart >= UART_NUM_MAX ||
            !available_pin(board::kX0aTx) || !available_pin(board::kX0aRx) || !available_pin(board::kX0aTxInhibit) ||
            board::kX0aTx == board::kX0aRx || board::kX0aTx == board::kX0aTxInhibit ||
            board::kX0aRx == board::kX0aTxInhibit ||
            (board::kX0aInhibitLevel != 0 && board::kX0aInhibitLevel != 1)) return false;
#if defined(CONFIG_ESP_CONSOLE_UART_NUM)
        if (board::kX0aUart == CONFIG_ESP_CONSOLE_UART_NUM) return false;
#endif
        return GPIO_IS_VALID_OUTPUT_GPIO(board::kX0aTx) && GPIO_IS_VALID_GPIO(board::kX0aRx) &&
               GPIO_IS_VALID_OUTPUT_GPIO(board::kX0aTxInhibit);
    }

    bool open() override
    {
        if (!qualified() || installed_) return false;
        const auto gate = static_cast<gpio_num_t>(board::kX0aTxInhibit);
        if (gpio_set_level(gate, board::kX0aInhibitLevel) != ESP_OK) return false;
        gpio_config_t output{};
        // qualified() has checked the range before this shift.
        const unsigned gate_index = static_cast<unsigned>(board::kX0aTxInhibit);
        output.pin_bit_mask = std::uint64_t{1} << (gate_index & 63U);
        output.mode = GPIO_MODE_OUTPUT;
        if (gpio_config(&output) != ESP_OK) return false;
        gate_ready_ = true;
        if (uart_driver_install(port(), 256, 0, kEventQueueSize, &events_, 0) != ESP_OK) return false;
        installed_ = true;
        uart_config_t config{};
        config.baud_rate = kBaud;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.source_clk = UART_SCLK_DEFAULT;
        // ESP-IDF 5.4.1's default mask omits framing errors. Request the event
        // explicitly so an invalid stop bit cannot pass as a valid XOR frame.
        if (uart_param_config(port(), &config) != ESP_OK ||
            uart_enable_intr_mask(port(), UART_INTR_FRAM_ERR) != ESP_OK ||
            uart_set_pin(port(), board::kX0aTx, board::kX0aRx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
            close();
            return false;
        }
        return true;
    }

    bool inhibit(bool disabled) override
    {
        return gate_ready_ && gpio_set_level(static_cast<gpio_num_t>(board::kX0aTxInhibit),
            disabled ? board::kX0aInhibitLevel : 1 - board::kX0aInhibitLevel) == ESP_OK;
    }

    int read(std::uint8_t *bytes, std::size_t capacity) override
    {
        if (!installed_) return -1;
        uart_event_t event{};
        unsigned drained = 0;
        while (xQueueReceive(events_, &event, 0) == pdTRUE) {
            if (event.type != UART_DATA || ++drained >= kEventQueueSize) return -1;
        }
        return uart_read_bytes(port(), bytes, static_cast<std::uint32_t>(capacity), 0);
    }

    int write(const Frame &frame) override
    {
        if (!installed_) return -1;
        // No software TX queue: a bounded FIFO write followed by bounded drain.
        const int count = uart_tx_chars(port(), reinterpret_cast<const char *>(frame.data()), frame.size());
        const auto ticks = pdMS_TO_TICKS(proflame2_config::kTimings.response_timeout_ms);
        if (count != static_cast<int>(frame.size()) || uart_wait_tx_done(port(), ticks ? ticks : 1) != ESP_OK) return -1;
        return count;
    }

    bool flush() override
    {
        return installed_ && uart_flush_input(port()) == ESP_OK && xQueueReset(events_) == pdPASS;
    }

    void close() override
    {
        if (gate_ready_) inhibit(true);
        if (installed_) uart_driver_delete(port());
        installed_ = false;
        events_ = nullptr;
        // Leave the independent transmit gate asserted; do not float its pin.
    }
private:
    static uart_port_t port() { return static_cast<uart_port_t>(board::kX0aUart); }
    QueueHandle_t events_ = nullptr;
    bool installed_ = false, gate_ready_ = false;
};

UartTransport transport;
Session session(transport, proflame2_config::kEnabled, proflame2_config::kQualifiedForControl,
                proflame2_config::kTimings, proflame2_config::kCapabilities);
SemaphoreHandle_t mutex = nullptr;
bool initialized = false;

// Every policy/transport call is under this mutex. No command queue exists.
void worker(void *)
{
    std::uint32_t reported_id = 0;
    while (true) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        session.poll(now_ms());
        const auto id = session.operation_id();
        const bool completed = !session.busy() && id != 0 && id != reported_id;
        const auto result = session.snapshot(now_ms()).last_outcome;
        if (completed) reported_id = id;
        xSemaphoreGive(mutex);
        if (completed) std::printf("X0A result local_id=%lu outcome=%s\n", static_cast<unsigned long>(id), outcome_name(result));
        vTaskDelay(1);
    }
}

esp_err_t submit(bool write, Change change)
{
    if (!mutex) {
        std::printf("X0A rejected outcome=Disabled\n");
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(mutex, 0) != pdTRUE) {
        std::printf("X0A rejected outcome=Busy\n");
        return ESP_ERR_INVALID_STATE;
    }
    const auto result = write ? session.change(change, now_ms()) : session.refresh(now_ms());
    if (result == Outcome::None)
        std::printf("X0A accepted local_id=%lu; result pending\n", static_cast<unsigned long>(session.operation_id()));
    else std::printf("X0A rejected outcome=%s\n", outcome_name(result));
    xSemaphoreGive(mutex);
    if (result == Outcome::None) return ESP_OK;
    return result == Outcome::InvalidRequest || result == Outcome::Unsupported ? ESP_ERR_INVALID_ARG : ESP_ERR_INVALID_STATE;
}
} // namespace

esp_err_t init()
{
    if (initialized) return ESP_ERR_INVALID_STATE;
    initialized = true;
    if (!proflame2_config::kEnabled) return ESP_OK;
    if (!proflame2_config::kQualifiedForControl || !valid_timings(proflame2_config::kTimings) || !transport.qualified())
        return ESP_ERR_INVALID_STATE;
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return ESP_ERR_NO_MEM;
    if (!session.init() || !session.active() || xTaskCreate(worker, "x0a", 4096, nullptr, 2, nullptr) != pdPASS) {
        session.quiesce(now_ms());
        vSemaphoreDelete(mutex);
        mutex = nullptr;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t refresh() { return submit(false, {Field::Power,0}); }
esp_err_t request(Change change) { return submit(true, change); }

Snapshot snapshot()
{
    if (!mutex) return session.snapshot(now_ms());
    xSemaphoreTake(mutex, portMAX_DELAY);
    const auto value = session.snapshot(now_ms());
    xSemaphoreGive(mutex);
    return value;
}

esp_err_t print_status()
{
    if (mutex && xSemaphoreTake(mutex, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    const auto now = now_ms();
    const auto value = session.snapshot(now);
    const bool active = session.active(), working = session.busy();
    if (mutex) xSemaphoreGive(mutex);
    std::printf("X0A device=%s configured=%d enabled=%d hardware_qualified=%d busy=%d ownership=%s "
                "observed_ms=%llu age_ms=%llu readback_valid=%d recovery_required=%d outcome=%s\n",
                proflame2_config::kDeviceId, proflame2_config::kEnabled, active, board::kX0aHardwareQualified,
                working, ownership_name(value.ownership), static_cast<unsigned long long>(value.observed_ms),
                static_cast<unsigned long long>(value.observed_ms ? now - value.observed_ms : 0),
                value.readback_valid, value.resync_required, outcome_name(value.last_outcome));
    if (value.readback_valid) std::printf("X0A register_readback state1=%02X state2=%02X; combustion status unknown\n", value.state1, value.state2);
    else std::printf("X0A register_readback unknown; combustion status unknown\n");
    return ESP_OK;
}

bool busy()
{
    if (!mutex) return false;
    if (xSemaphoreTake(mutex, 0) != pdTRUE) return true;
    const bool value = session.busy();
    xSemaphoreGive(mutex);
    return value;
}

void quiesce()
{
    if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
    session.quiesce(now_ms());
    initialized = true;
    if (mutex) xSemaphoreGive(mutex);
}

esp_err_t prepare_reset()
{
    if (mutex && xSemaphoreTake(mutex, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    if (session.busy()) {
        if (mutex) xSemaphoreGive(mutex);
        return ESP_ERR_INVALID_STATE;
    }
    session.quiesce(now_ms());
    initialized = true;
    if (mutex) xSemaphoreGive(mutex);
    return ESP_OK;
}
} // namespace proflame2_service
