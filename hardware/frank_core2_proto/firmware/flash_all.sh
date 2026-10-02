#!/bin/bash
#
# FRANK Core 2 — dual-RP2350 test firmware
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#
# flash_all.sh — walk through flashing both MCUs.
#
# There is no way to automate this end to end. The two RP2350s have
# separate USB-C ports (J8 master, J9 slave) and separate BOOTSEL/RUN
# button pairs, and nothing on the board lets one reset the other, so
# the operator has to move the cable and press the buttons. This script
# just sequences the prompts so neither half gets forgotten.
#
set -euo pipefail

cd "$(dirname "$0")"

prompt() {
    echo
    echo "$1"
    read -r -p "Press Enter when ready (or Ctrl-C to stop)... " _
}

prompt "SLAVE:  connect USB-C to J9, hold S3 (BOOT), tap S4 (RUN), release S3."
./slave/flash.sh

prompt "MASTER: connect USB-C to J8, hold S1 (BOOT), tap S2 (RUN), release S1."
./master/flash.sh

echo
echo "Both flashed. Reset the slave first (S4), then the master (S2) — the"
echo "master reports the link as down if the slave is not already serving"
echo "when the sweep starts."
