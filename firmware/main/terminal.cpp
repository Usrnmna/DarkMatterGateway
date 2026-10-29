#include "terminal.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "esp_console.h"
#include "console_line.hpp"
#include "ir_service.hpp"
#include "proflame2_service.hpp"
#include "matter_app.hpp"
#include "profiles.hpp"

namespace {
int report(esp_err_t error)
{
    if (error != ESP_OK) std::printf("ERROR %s\n", esp_err_to_name(error));
    return error == ESP_OK ? 0 : 1;
}

bool number(const char *text, unsigned &value, unsigned maximum)
{
    if (!text || *text < '0' || *text > '9') return false;
    char *end = nullptr;
    errno = 0;
    auto parsed = std::strtoul(text, &end, 10);
    if (errno || *end || parsed > maximum) return false;
    value = static_cast<unsigned>(parsed);
    return true;
}

int learn_command(int argc, char **argv)
{
    auto receiver = ir_service::Receiver::Carrier;
    if (argc > 2) return report(ESP_ERR_INVALID_ARG);
    if (argc == 2) {
        if (std::strcmp(argv[1], "38") == 0) receiver = ir_service::Receiver::Envelope38;
        else if (std::strcmp(argv[1], "56") == 0) receiver = ir_service::Receiver::Envelope56;
        else if (std::strcmp(argv[1], "carrier") != 0) return report(ESP_ERR_INVALID_ARG);
    }
    std::printf("Waiting for one remote frame; nothing is saved yet.\n");
    return report(ir_service::learn(receiver));
}

int test_command(int argc, char **argv)
{
    unsigned zone = 0;
    if (argc != 2 || !number(argv[1], zone, 2)) return report(ESP_ERR_INVALID_ARG);
    return report(ir_service::send_pending(zone));
}

int save_command(int argc, char **argv)
{
    if ((argc != 3 && argc != 4) || std::strcmp(argv[2], "--confirmed") != 0 ||
        (argc == 4 && std::strcmp(argv[3], "--overwrite") != 0)) {
        std::printf("Use save NAME --confirmed [--overwrite] after observing correct appliance behavior.\n");
        return 1;
    }
    return report(ir_service::save_pending(argv[1], argc == 4));
}

int send_command(int argc, char **argv)
{
    unsigned zone = 0, repeats = 1, gap = 40;
    if (argc < 3 || argc > 5 || !number(argv[2], zone, 2) ||
        (argc >= 4 && !number(argv[3], repeats, 5)) ||
        (argc == 5 && !number(argv[4], gap, 500))) return report(ESP_ERR_INVALID_ARG);
    return report(ir_service::send(argv[1], zone, repeats, gap));
}

int show_command(int argc, char **argv)
{
    if (argc == 1) return report(ir_service::print_pending());
    if (argc == 2) return report(ir_service::print_saved(argv[1]));
    return report(ESP_ERR_INVALID_ARG);
}

int list_command(int argc, char **)
{
    if (argc != 1) return report(ESP_ERR_INVALID_ARG);
    return report(ir_service::list());
}

int delete_command(int argc, char **argv)
{
    if (argc != 3 || std::strcmp(argv[2], "--confirmed") != 0) return report(ESP_ERR_INVALID_ARG);
    return report(ir_service::remove(argv[1]));
}

int call_command(int argc, char **argv)
{
    if (argc != 3) return report(ESP_ERR_INVALID_ARG);
    for (const auto &profile : configured::profiles) {
        if (std::strcmp(profile.id, argv[1]) != 0) continue;
        if (!profile.enabled) {
            std::printf("Profile disabled; fill configuration and verify its command mappings before enabling.\n");
            return 1;
        }
        for (const auto &action : configured::actions) {
            if (std::strcmp(action.device, profile.id) == 0 && std::strcmp(action.action, argv[2]) == 0)
                return report(ir_service::send(action.code, profile.zone));
        }
    }
    return report(ESP_ERR_NOT_FOUND);
}

int profiles_command(int argc, char **)
{
    if (argc != 1) return report(ESP_ERR_INVALID_ARG);
    for (const auto &profile : configured::profiles)
        std::printf("%s enabled=%d zone=%u\n", profile.id, profile.enabled, profile.zone);
    return 0;
}

int commission_command(int argc, char **)
{
    return report(argc == 1 ? matter_app::open_commissioning() : ESP_ERR_INVALID_ARG);
}

int reset_command(int argc, char **argv)
{
    if (argc != 2 || std::strcmp(argv[1], "--confirmed") != 0) return report(ESP_ERR_INVALID_ARG);
    return report(matter_app::reset_fabrics());
}

struct Command { const char *name; const char *help; esp_console_cmd_func_t function; };
// Typed, explicit ordinary controls only; the service still enforces qualification.
int x0a_command(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "status") == 0) return report(proflame2_service::print_status());
    if (argc == 2 && std::strcmp(argv[1], "refresh") == 0) return report(proflame2_service::refresh());
    if (argc != 4 || std::strcmp(argv[1], "set") != 0) return report(ESP_ERR_INVALID_ARG);
    struct Control { const char *name; proflame2::Field field; bool level; };
    constexpr Control controls[] = {
        {"power", proflame2::Field::Power, false}, {"flame", proflame2::Field::Flame, true},
        {"fan", proflame2::Field::Fan, true}, {"light", proflame2::Field::Light, true},
        {"aux", proflame2::Field::Aux, false}, {"split", proflame2::Field::SplitFlow, false}
    };
    for (const auto &control : controls) {
        if (std::strcmp(control.name, argv[2]) != 0) continue;
        unsigned value = 0;
        if (control.level) {
            if (!number(argv[3], value, 6)) return report(ESP_ERR_INVALID_ARG);
        } else if (std::strcmp(argv[3], "on") == 0) value = 1;
        else if (std::strcmp(argv[3], "off") != 0) return report(ESP_ERR_INVALID_ARG);
        return report(proflame2_service::request({control.field, static_cast<std::uint8_t>(value)}));
    }
    return report(ESP_ERR_INVALID_ARG);
}

constexpr Command commands[] = {
    {"x0a", "x0a status|refresh|set FIELD VALUE: qualified wired control (disabled by default)", x0a_command},
    {"learn", "learn [carrier|38|56]: capture into RAM", learn_command},
    {"test", "test ZONE: transmit the pending capture once", test_command},
    {"save", "save NAME --confirmed [--overwrite]: commit a verified capture", save_command},
    {"send", "send NAME ZONE [REPEATS [GAP_MS]]: transmit a saved command", send_command},
    {"show", "show [NAME]: print pending or saved waveform JSON", show_command},
    {"codes", "codes: list stored command names", list_command},
    {"delete", "delete NAME --confirmed: remove one stored command", delete_command},
    {"call", "call DEVICE ACTION: run a configured named function", call_command},
    {"profiles", "profiles: show compiled appliance configuration", profiles_command},
    {"commission", "commission: open Matter commissioning for 300 seconds", commission_command},
    {"matter-reset", "matter-reset --confirmed: remove Matter configuration; retain IR commands", reset_command}
};
} // namespace

esp_err_t start_terminal()
{
    esp_console_repl_t *repl = nullptr;
    esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    config.prompt = "ir> ";
    config.max_cmdline_length = console_line::kMaxCharacters + 1;
    config.task_stack_size = 8192;
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t hardware = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    auto error = esp_console_new_repl_usb_serial_jtag(&hardware, &config, &repl);
#else
    esp_console_dev_uart_config_t hardware = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    auto error = esp_console_new_repl_uart(&hardware, &config, &repl);
#endif
    if (error != ESP_OK) return error;
    for (const auto &command : commands) {
        esp_console_cmd_t registration{};
        registration.command = command.name;
        registration.help = command.help;
        registration.func = command.function;
        error = esp_console_cmd_register(&registration);
        if (error != ESP_OK) return error;
    }
    return esp_console_start_repl(repl);
}
