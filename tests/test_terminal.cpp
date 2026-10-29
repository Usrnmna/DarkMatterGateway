#include <cassert>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>
#include "../firmware/main/proflame2_service.hpp"
#include "../firmware/main/terminal.cpp"

namespace {
esp_err_t service_result = ESP_OK;
esp_err_t repl_create_result = ESP_OK;
unsigned service_calls = 0;
unsigned repl_starts = 0;
unsigned x0a_calls = 0;
esp_err_t x0a_result = ESP_OK;
proflame2::Change last_change{proflame2::Field::Power,0};
unsigned sent_zone = 0;
bool overwrite_saved = false;
std::string sent_name;

// Dispatch actual firmware command handlers with writable argv storage.
int invoke(const char *name, std::initializer_list<const char *> arguments)
{
    std::vector<std::string> storage{name};
    for (const auto *argument : arguments) storage.emplace_back(argument);
    std::vector<char *> argv;
    for (auto &argument : storage) argv.push_back(argument.data());
    for (const auto &command : commands) {
        if (std::strcmp(command.name, name) == 0)
            return command.function(static_cast<int>(argv.size()), argv.data());
    }
    assert(false);
    return 1;
}

esp_err_t called() { ++service_calls; return service_result; }
}

namespace ir_service {
esp_err_t learn(Receiver) { return called(); }
esp_err_t send_pending(unsigned, unsigned, unsigned) { return called(); }
esp_err_t save_pending(const char *, bool overwrite) { overwrite_saved = overwrite; return called(); }
esp_err_t send(const char *name, unsigned zone, unsigned, unsigned)
{
    sent_name = name;
    sent_zone = zone;
    return called();
}
esp_err_t print_pending() { return called(); }
esp_err_t print_saved(const char *) { return called(); }
esp_err_t list() { return called(); }
esp_err_t remove(const char *) { return called(); }
}
namespace matter_app {
esp_err_t open_commissioning() { return called(); }
esp_err_t reset_fabrics() { return called(); }
}
namespace proflame2_service {
esp_err_t print_status() { ++x0a_calls; return x0a_result; }
esp_err_t refresh() { ++x0a_calls; return x0a_result; }
esp_err_t request(proflame2::Change change) { ++x0a_calls; last_change = change; return x0a_result; }
}
esp_err_t esp_console_new_repl_uart(const esp_console_dev_uart_config_t *,
                                  const esp_console_repl_config_t *, esp_console_repl_t **) { return repl_create_result; }
esp_err_t esp_console_new_repl_usb_serial_jtag(const esp_console_dev_usb_serial_jtag_config_t *,
                                            const esp_console_repl_config_t *, esp_console_repl_t **) { return repl_create_result; }
esp_err_t esp_console_cmd_register(const esp_console_cmd_t *) { return ESP_OK; }
esp_err_t esp_console_start_repl(esp_console_repl_t *) { ++repl_starts; return ESP_OK; }

int main()
{
    const auto ir_before_x0a = service_calls;
    assert(invoke("x0a", {"status"}) == 0);
    assert(invoke("x0a", {"refresh"}) == 0);
    assert(invoke("x0a", {"set","power","on"}) == 0);
    assert(last_change.field == proflame2::Field::Power && last_change.value == 1);
    assert(invoke("x0a", {"set","power","off"}) == 0 && last_change.value == 0);
    assert(invoke("x0a", {"set","fan","3"}) == 0);
    assert(last_change.field == proflame2::Field::Fan && last_change.value == 3);
    assert(invoke("x0a", {"set","flame","0"}) == 0);
    assert(invoke("x0a", {"set","light","6"}) == 0);
    assert(invoke("x0a", {"set","aux","on"}) == 0);
    assert(invoke("x0a", {"set","split","off"}) == 0);
    const auto valid_x0a = x0a_calls;
    for (const char *field : {"pilot", "thermostat", "smart", "reset", "register", "unknown"})
        assert(invoke("x0a", {"set",field,"on"}) != 0);
    for (const char *value : {"7", "-1", "+1", "1junk", "999999999999999999999999"})
        assert(invoke("x0a", {"set","fan",value}) != 0);
    assert(invoke("x0a", {}) != 0);
    assert(invoke("x0a", {"status","extra"}) != 0);
    assert(invoke("x0a", {"refresh","extra"}) != 0);
    assert(invoke("x0a", {"set","power","1"}) != 0);
    assert(invoke("x0a", {"set","power","on","extra"}) != 0);
    assert(x0a_calls == valid_x0a && service_calls == ir_before_x0a);
    for (esp_err_t error : {ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_ARG}) {
        x0a_result = error; // Disabled/busy/recovery or unqualified field.
        assert(invoke("x0a", {"set","power","on"}) != 0);
        assert(invoke("x0a", {"refresh"}) != 0);
    }
    assert(service_calls == ir_before_x0a);
    x0a_result = ESP_OK;
    // Read failures must remain failures at the console boundary.
    service_result = ESP_ERR_NOT_FOUND;
    assert(invoke("show", {}) != 0);
    assert(invoke("show", {"missing"}) != 0);
    assert(invoke("codes", {}) != 0);
    service_result = ESP_OK;
    assert(invoke("show", {}) == 0);
    assert(invoke("codes", {}) == 0);

    // Unconfirmed writes and malformed arguments cannot reach the service.
    const auto calls_before_invalid = service_calls;
    assert(invoke("save", {"test"}) != 0);
    assert(invoke("save", {"test", "--overwrite"}) != 0);
    assert(invoke("delete", {"test"}) != 0);
    assert(invoke("matter-reset", {}) != 0);
    assert(invoke("send", {"test", "-1"}) != 0);
    assert(invoke("send", {"test", "1junk"}) != 0);
    assert(invoke("send", {"test", "99999999999999999999999999"}) != 0);
    assert(invoke("send", {"test", "3"}) != 0);
    assert(invoke("send", {"test", "0", "6"}) != 0);
    assert(invoke("send", {"test", "0", "1", "501"}) != 0);
    assert(invoke("call", {"disabled", "on"}) != 0);
    assert(invoke("call", {"enabled", "missing"}) != 0);
    assert(service_calls == calls_before_invalid);

    assert(invoke("save", {"test", "--confirmed"}) == 0);
    assert(!overwrite_saved);
    assert(invoke("save", {"test", "--confirmed", "--overwrite"}) == 0);
    assert(overwrite_saved);
    assert(invoke("delete", {"test", "--confirmed"}) == 0);
    assert(invoke("matter-reset", {"--confirmed"}) == 0);
    assert(invoke("call", {"enabled", "on"}) == 0);
    assert(sent_name == "light_on" && sent_zone == 2);
    service_result = ESP_ERR_INVALID_STATE;
    assert(invoke("call", {"enabled", "on"}) != 0);
    assert(invoke("save", {"test", "--confirmed"}) != 0);

    repl_create_result = ESP_FAIL;
    assert(start_terminal() == ESP_FAIL);
    assert(repl_starts == 0);
    repl_create_result = ESP_OK;
    assert(start_terminal() == ESP_OK && repl_starts == 1);
    std::cout << "Console dispatch tests passed\n";
}
