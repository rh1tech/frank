#
# FRANK Core 2 — dual-RP2350 test firmware
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank
# SPDX-License-Identifier: GPL-3.0-or-later
#
# sdk_env.sh — locate the Pico SDK. Sourced by every build script.
#
# An exported PICO_SDK_PATH wins, but only if it actually contains an
# SDK; a stale export pointing at a moved or empty directory is a common
# way to get a confusing CMake failure, so we fall back to the usual
# install locations instead of trusting it blindly.

_sdk_is_valid() {
    [[ -n "${1:-}" && -f "$1/pico_sdk_init.cmake" ]]
}

if ! _sdk_is_valid "${PICO_SDK_PATH:-}"; then
    for _candidate in \
        "$HOME/pico/pico-sdk" \
        "$HOME/pico-sdk" \
        "$HOME/Documents/pico/pico-sdk" \
        "/usr/local/pico-sdk" \
        "/opt/pico-sdk"
    do
        if _sdk_is_valid "$_candidate"; then
            export PICO_SDK_PATH="$_candidate"
            break
        fi
    done
fi

if ! _sdk_is_valid "${PICO_SDK_PATH:-}"; then
    echo "ERROR: could not find the Pico SDK." >&2
    echo "       Set PICO_SDK_PATH to a checkout containing pico_sdk_init.cmake." >&2
    exit 1
fi

echo "Pico SDK: $PICO_SDK_PATH"
