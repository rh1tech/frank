#
# frank-msx — fMSX for RP2350
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank-msx
# SPDX-License-Identifier: GPL-3.0-or-later
#

add_library(fatfs INTERFACE)
target_sources(fatfs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/ffsystem.c
)
target_include_directories(fatfs INTERFACE ${CMAKE_CURRENT_LIST_DIR})
