#!/bin/bash
#
# FRANK Core 2 — dual-RP2350 test firmware
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#
# swd_flash.sh — flash over SWD with a Raspberry Pi Debug Probe.
#
# Strongly preferred over flash.sh once a probe is attached. USB-BOOTSEL
# flashing needs a button press whenever the firmware wedges — which is
# exactly when you are iterating fastest — and `picotool reboot -u` is
# unreliable against a target that has faulted into lockup. SWD does not
# care what the target is doing.
#
# Wiring: probe SWD to J1 (master, U3) or J3 (slave, U6). Both headers
# are pin 1 = SWDIO, pin 2 = GND, pin 3 = SWCLK.
#
# Usage: ./swd_flash.sh master|slave [--reset-only]
#
set -euo pipefail

cd "$(dirname "$0")"

TARGET="${1:-master}"
case "$TARGET" in
    master) ELF="master/build/frank-core2-master.elf" ;;
    slave)  ELF="slave/build/frank-core2-slave.elf" ;;
    *) echo "usage: $0 master|slave [--reset-only]" >&2; exit 1 ;;
esac

OPENOCD_ARGS=(-f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2350.cfg)

if [[ "${2:-}" == "--reset-only" ]]; then
    exec openocd "${OPENOCD_ARGS[@]}" -c "init" -c "reset run" -c "exit"
fi

if [[ ! -f "$ELF" ]]; then
    echo "ERROR: $ELF not found. Run ./build_all.sh first." >&2
    exit 1
fi

echo "Flashing $TARGET over SWD: $ELF"
openocd "${OPENOCD_ARGS[@]}" -c "program $ELF verify reset exit"
