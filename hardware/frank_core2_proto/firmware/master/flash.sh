#!/bin/bash
#
# FRANK Core 2 — dual-RP2350 test firmware
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#
# flash.sh — flash the master (RP2350B) over its own USB-C port (J8).
#
# Put the master into BOOTSEL first: hold S1 (BOOT), tap S2 (RUN), let
# go of S1. The two MCUs enumerate as separate devices, so make sure the
# cable is in J8 and not the slave's J9 — see ../README.md.
#
set -euo pipefail

cd "$(dirname "$0")"

FIRMWARE="${1:-build/frank-core2-master.uf2}"

if [[ ! -f "$FIRMWARE" ]]; then
    echo "ERROR: $FIRMWARE not found. Run ./build.sh first." >&2
    exit 1
fi

echo "Flashing master: $FIRMWARE"
picotool load -f "$FIRMWARE"
picotool reboot -f
