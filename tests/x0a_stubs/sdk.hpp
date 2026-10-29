#pragma once
// Host declarations for the pinned UART adapter. No serial device is opened.
#include "../service_stubs/sdk.hpp"
using uart_port_t = int;
constexpr int UART_NUM_MAX = 3, UART_PIN_NO_CHANGE = -1;
constexpr int UART_DATA_8_BITS = 3, UART_PARITY_DISABLE = 0, UART_STOP_BITS_1 = 1;
constexpr int UART_HW_FLOWCTRL_DISABLE = 0, UART_SCLK_DEFAULT = 0, UART_DATA = 0;
constexpr int UART_FRAME_ERR = 1, UART_PARITY_ERR = 2, UART_FIFO_OVF = 3, UART_BUFFER_FULL = 4;
constexpr std::uint32_t UART_INTR_FRAM_ERR = 1U << 3;
struct uart_config_t { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk; };
struct uart_event_t { int type; std::size_t size; bool timeout_flag; };
esp_err_t uart_driver_install(uart_port_t,int,int,int,QueueHandle_t*,int);
esp_err_t uart_driver_delete(uart_port_t);
esp_err_t uart_enable_intr_mask(uart_port_t,std::uint32_t);
esp_err_t uart_param_config(uart_port_t,const uart_config_t*);
esp_err_t uart_set_pin(uart_port_t,int,int,int,int);
int uart_read_bytes(uart_port_t,void*,std::uint32_t,TickType_t);
int uart_tx_chars(uart_port_t,const char*,std::uint32_t);
esp_err_t uart_wait_tx_done(uart_port_t,TickType_t);
esp_err_t uart_flush_input(uart_port_t);
BaseType_t xQueueReset(QueueHandle_t);
std::int64_t esp_timer_get_time();
#define GPIO_IS_VALID_GPIO(pin) ((pin) >= 0 && (pin) <= 48)
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) GPIO_IS_VALID_GPIO(pin)
