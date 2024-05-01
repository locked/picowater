# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/../pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

#set(PICO_SENSORS_FETCH_FROM_GIT 1)
include(pico_sensors_import.cmake)

#include(pico_lib2_import.cmake)

project(picowater
    VERSION "0.1"
    LANGUAGES C CXX ASM
)

# set(PICO_DEOPTIMIZED_DEBUG "1")
set(CMAKE_BUILD_TYPE "Release")
#set(PICO_COPY_TO_RAM 1)
#set(SKIP_PICO_MALLOC 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

#add_subdirectory(sleep)

# Add executable. Default name is the project name, version 0.1
add_executable(picowater picowater.cpp)
pico_set_program_version(picowater ${CMAKE_PROJECT_VERSION})

target_compile_definitions(picowater PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        PICO_DEFAULT_LED_PIN=\"${PICO_DEFAULT_LED_PIN}\"
)

target_sources(picowater PRIVATE)

# wifi
add_library(wifi
       wifi.h
       wifi.c
)
target_include_directories(wifi PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
)
target_link_libraries(wifi
    pico_stdlib
    pico_cyw43_arch_lwip_poll
)


# rtc_ds3231
add_library(rtc_ds3231
       rtc_ds3231.h
       rtc_ds3231.c
)
target_include_directories(rtc_ds3231 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
)
target_link_libraries(rtc_ds3231
    pico_stdlib
    hardware_i2c
)


# ssd1306
add_library(ssd1306
       ssd1306.h
       ssd1306.c
)
target_include_directories(ssd1306 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
)
target_link_libraries(ssd1306
    pico_stdlib
    hardware_i2c
)


# json-tiny
add_library(tiny-json
       tiny-json.h
       tiny-json.c
)
target_include_directories(tiny-json PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(tiny-json
    pico_stdlib
)


# main program
target_include_directories(picowater PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(
    picowater
    pico_stdlib
    hardware_dma
    hardware_i2c
    hardware_rtc
    hardware_sleep
    hardware_adc
    pico_cyw43_arch_lwip_poll
    wifi
    rtc_ds3231
    ssd1306
    sensor_PCF8563
    tiny-json
)

pico_enable_stdio_usb(picowater 1)
pico_enable_stdio_uart(picowater 0)

pico_add_extra_outputs(picowater)
