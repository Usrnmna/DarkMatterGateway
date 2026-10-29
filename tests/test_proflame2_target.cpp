#include "service_stubs/support.hpp"
#include "../firmware/main/proflame2_service.cpp"
#include <cassert>
#include <iostream>

namespace { unsigned uart_calls = 0; }
esp_err_t uart_driver_install(uart_port_t,int,int,int,QueueHandle_t*,int) { ++uart_calls; return ESP_FAIL; }
esp_err_t uart_driver_delete(uart_port_t) { ++uart_calls; return ESP_OK; }
esp_err_t uart_enable_intr_mask(uart_port_t,std::uint32_t) { ++uart_calls; return ESP_FAIL; }
esp_err_t uart_param_config(uart_port_t,const uart_config_t*) { ++uart_calls; return ESP_FAIL; }
esp_err_t uart_set_pin(uart_port_t,int,int,int,int) { ++uart_calls; return ESP_FAIL; }
int uart_read_bytes(uart_port_t,void*,std::uint32_t,TickType_t) { ++uart_calls; return -1; }
int uart_tx_chars(uart_port_t,const char*,std::uint32_t) { ++uart_calls; return -1; }
esp_err_t uart_wait_tx_done(uart_port_t,TickType_t) { ++uart_calls; return ESP_FAIL; }
esp_err_t uart_flush_input(uart_port_t) { ++uart_calls; return ESP_FAIL; }
BaseType_t xQueueReset(QueueHandle_t) { ++uart_calls; return pdPASS; }
std::int64_t esp_timer_get_time() { return 1000000; }

int main()
{
    using namespace proflame2_service;
    assert(init() == (proflame2_config::kEnabled ? ESP_ERR_INVALID_STATE : ESP_OK));
    assert(!busy());
    assert(refresh() == ESP_ERR_INVALID_STATE);
    assert(request({proflame2::Field::Power,1}) == ESP_ERR_INVALID_STATE);
    assert(print_status() == ESP_OK);
    assert(!snapshot().readback_valid);
    assert(prepare_reset() == ESP_OK);
    quiesce();
    assert(init() == ESP_ERR_INVALID_STATE);
    assert(refresh() == ESP_ERR_INVALID_STATE && !busy());
    assert(uart_calls == 0 && fake::state.calls.empty());
    assert(fake::semaphores() == 0);
    std::cout << "Proflame2 target activation boundary tests passed\n";
}
