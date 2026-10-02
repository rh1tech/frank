#!/bin/bash
#
# FRANK Core 2 — dual-RP2350 test firmware
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#
# build.sh — build the FRANK Core 2 master firmware (RP2350B, U3).
#
# Env vars:
#   USB_HID      0 = USB CDC console on J8 (default, best for bring-up)
#                1 = USB HID host keyboard on J8, console on UART0 (J2)
#   CPU_SPEED    system clock in MHz (default 252)
#   PSRAM_SPEED  PSRAM clock ceiling in MHz (default 133)
#   FLASH_SPEED  flash clock ceiling in MHz (default 66)
#   CLEAN        1 = wipe the build directory first
#
set -euo pipefail

cd "$(dirname "$0")"
source ../sdk_env.sh

: "${USB_HID:=0}"
: "${CPU_SPEED:=252}"
: "${PSRAM_SPEED:=133}"
: "${FLASH_SPEED:=66}"
: "${CLEAN:=0}"

case "$USB_HID" in
    1|ON|on|yes|true) USB_HID_CMAKE=ON ;;
    *)                USB_HID_CMAKE=OFF ;;
esac

if [[ "$CLEAN" == "1" ]]; then
    rm -rf build
fi
mkdir -p build

cmake -S . -B build \
    -DPICO_PLATFORM=rp2350 \
    -DCPU_SPEED="${CPU_SPEED}" \
    -DPSRAM_SPEED="${PSRAM_SPEED}" \
    -DFLASH_SPEED="${FLASH_SPEED}" \
    -DUSB_HID_ENABLED="${USB_HID_CMAKE}"

cmake --build build -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

echo
echo "Master firmware: $(pwd)/build/frank-core2-master.uf2"
