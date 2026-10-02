/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * ui_draw.c — 8-bit framebuffer drawing primitives for the loader UI.
 *
 * Ported and simplified from murmapple's disk_ui.c. The murmapple
 * version wrote 4bpp-packed pixels; our HDMI driver is 8bpp indexed,
 * so every routine here reduces to a plain byte store.
 */

#include "ui_draw.h"
#include "HDMI.h"
#include <string.h>

void ui_draw_install_palette(void) {
    graphics_set_palette(UI_COLOR_BG,        0x20283c);  /* deep blue-grey */
    graphics_set_palette(UI_COLOR_FG,        0xe8e8e8);  /* near-white */
    graphics_set_palette(UI_COLOR_ACCENT,    0xe8e8e8);  /* titlebar fill */
    graphics_set_palette(UI_COLOR_ACCENT_FG, 0x20283c);  /* titlebar text */
    graphics_set_palette(UI_COLOR_DIM,       0x606878);  /* scrollbar track */
}

static inline void put_pixel(uint8_t *fb, int stride, int x, int y, uint8_t color) {
    fb[(size_t)y * (size_t)stride + (size_t)x] = color;
}

void ui_fill_rect(uint8_t *fb, int stride, int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; ++row) {
        uint8_t *p = fb + (size_t)(y + row) * (size_t)stride + (size_t)x;
        memset(p, color, (size_t)w);
    }
}

void ui_draw_border(uint8_t *fb, int stride, int x, int y, int w, int h, uint8_t color) {
    ui_fill_rect(fb, stride, x,         y,         w, 1, color);
    ui_fill_rect(fb, stride, x,         y + h - 1, w, 1, color);
    ui_fill_rect(fb, stride, x,         y,         1, h, color);
    ui_fill_rect(fb, stride, x + w - 1, y,         1, h, color);
}

void ui_draw_char(uint8_t *fb, int stride, int x, int y, char c, uint8_t color) {
    int idx = (unsigned char)c - 32;
    if (idx < 0 || idx >= UI_FONT_GLYPHS) return;
    const uint8_t *glyph = ui_font_6x8[idx];
    for (int row = 0; row < UI_CHAR_H; ++row) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < UI_CHAR_W; ++col) {
            if (bits & (0x80u >> col))
                put_pixel(fb, stride, x + col, y + row, color);
        }
    }
}

void ui_draw_string(uint8_t *fb, int stride, int x, int y, const char *s, uint8_t color) {
    while (*s) {
        ui_draw_char(fb, stride, x, y, *s, color);
        x += UI_CHAR_W;
        ++s;
    }
}

void ui_draw_string_truncated(uint8_t *fb, int stride, int x, int y,
                              const char *s, int max_chars, uint8_t color) {
    if (max_chars <= 3) { ui_draw_string(fb, stride, x, y, s, color); return; }
    int len = (int)strlen(s);
    if (len <= max_chars) {
        ui_draw_string(fb, stride, x, y, s, color);
        return;
    }
    for (int i = 0; i < max_chars - 3; ++i)
        ui_draw_char(fb, stride, x + i * UI_CHAR_W, y, s[i], color);
    ui_draw_string(fb, stride, x + (max_chars - 3) * UI_CHAR_W, y, "...", color);
}

void ui_draw_header(uint8_t *fb, int stride, int x, int y, int w, const char *title) {
    ui_fill_rect(fb, stride, x, y, w, UI_HEADER_H, UI_COLOR_ACCENT);
    int tlen = (int)strlen(title);
    int tx = x + (w - tlen * UI_CHAR_W) / 2;
    int ty = y + (UI_HEADER_H - UI_CHAR_H) / 2;
    ui_draw_string(fb, stride, tx, ty, title, UI_COLOR_ACCENT_FG);
}

void ui_draw_menu_item(uint8_t *fb, int stride, int x, int y, int w,
                       const char *text, int max_chars, bool selected) {
    if (selected) {
        ui_fill_rect(fb, stride, x, y, w, UI_LINE_H, UI_COLOR_ACCENT);
        ui_draw_string_truncated(fb, stride, x + 2, y + 1, text, max_chars, UI_COLOR_ACCENT_FG);
    } else {
        ui_fill_rect(fb, stride, x, y, w, UI_LINE_H, UI_COLOR_BG);
        ui_draw_string_truncated(fb, stride, x + 2, y + 1, text, max_chars, UI_COLOR_FG);
    }
}

void ui_draw_scrollbar(uint8_t *fb, int stride, int x, int y, int h,
                       int total, int visible, int scroll_pos) {
    if (total <= visible) return;
    ui_fill_rect(fb, stride, x, y, 4, h, UI_COLOR_DIM);
    int thumb_h = (h * visible) / total;
    if (thumb_h < 8) thumb_h = 8;
    int max_scroll = total - visible;
    int thumb_y = (max_scroll > 0) ? (y + ((h - thumb_h) * scroll_pos) / max_scroll) : y;
    ui_fill_rect(fb, stride, x, thumb_y, 4, thumb_h, UI_COLOR_FG);
}
