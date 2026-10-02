/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * HDMI_hstx.c — adapter that exposes the same `graphics_*` API as the
 * PIO HDMI driver (HDMI.c) but drives the HSTX peripheral via the copy
 * of the murmnes/frank-nes pico_hdmi driver (verbatim).
 *
 * Frank-MSX renders into an 8-bit indexed framebuffer. This file owns a
 * 256-entry RGB565-packed palette and registers a scanline callback with
 * video_output.c that converts the indexed line into the RGB565 line
 * buffer HSTX expects. The MSX framebuffer is centred (letter-boxed) in
 * the 640x480 HSTX output.
 */

#include "HDMI.h"
#include "pico_hdmi/video_output.h"
#include "pico_hdmi/hstx_data_island_queue.h"
#include "pico_hdmi/hstx_packet.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include <string.h>

/* ---- Same externally-visible globals the frank-msx code pokes. ------- */
int graphics_buffer_width  = 320;
int graphics_buffer_height = 240;
int graphics_buffer_shift_x = 0;
int graphics_buffer_shift_y = 0;
enum graphics_mode_t hdmi_graphics_mode = GRAPHICSMODE_DEFAULT;

/* ---- Framebuffer + palette owned by this driver ---------------------- */
static uint8_t *graphics_buffer = NULL;
static uint8_t *pending_buffer  = NULL;

/* Pre-doubled RGB565 palette: pal_32[idx] = (px<<16) | px, so a scanline
 * can write 32 bits at a time and satisfy MODE_H_ACTIVE_PIXELS / 2 words. */
static uint32_t pal_32[256];
static uint32_t pal_raw_rgb888[256];

static bool crt_active = false;
static bool greyscale_active = false;

/* Output dimensions (640x480 by default, from video_output.h). */
#define HSTX_OUT_W MODE_H_ACTIVE_PIXELS  /* 640 */
#define HSTX_OUT_H MODE_V_ACTIVE_LINES   /* 480 */

/* ---- RGB888 → RGB565 helper ------------------------------------------ */
static inline uint16_t rgb888_to_rgb565(uint32_t c888) {
    uint8_t r = (c888 >> 16) & 0xff;
    uint8_t g = (c888 >>  8) & 0xff;
    uint8_t b =  c888        & 0xff;
    return (uint16_t)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

static inline uint32_t rgb888_to_grey888(uint32_t c888) {
    uint8_t r = (c888 >> 16) & 0xff;
    uint8_t g = (c888 >>  8) & 0xff;
    uint8_t b =  c888        & 0xff;
    uint8_t Y = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
    return ((uint32_t)Y << 16) | ((uint32_t)Y << 8) | Y;
}

/* ---- Public API mirror ---------------------------------------------- */
void graphics_set_buffer(uint8_t *buffer) {
    pending_buffer = buffer;
}

uint8_t *graphics_get_buffer(void) {
    return graphics_buffer ? graphics_buffer : pending_buffer;
}

uint32_t graphics_get_width(void)  { return (uint32_t)graphics_buffer_width;  }
uint32_t graphics_get_height(void) { return (uint32_t)graphics_buffer_height; }

void graphics_set_res(int w, int h) {
    graphics_buffer_width  = w;
    graphics_buffer_height = h;
}

void graphics_set_shift(int x, int y) {
    graphics_buffer_shift_x = x;
    graphics_buffer_shift_y = y;
}

void graphics_set_palette(uint8_t i, uint32_t color888) {
    pal_raw_rgb888[i] = color888 & 0x00ffffff;
    uint32_t c = greyscale_active ? rgb888_to_grey888(color888) : color888;
    uint32_t p = rgb888_to_rgb565(c);
    pal_32[i] = p | (p << 16);
}

uint32_t graphics_get_palette(uint8_t i) {
    return pal_raw_rgb888[i];
}

void graphics_set_mode(enum graphics_mode_t mode) {
    hdmi_graphics_mode = mode;
}

void graphics_set_bgcolor(uint32_t color888) {
    /* Reuse slot 0 as background on the HSTX path. */
    graphics_set_palette(0, color888);
}

void graphics_restore_sync_colors(void) {
    /* HSTX doesn't reserve indices for sync — nothing to do. */
}

void graphics_set_crt_active(bool active) { crt_active       = active; }
bool graphics_get_crt_active(void)        { return crt_active;         }

void graphics_set_greyscale(bool active) {
    if (greyscale_active == active) return;
    greyscale_active = active;
    /* Rebuild palette from the raw RGB888 cache. */
    for (int i = 0; i < 256; i++) {
        uint32_t c = greyscale_active ? rgb888_to_grey888(pal_raw_rgb888[i])
                                      : pal_raw_rgb888[i];
        uint32_t p = rgb888_to_rgb565(c);
        pal_32[i] = p | (p << 16);
    }
}
bool graphics_get_greyscale(void) { return greyscale_active; }

struct video_mode_t graphics_get_video_mode(int mode) {
    (void)mode;
    struct video_mode_t vm = {
        .h_total = MODE_H_TOTAL_PIXELS,
        .h_width = MODE_H_ACTIVE_PIXELS,
        .freq = 60,
        .vgaPxClk = 25200000,
    };
    return vm;
}

/* ====================================================================
 * Scanline callback — convert one row of the indexed MSX framebuffer
 * into an RGB565 line for HSTX. Buffer is 640x480; MSX frame is
 * typically 272x228 centred vertically/horizontally.
 * ==================================================================== */
static void __not_in_flash("scanline") scanline_cb(
        uint32_t v_scanline, uint32_t active_line, uint32_t *dst)
{
    (void)v_scanline;

    if (!graphics_buffer) return;

    const int words_total = HSTX_OUT_W / 2;
    int fb_w = graphics_buffer_width;
    int fb_h = graphics_buffer_height;
    if (fb_w <= 0 || fb_h <= 0) return;

    /* Vertical 2× pixel-double: 228 MSX lines → 456 output lines
     * inside 480 (12 HSTX px top/bottom border). */
    int scaled_h = fb_h * 2;
    int y_off = (HSTX_OUT_H - scaled_h) / 2;
    int fb_y = ((int)active_line - y_off) / 2;
    if (fb_y < 0 || fb_y >= fb_h) return;

    /* CRT effect — blank every other scaled line. */
    if (crt_active && (((int)active_line - y_off) & 1)) return;

    const uint8_t *src = graphics_buffer + fb_y * fb_w;

    /* Horizontal 2× pixel-double, centred. */
    int scaled_w = fb_w * 2;
    int x_off = (HSTX_OUT_W - scaled_w) / 2;
    if (x_off < 0) x_off = 0;

    /* Only write the content region. Border regions (dst[0..x_off/2-1]
     * and dst[x_off/2+fb_w..words_total-1]) are left untouched — they
     * stay at their BSS-init zero (black) value for the lifetime of the
     * program. Unconditionally refilling them every scanline with a
     * palette colour created visible left-edge ticks whenever pal_32[0]
     * drifted from the MSX's own BG color. Mirrors murmnes's approach. */
    uint32_t *out = dst + (x_off / 2);
    int pairs = fb_w;
    if ((x_off / 2) + pairs > words_total)
        pairs = words_total - (x_off / 2);

    for (int x = 0; x < pairs; x++) {
        out[x] = pal_32[src[x]];
    }
}

/* Vsync callback: swap graphics_buffer to the pending pointer during
 * vblank so scanout never mid-frames. */
static void __not_in_flash("vsync") vsync_cb(void) {
    if (pending_buffer && pending_buffer != graphics_buffer) {
        graphics_buffer = pending_buffer;
    } else if (!graphics_buffer && pending_buffer) {
        graphics_buffer = pending_buffer;
    }
}

/* ====================================================================
 * graphics_init — bring up HSTX + audio queue, launch core1 scanout.
 * ==================================================================== */
void graphics_init(g_out out) {
    (void)out;   /* HSTX path is HDMI-only; VGA is not supported here. */

    /* Seed palette to black + a visible default so the first frame isn't
     * undefined. */
    for (int i = 0; i < 256; i++) {
        pal_raw_rgb888[i] = 0;
        pal_32[i] = 0;
    }

    /* If graphics_set_buffer() was called before init, pick it up. */
    if (pending_buffer && !graphics_buffer) graphics_buffer = pending_buffer;

    video_output_set_vsync_callback(vsync_cb);
    video_output_set_scanline_callback(scanline_cb);
    video_output_init(HSTX_OUT_W, HSTX_OUT_H);

    /* Keep HDMI DI scheduler happy — audio packets flow from WriteAudio. */
    extern void hstx_di_queue_init(void);
    hstx_di_queue_init();

    /* Let video_output set its audio sample rate to match fMSX (22050 Hz). */
    extern void pico_hdmi_set_audio_sample_rate(uint32_t rate);
    pico_hdmi_set_audio_sample_rate(22050);

    /* Force clk_hstx = 126 MHz — the pixel-clock expected by the copied
     * HSTX driver (25.2 MHz after the CSR divider of 5). Matches murmnes
     * main_pico.c's post-init clock_configure() override. */
    clock_configure(clk_hstx, 0,
                    CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    clock_get_hz(clk_sys), 126000000);

    /* Hand core1 over to HSTX scanout. Audio must run on core0. */
    multicore_launch_core1(video_output_core1_run);
}

/* ====================================================================
 * HDMI audio push — copied verbatim from murmnes main_pico.c's
 * hdmi_push_samples(). Encodes mono int16 samples into HDMI data-island
 * packets and enqueues them for the HSTX scanline dispatcher.
 * ==================================================================== */
static int audio_frame_counter = 0;
static int16_t audio_carry[3];
static int audio_carry_count = 0;

void __not_in_flash("audio") hdmi_hstx_push_samples(const int16_t *buf, int count)
{
    int16_t merged[4];
    int pos = 0;

    if (audio_carry_count > 0) {
        for (int i = 0; i < audio_carry_count; i++)
            merged[i] = audio_carry[i];
        int need = 4 - audio_carry_count;
        if (need > count) need = count;
        for (int i = 0; i < need; i++)
            merged[audio_carry_count + i] = buf[i];
        pos = need;
        if (audio_carry_count + need == 4) {
            audio_sample_t samples[4];
            for (int i = 0; i < 4; i++) {
                samples[i].left = merged[i];
                samples[i].right = merged[i];
            }
            hstx_packet_t packet;
            int new_fc = hstx_packet_set_audio_samples(&packet, samples, 4, audio_frame_counter);
            hstx_data_island_t island;
            hstx_encode_data_island(&island, &packet, false, true);
            if (!hstx_di_queue_push(&island)) {
                return;
            }
            audio_frame_counter = new_fc;
        }
        audio_carry_count = 0;
    }

    while (pos + 4 <= count) {
        audio_sample_t samples[4];
        for (int i = 0; i < 4; i++) {
            samples[i].left = buf[pos + i];
            samples[i].right = buf[pos + i];
        }
        hstx_packet_t packet;
        int new_fc = hstx_packet_set_audio_samples(&packet, samples, 4, audio_frame_counter);
        hstx_data_island_t island;
        hstx_encode_data_island(&island, &packet, false, true);
        if (!hstx_di_queue_push(&island)) {
            break;
        }
        audio_frame_counter = new_fc;
        pos += 4;
    }

    audio_carry_count = count - pos;
    if (audio_carry_count > 3) {
        audio_carry_count = count % 4;
        pos = count - audio_carry_count;
    }
    for (int i = 0; i < audio_carry_count; i++)
        audio_carry[i] = buf[pos + i];
    hstx_di_queue_update_silence(audio_frame_counter);
}

void hdmi_hstx_fill_silence(int count)
{
    for (int i = 0; i < count / 4; i++) {
        audio_sample_t samples[4] = {0};
        hstx_packet_t packet;
        audio_frame_counter = hstx_packet_set_audio_samples(&packet, samples, 4, audio_frame_counter);
        hstx_data_island_t island;
        hstx_encode_data_island(&island, &packet, false, true);
        hstx_di_queue_push(&island);
    }
}
