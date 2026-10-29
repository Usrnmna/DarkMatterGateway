#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
mkdir -p build
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 tools/profile_config.py config/devices.json --header build/generated_profiles.hpp
python3 tools/proflame2_config.py config/proflame2.json --ir-profile config/devices.json --header build/x0a-generated/proflame2_config.hpp
for suite in protocol controller service; do
  "${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -g -Ifirmware/components/proflame2_core/include \
    firmware/components/proflame2_core/{protocol,controller,session}.cpp \
    "tests/test_proflame2_${suite}.cpp" -o "build/test_proflame2_${suite}"
  "./build/test_proflame2_${suite}"
done
for configuration in build/x0a-generated tests/x0a_unqualified; do
  "${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -g \
    -I"$configuration" -Itests/x0a_stubs -Itests/service_stubs \
    -Ifirmware/components/proflame2_core/include \
    firmware/components/proflame2_core/{protocol,controller,session}.cpp \
    tests/service_stubs/support.cpp tests/test_proflame2_target.cpp \
    -o build/test_proflame2_target
  ./build/test_proflame2_target
done
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -g \
  -Itests/x0a_unqualified -Itests/x0a_stubs -Itests/service_stubs \
  -Ifirmware/components/proflame2_core/include \
  firmware/components/proflame2_core/{protocol,controller,session}.cpp \
  tests/test_proflame2_uart.cpp -o build/test_proflame2_uart
./build/test_proflame2_uart
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -g \
  -Ifirmware/components/ir_core/include \
  firmware/components/ir_core/ir_core.cpp tests/test_ir_core.cpp \
  -o build/test_ir_core
./build/test_ir_core
for suite in ir_service ir_storage; do
  "${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -g \
    -Itests/service_stubs -Ifirmware/components/ir_core/include \
    firmware/components/ir_core/ir_core.cpp tests/service_stubs/support.cpp \
    "tests/test_${suite}.cpp" -o "build/test_${suite}"
  "./build/test_${suite}"
done
for suite in matter_app startup; do
  "${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -g \
    -Itests/app_stubs -Ifirmware/components/ir_core/include -Ifirmware/components/proflame2_core/include \
    firmware/components/ir_core/ir_core.cpp tests/app_stubs/support.cpp \
    "tests/test_${suite}.cpp" -o "build/test_${suite}"
  "./build/test_${suite}"
done
for console_usb in 0 1; do
  "${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -g \
    -DCONFIG_ESP_CONSOLE_USB_SERIAL_JTAG="$console_usb" \
    -Itests/terminal_stubs -Ifirmware/components/ir_core/include -Ifirmware/components/proflame2_core/include \
    tests/test_terminal.cpp -o "build/test_terminal_$console_usb"
  "./build/test_terminal_$console_usb"
done
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -g \
  -Ifirmware/main firmware/main/console_line.cpp tests/test_console_line.cpp \
  -Wl,--wrap=linenoise -Wl,--wrap=linenoiseProbe \
  -o build/test_console_line
./build/test_console_line
