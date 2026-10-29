#pragma once
#include "esp_err.h"

// Minimal declarations isolate console dispatch from ESP-IDF peripheral setup.
using esp_console_cmd_func_t = int (*)(int, char **);
struct esp_console_cmd_t {
    const char *command;
    const char *help;
    esp_console_cmd_func_t func;
};
struct esp_console_repl_t {};
struct esp_console_repl_config_t {
    const char *prompt;
    unsigned max_cmdline_length;
    unsigned task_stack_size;
};
struct esp_console_dev_uart_config_t {};
struct esp_console_dev_usb_serial_jtag_config_t {};
#define ESP_CONSOLE_REPL_CONFIG_DEFAULT() {}
#define ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT() {}
#define ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT() {}

esp_err_t esp_console_new_repl_uart(const esp_console_dev_uart_config_t *,
                                  const esp_console_repl_config_t *, esp_console_repl_t **);
esp_err_t esp_console_new_repl_usb_serial_jtag(const esp_console_dev_usb_serial_jtag_config_t *,
                                            const esp_console_repl_config_t *, esp_console_repl_t **);
esp_err_t esp_console_cmd_register(const esp_console_cmd_t *);
esp_err_t esp_console_start_repl(esp_console_repl_t *);
