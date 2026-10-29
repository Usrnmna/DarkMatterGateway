#include "board_config.hpp"
#include "ir_service.hpp"
#include "proflame2_service.hpp"
#include "matter_app.hpp"
#include "terminal.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

namespace {
// A stable press opens commissioning once per press. It does not transmit IR,
// erase learned commands, or change appliance power.
void button_task(void *)
{
    unsigned low_samples = 0;
    bool handled = false;
    while (true) {
        if (gpio_get_level(board::kCommissionButton) == 0) {
            if (low_samples < 50) ++low_samples;
            if (low_samples == 50 && !handled) {
                handled = true;
                const auto error = matter_app::open_commissioning();
                if (error != ESP_OK) ESP_LOGW("button", "Commissioning unavailable: %s", esp_err_to_name(error));
            }
        } else {
            low_samples = 0;
            handled = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
} // namespace

extern "C" void app_main()
{
    // Preserve flash on initialization errors. Recovery must be explicit.
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(ir_service::init());
    const auto x0a_error = proflame2_service::init();
    if (x0a_error != ESP_OK) ESP_LOGW("startup", "X0A unavailable: %s", esp_err_to_name(x0a_error));
    gpio_config_t button{};
    button.pin_bit_mask = 1ULL << board::kCommissionButton;
    button.mode = GPIO_MODE_INPUT;
    button.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&button));
    gpio_config_t indicator{};
    indicator.pin_bit_mask = 1ULL << board::kStatusLed;
    indicator.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&indicator));
    gpio_set_level(board::kStatusLed, 0);
    ESP_ERROR_CHECK(matter_app::init());
    ESP_ERROR_CHECK(start_terminal());
    if (xTaskCreate(button_task, "commission_button", 3072, nullptr, 2, nullptr) != pdPASS)
        ESP_LOGE("startup", "Commissioning button task unavailable; use the console command");
}
