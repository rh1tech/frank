/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * FRANK NES - NES Emulator for RP2350
 * Originally from pico_hdmi by fliperama86 (https://github.com/fliperama86/pico_hdmi)
 * https://rh1.tech | https://github.com/rh1tech/frank-nes
 * SPDX-License-Identifier: Unlicense
 */

#include "pico_hdmi/hstx_data_island_queue.h"

#include <string.h>

#include "pico.h"
#include "hardware/sync.h"

#define DI_RING_BUFFER_SIZE 512
static hstx_data_island_t di_ring_buffer[DI_RING_BUFFER_SIZE];
static volatile uint32_t di_ring_head = 0;
static volatile uint32_t di_ring_tail = 0;

// Single pre-encoded silent audio packet (fixed B-frame flags).
static hstx_data_island_t silence_packet;

// Audio timing state (default 48kHz, 525 lines for 480p)
static uint32_t audio_sample_accum = 0; // Fixed-point accumulator (now Bresenham style)
static uint32_t cached_v_total_lines = 525;
#define DEFAULT_SAMPLES_PER_FRAME (48000 / 60)
static uint32_t samples_per_frame = DEFAULT_SAMPLES_PER_FRAME;

// Limit accumulator to avoid overflow if we run dry.
// Limit to 2 packets worth of accumulation so it catches up properly but doesn't overflow.
static uint32_t max_audio_accum = (8 * 525);

void hstx_di_queue_init(void)
{
    di_ring_head = 0;
    di_ring_tail = 0;
    audio_sample_accum = 0;
    // Build a single silent audio packet.
    hstx_packet_t packet;
    audio_sample_t samples[4] = {0};
    (void)hstx_packet_set_audio_samples(&packet, samples, 4, 0);
    hstx_encode_data_island(&silence_packet, &packet, false, true);
}

void hstx_di_queue_set_sample_rate(uint32_t sample_rate)
{
    samples_per_frame = sample_rate / 60;
}

void hstx_di_queue_set_v_total(uint32_t v_total)
{
    cached_v_total_lines = v_total;
    max_audio_accum = 8 * v_total;
}

void hstx_di_queue_set_samples_per_line_fp(uint32_t value)
{
    // Legacy support, try to deduce samples_per_frame from line_fp
    samples_per_frame = (value * cached_v_total_lines) >> 16;
}

bool __not_in_flash("audio") hstx_di_queue_push(const hstx_data_island_t *island)
{
    uint32_t next_head = (di_ring_head + 1) % DI_RING_BUFFER_SIZE;
    if (next_head == di_ring_tail)
        return false;

    di_ring_buffer[di_ring_head] = *island;
    di_ring_head = next_head;
    return true;
}

uint32_t __not_in_flash("audio") hstx_di_queue_get_level(void)
{
    uint32_t head = di_ring_head;
    uint32_t tail = di_ring_tail;
    if (head >= tail)
        return head - tail;
    return DI_RING_BUFFER_SIZE + head - tail;
}

void __scratch_x("") hstx_di_queue_tick(void)
{
    audio_sample_accum += samples_per_frame;
    if (audio_sample_accum > max_audio_accum) {
        audio_sample_accum = max_audio_accum;
    }
}

/* Debug: track silence fallback hits. Check via hstx_di_queue_get_underrun_count() */
static volatile uint32_t audio_underrun_count = 0;

uint32_t hstx_di_queue_get_underrun_count(void) { return audio_underrun_count; }

const uint32_t *__scratch_x("") hstx_di_queue_get_audio_packet(void)
{
    uint32_t threshold = 4 * cached_v_total_lines;
    if (audio_sample_accum >= threshold) {
        audio_sample_accum -= threshold;
        if (di_ring_tail != di_ring_head) {
            const uint32_t *words = di_ring_buffer[di_ring_tail].words;
            di_ring_tail = (di_ring_tail + 1) % DI_RING_BUFFER_SIZE;
            return words;
        }
        audio_underrun_count++;
        return silence_packet.words;
    }
    return NULL;
}

void __not_in_flash("audio") hstx_di_queue_update_silence(int frame_counter)
{
    hstx_packet_t packet;
    audio_sample_t samples[4] = {0};
    (void)hstx_packet_set_audio_samples(&packet, samples, 4, frame_counter);
    hstx_encode_data_island(&silence_packet, &packet, false, true);
}
