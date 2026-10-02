#!/bin/bash
#
# FRANK Core 2 — dual-RP2350 test firmware
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#
# build_all.sh — build both halves with matching options.
#
# The two firmwares must agree on CPU_SPEED: the link receiver's PIO
# loop has to keep up with the transmitter's byte period, and both are
# derived from each chip's own system clock. Building them separately at
# different speeds is the easiest way to get a link that half works, so
# this script exists to make the matched build the default path.
#
set -euo pipefail

cd "$(dirname "$0")"

export USB_HID="${USB_HID:-0}"
export CPU_SPEED="${CPU_SPEED:-252}"
export PSRAM_SPEED="${PSRAM_SPEED:-133}"
export FLASH_SPEED="${FLASH_SPEED:-66}"
export CLEAN="${CLEAN:-0}"

echo "=== FRANK Core 2 firmware ==="
echo "  CPU ${CPU_SPEED} MHz   PSRAM ${PSRAM_SPEED} MHz   flash ${FLASH_SPEED} MHz   USB_HID=${USB_HID}"
echo

echo "--- master (RP2350B) ---"
./master/build.sh

echo
echo "--- slave (RP2350A) ---"
./slave/build.sh

echo
echo "Both firmwares built:"
echo "  master/build/frank-core2-master.uf2"
echo "  slave/build/frank-core2-slave.uf2"
