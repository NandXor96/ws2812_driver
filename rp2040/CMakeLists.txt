cmake_minimum_required(VERSION 3.13)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/generated)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(usb_ws2812)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

file(GLOB SOURCES
    *.c
)

add_executable(usb_ws2812
    ${SOURCES}
)

pico_generate_pio_header(usb_ws2812 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(usb_ws2812 pico_stdlib pico_unique_id tinyusb_board tinyusb_device hardware_pio hardware_dma)

include_directories(".")

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(usb_ws2812)