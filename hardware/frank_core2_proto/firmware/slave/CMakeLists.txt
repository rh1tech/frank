#
# FRANK Core 2 Proto — dual-RP2350 test firmware (slave)
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#

cmake_minimum_required(VERSION 3.13)

set(FW_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# The slave is the RP2350A (QFN-60). Its own board header pins the
# console to UART1 on GPIO24/25 — GPIO0, where the SDK would put uart0
# TX, is the PSRAM chip select on this half of the board.
set(PICO_BOARD_HEADER_DIRS ${FW_ROOT}/boards CACHE PATH "" FORCE)
set(PICO_BOARD "frank_core2_proto_slave" CACHE STRING "Pico board type" FORCE)

# ---------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------
# The slave has no HID host to run, but it honours the same flag as the
# master so one build script can configure both consistently:
#   USB_HID_ENABLED  OFF -> USB CDC serial console on J9
#                    ON  -> UART1 serial console on the J4 header
option(USB_HID_ENABLED "Move the slave console from USB CDC to UART1" OFF)

set(CPU_SPEED   "252" CACHE STRING "CPU clock in MHz")
set(PSRAM_SPEED "133" CACHE STRING "PSRAM max frequency in MHz")
set(FLASH_SPEED "66"  CACHE STRING "Flash target max frequency in MHz")

if(CPU_SPEED GREATER_EQUAL 504)
    set(CPU_VOLTAGE "VREG_VOLTAGE_1_65")
elseif(CPU_SPEED GREATER_EQUAL 300)
    set(CPU_VOLTAGE "VREG_VOLTAGE_1_60")
else()
    set(CPU_VOLTAGE "VREG_VOLTAGE_1_50")
endif()

message(STATUS "Core2 slave: CPU ${CPU_SPEED} MHz, PSRAM ${PSRAM_SPEED} MHz, "
               "flash ${FLASH_SPEED} MHz, vreg ${CPU_VOLTAGE}, USB_HID ${USB_HID_ENABLED}")

include(${FW_ROOT}/pico_sdk_import.cmake)
project(frank-core2-slave C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(frank-core2-slave
    src/main.c

    ${FW_ROOT}/common/link_bus.c
    ${FW_ROOT}/common/link_proto.c
    ${FW_ROOT}/common/link_session.c
    ${FW_ROOT}/common/mem_test.c
    ${FW_ROOT}/common/heartbeat.c

    ${FW_ROOT}/drivers/psram_init.c
)

target_include_directories(frank-core2-slave PRIVATE
    src
    ${FW_ROOT}/common
    ${FW_ROOT}/drivers
)

pico_generate_pio_header(frank-core2-slave ${FW_ROOT}/common/link_bus.pio)

target_compile_definitions(frank-core2-slave PRIVATE
    CPU_CLOCK_MHZ=${CPU_SPEED}
    CPU_VOLTAGE=${CPU_VOLTAGE}
    PSRAM_MAX_FREQ_MHZ=${PSRAM_SPEED}
    FLASH_MAX_FREQ_MHZ=${FLASH_SPEED}
    PICO_ON_DEVICE=1
    FIRMWARE_VERSION="1.0"

    # Emit stdout even when the host has not raised DTR. A diagnostic
    # firmware that silently discards its own log because a terminal
    # was attached a second too late is useless exactly when it matters,
    # and the bounded stdout timeout keeps an unread port from stalling
    # the run.
    PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=20000
    # HEARTBEAT_WS2812 deliberately unset: the slave drives a plain LED.
)

target_compile_options(frank-core2-slave PRIVATE
    -O2
    -Wall
    -Wno-unused-parameter
    -Wno-unused-function
)

target_link_libraries(frank-core2-slave
    pico_stdlib
    hardware_clocks
    hardware_dma
    hardware_flash
    hardware_gpio
    hardware_pio
    hardware_sync
    hardware_vreg
    hardware_watchdog
    hardware_xip_cache
)

# Literal 0/1 required here — see the note in master/CMakeLists.txt.
if(USB_HID_ENABLED)
    pico_enable_stdio_uart(frank-core2-slave 1)
    pico_enable_stdio_usb(frank-core2-slave  0)
else()
    pico_enable_stdio_uart(frank-core2-slave 0)
    pico_enable_stdio_usb(frank-core2-slave  1)
endif()

pico_add_extra_outputs(frank-core2-slave)
