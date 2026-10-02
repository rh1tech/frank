/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "heartbeat.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#ifdef HEARTBEAT_WS2812
#include "ws2812.pio.h"
#endif

/* 40 ms tick: fast enough for a smooth WS2812 breathe, slow enough that
 * the IRQ is invisible next to a 63 MB/s DMA. */
#define HB_TICK_MS 40

static volatile hb_state_t hb_state = HB_BOOT;
static repeating_timer_t   hb_timer;
static uint32_t            hb_phase;

static bool hb_use_ws2812;
static uint hb_pin;

#ifdef HEARTBEAT_WS2812
static PIO  hb_pio;
static uint hb_sm;
#endif

/* Blink period in ticks for the plain-LED path, indexed by state.
 * 0 means "solid on". */
static const uint8_t hb_period[] = {
    [HB_BOOT]    = 25,   /* 1 s   — slow, obviously alive but idle   */
    [HB_TEST]    = 5,    /* 200 ms — busy                            */
    [HB_WAITING] = 25,
    [HB_OK]      = 12,   /* ~500 ms                                  */
    [HB_ERROR]   = 2,    /* 80 ms — frantic, hard to miss            */
};

/* Base colours for the WS2812 path, GRB order as the part expects. */
static uint32_t hb_colour(hb_state_t s, uint8_t level) {
    uint8_t r = 0, g = 0, b = 0;
    switch (s) {
    case HB_BOOT:    b = level; break;                 /* blue    */
    case HB_TEST:    r = level; g = level; break;      /* amber   */
    case HB_WAITING: r = level / 3; g = 0; b = level; break; /* violet */
    case HB_OK:      g = level; break;                 /* green   */
    case HB_ERROR:   r = level; break;                 /* red     */
    }
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

static bool hb_tick(repeating_timer_t *t) {
    (void)t;
    hb_phase++;

    hb_state_t s = hb_state;

    if (hb_use_ws2812) {
#ifdef HEARTBEAT_WS2812
        /* Triangle-wave breathe over 32 ticks (~1.3 s). HB_ERROR skips
         * the ramp and hard-blinks so it reads as a fault, not a mood. */
        uint8_t level;
        if (s == HB_ERROR) {
            level = (hb_phase & 2) ? 90 : 0;
        } else {
            uint32_t p = hb_phase & 31;
            uint32_t tri = p < 16 ? p : 31 - p;      /* 0..15 */
            level = (uint8_t)(4 + tri * 4);          /* 4..64, gentle */
        }
        /* Non-blocking on purpose. This runs in a timer IRQ, and
         * pio_sm_put_blocking() would spin inside the handler with
         * interrupts masked if the state machine ever stopped draining
         * its FIFO — deadlocking the whole chip, USB included, over a
         * status LED. A dropped frame just means one skipped tick. */
        if (!pio_sm_is_tx_fifo_full(hb_pio, hb_sm))
            pio_sm_put(hb_pio, hb_sm, hb_colour(s, level) << 8u);
#endif
    } else {
        uint8_t period = hb_period[s];
        gpio_put(hb_pin, (hb_phase % period) < (period / 2));
    }
    return true;
}

static void hb_start_timer(void) {
    add_repeating_timer_ms(-HB_TICK_MS, hb_tick, NULL, &hb_timer);
}

void heartbeat_init_ws2812(void *pio, uint32_t pin) {
#ifdef HEARTBEAT_WS2812
    hb_use_ws2812 = true;
    hb_pio = (PIO)pio;
    hb_pin = pin;

#if PICO_PIO_USE_GPIO_BASE
    /* LD1 is on GPIO46, outside the default 0..31 PIO window. */
    if (pin > 31) pio_set_gpio_base(hb_pio, 16);
#endif

    hb_sm  = pio_claim_unused_sm(hb_pio, true);

    uint offset = pio_add_program(hb_pio, &ws2812_program);
    ws2812_program_init(hb_pio, hb_sm, offset, pin);

    hb_start_timer();
#else
    (void)pio; (void)pin;
#endif
}

void heartbeat_init_gpio(uint32_t pin) {
    hb_use_ws2812 = false;
    hb_pin = pin;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);

    hb_start_timer();
}

void heartbeat_set(hb_state_t state) { hb_state = state; }
hb_state_t heartbeat_get(void)       { return hb_state;  }
