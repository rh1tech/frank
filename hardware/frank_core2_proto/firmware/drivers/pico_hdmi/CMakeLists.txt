#
# frank-msx — fMSX for RP2350
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank-msx
# SPDX-License-Identifier: GPL-3.0-or-later
#

add_library(pico_hdmi INTERFACE)

# Select compile-time or runtime video mode API
if (PICO_HDMI_RUNTIME_MODES)
    target_sources(pico_hdmi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/video_output_rt.c
    )
else()
    target_sources(pico_hdmi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/video_output.c
    )
endif()

target_sources(pico_hdmi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/hstx_packet.c
    ${CMAKE_CURRENT_LIST_DIR}/hstx_data_island_queue.c
)

target_include_directories(pico_hdmi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(pico_hdmi INTERFACE
    pico_stdlib
    hardware_clocks
    hardware_dma
    hardware_gpio
    hardware_irq
)
