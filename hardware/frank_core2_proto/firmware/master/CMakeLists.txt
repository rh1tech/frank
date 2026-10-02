#
# FRANK Core 2 Proto — dual-RP2350 test firmware (master)
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#

cmake_minimum_required(VERSION 3.13)

set(FW_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# The master is the RP2350B (QFN-80). The stock pico2 definition would
# declare PICO_RP2350A 1, capping NUM_BANK0_GPIOS at 30 and switching
# off PIO's movable GPIO window — the second link bus (GPIO30..39) and
# the WS2812 (GPIO46) would silently alias down into 0..29. Our own
# board header sets PICO_RP2350A 0 and PICO_PIO_USE_GPIO_BASE 1.
set(PICO_BOARD_HEADER_DIRS ${FW_ROOT}/boards CACHE PATH "" FORCE)
set(PICO_BOARD "frank_core2_proto_master" CACHE STRING "Pico board type" FORCE)

# ---------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------
# USB_HID_ENABLED  OFF -> USB CDC serial console on J8, no HID host
#                  ON  -> USB HID host keyboard on J8, console on UART0 (J2)
option(USB_HID_ENABLED "Enable USB HID host keyboard (console moves to UART)" OFF)

# This revision's GPIO43 -> slave RUN net is broken in the schematic, but
# a single wire from R3 pin 1 to S4 pin 1 restores it. Turn this on once
# that wire is fitted and the master can hard-reset the slave, including
# out of lockup. Safe to leave off with the wire fitted (the pin simply
# stays an input) and safe to turn on without it (the pulse goes nowhere).
option(SLAVE_RESET_BODGE "GPIO43 is bodged to the slave's RUN pin" OFF)

# Bench check: pulse FS once at boot and confirm the slave reboots.
# Off by default — it deliberately knocks the slave over, which is not
# something a normal run should do.
option(FS_RESET_SELFTEST "Run the FS slave-reset check once at boot" OFF)

set(CPU_SPEED   "252" CACHE STRING "CPU clock in MHz")
set(PSRAM_SPEED "133" CACHE STRING "PSRAM max frequency in MHz")
set(FLASH_SPEED "66"  CACHE STRING "Flash target max frequency in MHz")

# The RP2350 regulator exposes 1.50 / 1.60 / 1.65 / 1.70 V steps only.
if(CPU_SPEED GREATER_EQUAL 504)
    set(CPU_VOLTAGE "VREG_VOLTAGE_1_65")
elseif(CPU_SPEED GREATER_EQUAL 300)
    set(CPU_VOLTAGE "VREG_VOLTAGE_1_60")
else()
    set(CPU_VOLTAGE "VREG_VOLTAGE_1_50")
endif()

message(STATUS "Core2 master: CPU ${CPU_SPEED} MHz, PSRAM ${PSRAM_SPEED} MHz, "
               "flash ${FLASH_SPEED} MHz, vreg ${CPU_VOLTAGE}, USB_HID ${USB_HID_ENABLED}")

include(${FW_ROOT}/pico_sdk_import.cmake)
project(frank-core2-master C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

# ---------------------------------------------------------------------
# Reused driver libraries (copied verbatim from frank-msx)
# ---------------------------------------------------------------------
add_subdirectory(${FW_ROOT}/drivers/fatfs     fatfs)
add_subdirectory(${FW_ROOT}/drivers/sdcard    sdcard)
add_subdirectory(${FW_ROOT}/drivers/usbhid    usbhid)
add_subdirectory(${FW_ROOT}/drivers/pico_hdmi pico_hdmi)

add_executable(frank-core2-master
    src/main.c
    src/console.c
    src/report.c
    src/diag_link.c
    src/ui_draw.c
    src/ui_font.c

    ${FW_ROOT}/common/link_bus.c
    ${FW_ROOT}/common/link_proto.c
    ${FW_ROOT}/common/link_session.c
    ${FW_ROOT}/common/mem_test.c
    ${FW_ROOT}/common/heartbeat.c

    ${FW_ROOT}/drivers/HDMI_hstx.c
    ${FW_ROOT}/drivers/psram_init.c
    ${FW_ROOT}/drivers/uart_logging.c
)

target_include_directories(frank-core2-master PRIVATE
    src
    ${FW_ROOT}/common
    ${FW_ROOT}/drivers
)

pico_generate_pio_header(frank-core2-master ${FW_ROOT}/common/link_bus.pio)
pico_generate_pio_header(frank-core2-master ${FW_ROOT}/common/ws2812.pio)
pico_generate_pio_header(frank-core2-master ${FW_ROOT}/drivers/audio_i2s.pio)

target_compile_definitions(frank-core2-master PRIVATE
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
    SLAVE_RESET_BODGE=$<BOOL:${SLAVE_RESET_BODGE}>
    FS_RESET_SELFTEST=$<BOOL:${FS_RESET_SELFTEST}>

    # HDMI_hstx.c and the pico_hdmi subtree key off this.
    HDMI_HSTX=1
    # heartbeat.c only compiles the WS2812 path when asked, so the same
    # source file also serves the slave's plain-LED build.
    HEARTBEAT_WS2812=1

    # The SD card driver and audio driver are standalone: they take
    # their pins as -D flags rather than including the board header.
    SDCARD_PIN_SPI0_CS=5
    SDCARD_PIN_SPI0_SCK=6
    SDCARD_PIN_SPI0_MOSI=7
    SDCARD_PIN_SPI0_MISO=4
    I2S_DATA_PIN=9
    I2S_CLOCK_PIN_BASE=10
)

target_compile_options(frank-core2-master PRIVATE
    -O2
    -Wall
    -Wno-unused-parameter
    -Wno-unused-function
)

target_link_libraries(frank-core2-master
    pico_stdlib
    pico_multicore
    hardware_clocks
    hardware_dma
    hardware_flash
    hardware_gpio
    hardware_pio
    hardware_pwm
    hardware_spi
    hardware_sync
    hardware_vreg
    hardware_xip_cache
    fatfs
    sdcard
    pico_hdmi
    usbhid
)

# USB HID host needs exclusive use of the USB controller, so the CDC
# console moves to UART0 on the J2 header when it is enabled.
# These take a literal 0/1 at configure time — a generator expression
# reaches pico_enable_stdio_usb() as an unevaluated string and it ends
# up enabling the CDC stdio it was meant to switch off, which then
# collides with the TinyUSB host build.
if(USB_HID_ENABLED)
    pico_enable_stdio_uart(frank-core2-master 1)
    pico_enable_stdio_usb(frank-core2-master  0)
else()
    pico_enable_stdio_uart(frank-core2-master 0)
    pico_enable_stdio_usb(frank-core2-master  1)
endif()

pico_add_extra_outputs(frank-core2-master)
