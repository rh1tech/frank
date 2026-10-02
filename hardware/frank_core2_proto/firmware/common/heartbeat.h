/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * heartbeat.h — "this MCU is still executing" indicator.
 *
 * Driven from a repeating timer IRQ rather than the main loop, so it
 * keeps beating while the foreground is parked inside a link handshake
 * or a multi-second PSRAM sweep. If the LED stops, the core is wedged —
 * that distinction is the whole point, so do not move it into a polled
 * update.
 *
 * Master: WS2812B (LD1, GPIO46) — colour carries the state.
 * Slave:  plain blue LED (LD2, GPIO26) — blink rate carries the state.
 */
#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HB_BOOT = 0,   /* powering up / peripherals not probed yet */
    HB_TEST,       /* self-test or link test in progress       */
    HB_WAITING,    /* idle, waiting for the peer               */
    HB_OK,         /* everything passed                        */
    HB_ERROR,      /* something failed — see the console       */
} hb_state_t;

/* Master: WS2812B on `pin`, driven from `pio`. Claims one SM. */
void heartbeat_init_ws2812(void *pio, uint32_t pin);

/* Slave: plain active-high LED on `pin`. */
void heartbeat_init_gpio(uint32_t pin);

void heartbeat_set(hb_state_t state);
hb_state_t heartbeat_get(void);

#endif /* HEARTBEAT_H */
