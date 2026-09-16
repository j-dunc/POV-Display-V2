# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(rp2350-display C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(rp2350-display
    main.c
    spi_link.c
    drivers/led_shift.c
    drivers/hall_sensor.c
    render/frame_buffer.c
    render/led_map.c
    render/image_source.c
    render/render.c)

pico_generate_pio_header(rp2350-display ${CMAKE_CURRENT_LIST_DIR}/drivers/led_shift.pio)

pico_set_program_name(rp2350-display "rp2350-display")
pico_set_program_version(rp2350-display "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(rp2350-display 0)
pico_enable_stdio_usb(rp2350-display 1)

# Add the standard library to the build
target_link_libraries(rp2350-display
        pico_stdlib
        hardware_pio
        hardware_dma
        hardware_spi
        pico_multicore
        cmsis_core)
# Add the standard include files to the build
target_include_directories(rp2350-display PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/drivers
        ${CMAKE_CURRENT_LIST_DIR}/render
        ${CMAKE_CURRENT_LIST_DIR}/../shared
)

pico_add_extra_outputs(rp2350-display)

