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
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(tRexRunner C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(tRexRunner tRexRunner.c ssd1306/ssd1306.c)

pico_set_program_name(tRexRunner "tRexRunner")
pico_set_program_version(tRexRunner "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(tRexRunner 0)
pico_enable_stdio_usb(tRexRunner 1)

# Add the standard library to the build
target_link_libraries(tRexRunner
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(tRexRunner PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ssd1306
  )

# Add any user requested libraries
target_link_libraries(tRexRunner
        hardware_i2c
        pico_cyw43_arch_none
        )

pico_add_extra_outputs(tRexRunner)

