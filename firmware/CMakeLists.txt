cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{ESP_MATTER_PATH} OR NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "Export the pinned ESP-IDF and ESP-Matter environments; see docs/build.md")
endif()
set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)
set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/components"
    "${MATTER_SDK_PATH}/config/esp32/components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(matter_ir_terminal)
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-DCHIP_HAVE_CONFIG_H" APPEND)
