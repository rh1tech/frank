/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * frank-snes - I2S Audio Driver with Chained Double Buffer DMA
 * Based on murmgenesis audio driver by Mikhail Matveev
 *
 * Uses two DMA channels in ping-pong configuration:
 * - Channel A plays buffer 0, then triggers channel B
 * - Channel B plays buffer 1, then triggers channel A
 *
 * Each channel completion raises DMA_IRQ_1; the IRQ handler re-arms the
 * completed channel (reset read addr + transfer count) and marks its buffer
 * free for the CPU to refill.
 *
 * No pico-extras dependency - direct PIO + DMA.
 */

#include "audio.h"
#include "board_config.h"
#include "audio_i2s.pio.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

//=============================================================================
// State - Chained double buffer (ping-pong) DMA
//=============================================================================

// NOTE: HDMI uses DMA_IRQ_1 with an exclusive handler.
// Audio uses DMA_IRQ_0 to avoid conflicts.
#define AUDIO_DMA_IRQ DMA_IRQ_0

// Fixed DMA channels for audio (keep away from dynamically-claimed HDMI channels)
#define AUDIO_DMA_CH_A 10
#define AUDIO_DMA_CH_B 11

#define DMA_BUFFER_COUNT 2
// Max buffer size in stereo frames (32-bit words).
// SNES at 32kHz/60fps = 533 frames; generous headroom.
#define DMA_BUFFER_MAX_SAMPLES 600

static uint32_t __attribute__((aligned(4))) dma_buffers[DMA_BUFFER_COUNT][DMA_BUFFER_MAX_SAMPLES];

// Bitmask of buffers the CPU is allowed to write (1 = free)
static volatile uint32_t dma_buffers_free_mask = 0;

// Pre-roll: fill both buffers before starting playback
#define PREROLL_BUFFERS 2
static volatile int preroll_count = 0;

static int dma_channel_a = -1;
static int dma_channel_b = -1;
static PIO audio_pio;
static uint audio_sm;
static uint32_t dma_transfer_count;

static volatile bool audio_running = false;

static void audio_dma_irq_handler(void);

//=============================================================================
// I2S Implementation
//=============================================================================

i2s_config_t i2s_get_default_config(void) {
    i2s_config_t config = {
        .sample_freq = 18000,
        .channel_count = 2,
        .data_pin = I2S_DATA_PIN,
        .clock_pin_base = I2S_CLOCK_PIN_BASE,
        .pio = pio1,   // PIO1 - HDMI uses PIO0
        .sm = 0,
        .dma_channel = 0,
        .dma_trans_count = 300,
        .dma_buf = NULL,
        .volume = 0,
    };
    return config;
}

void i2s_init(i2s_config_t *config) {
    audio_pio = config->pio;
    dma_transfer_count = config->dma_trans_count;

    // Determine GPIO function based on which PIO we're using
    uint8_t func = (config->pio == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1;
    gpio_set_function(config->data_pin, func);
    gpio_set_function(config->clock_pin_base, func);
    gpio_set_function(config->clock_pin_base + 1, func);

    gpio_set_drive_strength(config->data_pin, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(config->clock_pin_base, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(config->clock_pin_base + 1, GPIO_DRIVE_STRENGTH_12MA);

    // Claim state machine
    audio_sm = pio_claim_unused_sm(audio_pio, true);
    config->sm = audio_sm;

    // Add PIO program
    uint offset = pio_add_program(audio_pio, &audio_i2s_program);
    audio_i2s_program_init(audio_pio, audio_sm, offset,
                           config->data_pin, config->clock_pin_base);

    // Drain the TX FIFO
    pio_sm_clear_fifos(audio_pio, audio_sm);

    // Set clock divider for sample rate
    uint32_t sys_clk = clock_get_hz(clk_sys);
    uint32_t divider = sys_clk * 4 / config->sample_freq;
    pio_sm_set_clkdiv_int_frac(audio_pio, audio_sm, divider >> 8u, divider & 0xffu);

    // Validate transfer count fits our static buffers
    dma_transfer_count = config->dma_trans_count;
    if (dma_transfer_count == 0) dma_transfer_count = 1;
    if (dma_transfer_count > DMA_BUFFER_MAX_SAMPLES) dma_transfer_count = DMA_BUFFER_MAX_SAMPLES;
    config->dma_trans_count = (uint16_t)dma_transfer_count;

    // Initialize DMA buffers with silence
    memset(dma_buffers, 0, sizeof(dma_buffers));
    config->dma_buf = (uint16_t *)(void *)dma_buffers[0];

    // Clear audio DMA IRQ flags (IRQ1)
    dma_hw->ints0 = (1u << AUDIO_DMA_CH_A) | (1u << AUDIO_DMA_CH_B);

    // Use fixed DMA channels for audio
    dma_channel_abort(AUDIO_DMA_CH_A);
    dma_channel_abort(AUDIO_DMA_CH_B);
    while (dma_channel_is_busy(AUDIO_DMA_CH_A) || dma_channel_is_busy(AUDIO_DMA_CH_B)) {
        tight_loop_contents();
    }

    dma_channel_unclaim(AUDIO_DMA_CH_A);
    dma_channel_unclaim(AUDIO_DMA_CH_B);
    dma_channel_claim(AUDIO_DMA_CH_A);
    dma_channel_claim(AUDIO_DMA_CH_B);
    dma_channel_a = AUDIO_DMA_CH_A;
    dma_channel_b = AUDIO_DMA_CH_B;
    config->dma_channel = (uint8_t)dma_channel_a;

    // Configure DMA channels in ping-pong chain
    dma_channel_config cfg_a = dma_channel_get_default_config(dma_channel_a);
    channel_config_set_read_increment(&cfg_a, true);
    channel_config_set_write_increment(&cfg_a, false);
    channel_config_set_transfer_data_size(&cfg_a, DMA_SIZE_32);
    channel_config_set_dreq(&cfg_a, pio_get_dreq(audio_pio, audio_sm, true));
    channel_config_set_chain_to(&cfg_a, dma_channel_b);

    dma_channel_config cfg_b = dma_channel_get_default_config(dma_channel_b);
    channel_config_set_read_increment(&cfg_b, true);
    channel_config_set_write_increment(&cfg_b, false);
    channel_config_set_transfer_data_size(&cfg_b, DMA_SIZE_32);
    channel_config_set_dreq(&cfg_b, pio_get_dreq(audio_pio, audio_sm, true));
    channel_config_set_chain_to(&cfg_b, dma_channel_a);

    dma_channel_configure(
        dma_channel_a, &cfg_a,
        &audio_pio->txf[audio_sm], dma_buffers[0], dma_transfer_count, false);

    dma_channel_configure(
        dma_channel_b, &cfg_b,
        &audio_pio->txf[audio_sm], dma_buffers[1], dma_transfer_count, false);

    // Set up DMA IRQ1 handler (avoid HDMI's DMA_IRQ_0 exclusive handler)
    irq_set_exclusive_handler(AUDIO_DMA_IRQ, audio_dma_irq_handler);
    irq_set_priority(AUDIO_DMA_IRQ, 0x80);
    irq_set_enabled(AUDIO_DMA_IRQ, true);

    // Enable IRQ1 for both channels
    dma_hw->ints0 = (1u << dma_channel_a) | (1u << dma_channel_b);
    dma_channel_set_irq0_enabled(dma_channel_a, true);
    dma_channel_set_irq0_enabled(dma_channel_b, true);

    // Enable PIO state machine
    pio_sm_set_enabled(audio_pio, audio_sm, true);

    // Initialize state
    preroll_count = 0;
    dma_buffers_free_mask = (1u << DMA_BUFFER_COUNT) - 1u; // both free
    audio_running = false;
}

void i2s_write(const i2s_config_t *config, const int16_t *samples, const size_t len) {
    for (size_t i = 0; i < len; i++) {
        pio_sm_put_blocking(config->pio, config->sm, (uint32_t)samples[i]);
    }
}

void i2s_dma_write(i2s_config_t *config, const int16_t *samples) {
    // Wait for a free buffer, then claim it (atomically vs DMA IRQ)
    uint8_t buf_index = 0;
    while (true) {
        uint32_t irq_state = save_and_disable_interrupts();
        uint32_t free_mask = dma_buffers_free_mask;

        if (!audio_running) {
            // Pre-roll fills buffer 0 then buffer 1 to preserve ordering
            buf_index = (uint8_t)preroll_count;
            if (buf_index < DMA_BUFFER_COUNT && (free_mask & (1u << buf_index))) {
                dma_buffers_free_mask &= ~(1u << buf_index);
                restore_interrupts(irq_state);
                break;
            }
        } else {
            if (free_mask) {
                buf_index = (free_mask & 1u) ? 0 : 1;
                dma_buffers_free_mask &= ~(1u << buf_index);
                restore_interrupts(irq_state);
                break;
            }
        }

        restore_interrupts(irq_state);
        tight_loop_contents();
    }

    uint32_t *dst = dma_buffers[buf_index];
    const uint32_t *src = (const uint32_t *)samples;
    const uint shift = config->volume;

    if (shift == 0) {
        memcpy(dst, src, dma_transfer_count * sizeof(uint32_t));
    } else {
#ifdef PICO_ON_DEVICE
        audio_volume_shift(dst, src, dma_transfer_count, shift);
#else
        for (uint32_t i = 0; i < dma_transfer_count; i++) {
            uint32_t v = src[i];
            int16_t l = (int16_t)(v >> 16);
            int16_t r = (int16_t)(v & 0xFFFF);
            l >>= shift;
            r >>= shift;
            dst[i] = ((uint32_t)(uint16_t)l << 16) | (uint16_t)r;
        }
#endif
    }

    // Memory barrier to ensure writes are visible before DMA reads
    __dmb();

    if (!audio_running) {
        preroll_count++;
        if (preroll_count >= PREROLL_BUFFERS) {
            // Both buffers are filled and queued; start playback on channel A
            dma_channel_start(dma_channel_a);
            audio_running = true;
        }
    }
}

void i2s_volume(i2s_config_t *config, uint8_t volume) {
    if (volume > 16) volume = 16;
    config->volume = volume;
}

void i2s_increase_volume(i2s_config_t *config) {
    if (config->volume > 0) config->volume--;
}

void i2s_decrease_volume(i2s_config_t *config) {
    if (config->volume < 16) config->volume++;
}

//=============================================================================
// DMA IRQ Handler
//=============================================================================

static void audio_dma_irq_handler(void) {
    uint32_t ints = dma_hw->ints0;
    uint32_t mask = 0;
    if (dma_channel_a >= 0) mask |= (1u << dma_channel_a);
    if (dma_channel_b >= 0) mask |= (1u << dma_channel_b);
    ints &= mask;
    if (!ints) return;

    if ((dma_channel_a >= 0) && (ints & (1u << dma_channel_a))) {
        dma_hw->ints0 = (1u << dma_channel_a);
        dma_channel_set_read_addr(dma_channel_a, dma_buffers[0], false);
        dma_channel_set_trans_count(dma_channel_a, dma_transfer_count, false);
        dma_buffers_free_mask |= 1u;
    }

    if ((dma_channel_b >= 0) && (ints & (1u << dma_channel_b))) {
        dma_hw->ints0 = (1u << dma_channel_b);
        dma_channel_set_read_addr(dma_channel_b, dma_buffers[1], false);
        dma_channel_set_trans_count(dma_channel_b, dma_transfer_count, false);
        dma_buffers_free_mask |= 2u;
    }
}
