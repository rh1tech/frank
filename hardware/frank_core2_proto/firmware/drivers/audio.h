/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * frank-snes - Chained Double-Buffer I2S Audio Driver for RP2350
 * Based on murmgenesis audio driver (PIO + DMA ping-pong, no pico-extras)
 */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>

// Assembly optimized volume shift
#ifdef PICO_ON_DEVICE
void audio_volume_shift(uint32_t* dst, const uint32_t* src,
                        uint32_t frames, uint32_t shift);
#endif

typedef struct i2s_config
{
    uint32_t sample_freq;
    uint16_t channel_count;
    uint8_t  data_pin;
    uint8_t  clock_pin_base;
    PIO      pio;
    uint8_t  sm;
    uint8_t  dma_channel;
    uint16_t dma_trans_count;
    uint16_t *dma_buf;
    uint8_t  volume;  // 0 = max volume, higher = quieter (shift amount)
} i2s_config_t;

i2s_config_t i2s_get_default_config(void);
void i2s_init(i2s_config_t *config);
void i2s_write(const i2s_config_t *config, const int16_t *samples, const size_t len);
void i2s_dma_write(i2s_config_t *config, const int16_t *samples);
void i2s_volume(i2s_config_t *config, uint8_t volume);
void i2s_increase_volume(i2s_config_t *config);
void i2s_decrease_volume(i2s_config_t *config);

#endif // AUDIO_H
