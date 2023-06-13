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
        SERVER_IP=\"${SERVER_IP}\"
        SERVER_PORT=\"${SERVER_PORT}\"
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


# ds3232rtc
#add_library(ds3232rtc ds3232rtc.cpp)
#target_link_libraries(ds3232rtc hardware_i2c pico_stdlib pico_runtime )

# ds3231
#add_library(dev_ds3231 dev_ds3231.c)
#target_link_libraries(dev_ds3231 hardware_i2c pico_stdlib pico_runtime )


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
    #lib2_sys
    #dev_ds3231
)

pico_enable_stdio_usb(picowater 1)
pico_enable_stdio_uart(picowater 0)

pico_add_extra_outputs(picowater)
