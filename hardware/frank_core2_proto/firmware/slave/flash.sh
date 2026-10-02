#!/bin/bash
#
# FRANK Core 2 — dual-RP2350 test firmware
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#
# flash.sh — flash the slave (RP2350) over its own USB-C port (J9).
#
# Put the slave into BOOTSEL first: hold S3 (BOOT), tap S4 (RUN), let go
# of S3. The master cannot reset the slave — GPIO43 is labelled as slave
# reset in the schematic but only reaches a pull-up, so the slave must
# always be reset by hand. See ../README.md.
#
set -euo pipefail

cd "$(dirname "$0")"

FIRMWARE="${1:-build/frank-core2-slave.uf2}"

if [[ ! -f "$FIRMWARE" ]]; then
    echo "ERROR: $FIRMWARE not found. Run ./build.sh first." >&2
    exit 1
fi

echo "Flashing slave: $FIRMWARE"
picotool load -f "$FIRMWARE"
picotool reboot -f
