// HOST ONLY: exercise the actual adapter with synthetic pins and SDK calls.
// No generated production configuration or hardware allocation uses this board.
#include "x0a_stubs/sdk.hpp"
#include "../firmware/main/board_config.hpp"
namespace synthetic_x0a_board {
constexpr bool kX0aHardwareQualified = true;
constexpr int kX0aUart = 2, kX0aTx = 47, kX0aRx = 48, kX0aTxInhibit = 15, kX0aInhibitLevel = 1;
}
#define board synthetic_x0a_board
#include "../firmware/main/proflame2_service.cpp"
#undef board
#include <cassert>
#include <cstring>
#include <iostream>
#include <deque>
#include <map>

struct HostQueue { unsigned capacity; std::deque<uart_event_t> items; };

namespace {
enum class Fault { None, Install, Parameters, Pins, Interrupt, TxShort, TxWait, Rx, Flush };
Fault fault = Fault::None;
QueueHandle_t events = nullptr;
std::uint32_t enabled_interrupts = 0;
unsigned installs = 0, deletes = 0, transmissions = 0;
proflame2::Frame last_frame{};
std::map<int,std::uint32_t> pins;
}

// Value-copy event queues match the UART API; the IR harness uses pointer jobs.
QueueHandle_t xQueueCreate(unsigned capacity,unsigned size)
{
    assert(size == sizeof(uart_event_t));
    return new HostQueue{capacity,{}};
}
BaseType_t xQueueSend(QueueHandle_t queue,const void *event,TickType_t)
{
    if (queue->items.size() == queue->capacity) return pdFALSE;
    queue->items.push_back(*static_cast<const uart_event_t *>(event)); return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue,void *event,TickType_t ticks)
{
    assert(ticks == 0);
    if (queue->items.empty()) return pdFALSE;
    *static_cast<uart_event_t *>(event) = queue->items.front(); queue->items.pop_front(); return pdTRUE;
}
void vQueueDelete(QueueHandle_t queue) { delete queue; }
esp_err_t gpio_set_level(gpio_num_t pin,std::uint32_t level) { assert(pin == 15); pins[pin] = level; return ESP_OK; }
esp_err_t gpio_config(const gpio_config_t *config) {
    assert(config->pin_bit_mask == (1ULL << 15) && config->mode == GPIO_MODE_OUTPUT); return ESP_OK;
}
// These service-only entry points are linked but never run by the adapter test.
SemaphoreHandle_t xSemaphoreCreateMutex() { assert(false); return nullptr; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t,TickType_t) { assert(false); return pdFALSE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t) { assert(false); return pdFALSE; }
void vSemaphoreDelete(SemaphoreHandle_t) { assert(false); }
BaseType_t xTaskCreate(TaskFunction_t,const char*,unsigned,void*,unsigned,void*) { assert(false); return pdFALSE; }
void vTaskDelay(TickType_t) { assert(false); }

esp_err_t uart_driver_install(uart_port_t port,int rx,int tx,int count,QueueHandle_t *queue,int)
{
    assert(port == 2 && rx >= 128 && tx == 0 && count == 16);
    ++installs;
    if (fault == Fault::Install) return ESP_FAIL;
    events = xQueueCreate(count, sizeof(uart_event_t));
    *queue = events;
    return ESP_OK;
}
esp_err_t uart_driver_delete(uart_port_t)
{
    ++deletes;
    if (events) vQueueDelete(events);
    events = nullptr;
    return ESP_OK;
}
esp_err_t uart_enable_intr_mask(uart_port_t,std::uint32_t mask)
{
    enabled_interrupts |= mask;
    return fault == Fault::Interrupt ? ESP_FAIL : ESP_OK;
}
esp_err_t uart_param_config(uart_port_t,const uart_config_t *config)
{
    assert(config->baud_rate == 9600 && config->data_bits == UART_DATA_8_BITS &&
           config->parity == UART_PARITY_DISABLE && config->stop_bits == UART_STOP_BITS_1 &&
           config->flow_ctrl == UART_HW_FLOWCTRL_DISABLE);
    return fault == Fault::Parameters ? ESP_FAIL : ESP_OK;
}
esp_err_t uart_set_pin(uart_port_t,int tx,int rx,int rts,int cts)
{
    assert(tx == 47 && rx == 48 && rts == -1 && cts == -1);
    return fault == Fault::Pins ? ESP_FAIL : ESP_OK;
}
int uart_read_bytes(uart_port_t,void*,std::uint32_t,TickType_t ticks)
{
    assert(ticks == 0);
    return fault == Fault::Rx ? -1 : 0;
}
int uart_tx_chars(uart_port_t,const char *bytes,std::uint32_t size)
{
    ++transmissions;
    assert(size == 6 && pins.at(15) == 0);
    std::memcpy(last_frame.data(),bytes,size);
    return fault == Fault::TxShort ? 3 : 6;
}
esp_err_t uart_wait_tx_done(uart_port_t,TickType_t ticks)
{
    assert(ticks == 100);
    return fault == Fault::TxWait ? ESP_FAIL : ESP_OK;
}
esp_err_t uart_flush_input(uart_port_t) { return fault == Fault::Flush ? ESP_FAIL : ESP_OK; }
BaseType_t xQueueReset(QueueHandle_t queue)
{
    uart_event_t event{};
    while (xQueueReceive(queue,&event,0) == pdTRUE) {}
    return pdPASS;
}
std::int64_t esp_timer_get_time() { return 1000000; }

int main()
{
    using proflame2_service::UartTransport;
    {
        UartTransport uart;
        assert(uart.qualified() && uart.open());
        // Regression: the pinned SDK default mask omits frame-error interrupts.
        assert((enabled_interrupts & UART_INTR_FRAM_ERR) != 0);
        assert(pins.at(15) == 1 && transmissions == 0);
        for (int error : {UART_FRAME_ERR, UART_PARITY_ERR, UART_FIFO_OVF, UART_BUFFER_FULL}) {
            uart_event_t event{error,0,false};
            assert(xQueueSend(events,&event,0) == pdTRUE);
            std::uint8_t bytes[7]{};
            assert(uart.read(bytes,sizeof(bytes)) == -1);
            assert(uart.flush());
        }
        const auto frame = proflame2::encode({proflame2::Type::Get,{},proflame2::Register::Control});
        assert(uart.inhibit(false) && uart.write(frame) == 6);
        assert(last_frame == frame && uart.inhibit(true));
        for (auto error : {Fault::TxShort,Fault::TxWait}) {
            fault = error; assert(uart.inhibit(false));
            assert(uart.write(frame) == -1 && uart.inhibit(true));
        }
        fault = Fault::Rx; std::uint8_t bytes[7]{}; assert(uart.read(bytes,sizeof(bytes)) == -1);
        fault = Fault::Flush; assert(!uart.flush());
        fault = Fault::None; uart.close();
        assert(pins.at(15) == 1 && events == nullptr);
    }
    for (auto error : {Fault::Install,Fault::Parameters,Fault::Pins,Fault::Interrupt}) {
        fault = error;
        UartTransport uart;
        const auto previous_deletes = deletes;
        assert(!uart.open());
        assert(events == nullptr && pins.at(15) == 1);
        assert(deletes == previous_deletes + (error == Fault::Install ? 0U : 1U));
    }
    assert(installs == 5 && events == nullptr);
    std::cout << "Proflame2 UART adapter tests passed\n";
}
