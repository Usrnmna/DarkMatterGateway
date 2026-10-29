set(PROFILE_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../config/devices.json" CACHE FILEPATH
    "Appliance profile configuration")
set(PROFILE_GENERATOR "${CMAKE_CURRENT_LIST_DIR}/../../tools/profile_config.py")
set(PROFILE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/profiles.hpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROFILE_CONFIG}" "${PROFILE_GENERATOR}")
execute_process(COMMAND "${PYTHON}" "${PROFILE_GENERATOR}" "${PROFILE_CONFIG}"
    --header "${PROFILE_HEADER}" RESULT_VARIABLE PROFILE_RESULT)
if(NOT PROFILE_RESULT EQUAL 0)
    message(FATAL_ERROR "Invalid appliance profile configuration")
endif()
set(PROFLAME2_CONFIG "${CMAKE_CURRENT_LIST_DIR}/../../config/proflame2.json" CACHE FILEPATH
    "Disabled-by-default X0A configuration")
set(PROFLAME2_GENERATOR "${CMAKE_CURRENT_LIST_DIR}/../../tools/proflame2_config.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROFLAME2_CONFIG}" "${PROFLAME2_GENERATOR}" "${PROFILE_CONFIG}" "${PROFILE_GENERATOR}")
execute_process(COMMAND "${PYTHON}" "${PROFLAME2_GENERATOR}" "${PROFLAME2_CONFIG}"
    --ir-profile "${PROFILE_CONFIG}" --header "${CMAKE_CURRENT_BINARY_DIR}/generated/proflame2_config.hpp"
    RESULT_VARIABLE PROFLAME2_RESULT)
if(NOT PROFLAME2_RESULT EQUAL 0)
    message(FATAL_ERROR "Invalid or unqualified X0A configuration")
endif()
idf_component_register(
    SRCS "app_main.cpp" "matter_app.cpp" "ir_service.cpp" "proflame2_service.cpp" "terminal.cpp" "console_line.cpp"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}/generated"
    REQUIRES ir_core proflame2_core esp_matter esp_driver_gpio esp_driver_rmt esp_driver_uart esp_timer nvs_flash console)
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=linenoise" "-Wl,--wrap=linenoiseProbe"
    # Force extraction from the component archive before console references resolve.
    "-Wl,--undefined=__wrap_linenoise" "-Wl,--undefined=__wrap_linenoiseProbe")
