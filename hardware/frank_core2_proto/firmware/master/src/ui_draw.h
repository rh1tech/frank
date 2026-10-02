/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * ui_draw.h — 8-bit indexed framebuffer drawing primitives for the
 *             MSX loader overlay. Palette indices are reserved via
 *             ui_draw_install_palette() so they don't collide with
 *             fMSX's own palette writes.
 */
#ifndef UI_DRAW_H
#define UI_DRAW_H

#include <stdint.h>
#include <stdbool.h>

/* Font */
#define UI_FONT_GLYPHS       95   /* ASCII 32..126 */
#define UI_FONT_GLYPH_BYTES   8   /* 8 rows */
#define UI_CHAR_W             6   /* cell width  (6 bits used of 8) */
#define UI_CHAR_H             8   /* cell height */

extern const uint8_t ui_font_6x8[UI_FONT_GLYPHS][UI_FONT_GLYPH_BYTES];

/* Palette slots we claim for the UI. Values are palette indices in
 * the HDMI driver's lookup table. We stay below the HDMI sync-control
 * region (250..253) and above fMSX's 16-color palette (0..15). */
#define UI_COLOR_BG      247   /* window background */
#define UI_COLOR_FG      246   /* primary text / border */
#define UI_COLOR_ACCENT  245   /* titlebar fill (inverse) */
#define UI_COLOR_ACCENT_FG 244 /* titlebar text */
#define UI_COLOR_DIM     243   /* scrollbar track */

/* Call once from the UI init path; pushes the reserved RGB colors
 * into the HDMI driver's palette so the overlay is legible regardless
 * of what fMSX did with indices 0..15. */
void ui_draw_install_palette(void);

/* Low-level primitives. `fb` points at the current front/back buffer
 * (uint8_t stride `stride`, total height `height`). */
void ui_fill_rect(uint8_t *fb, int stride, int x, int y, int w, int h, uint8_t color);
void ui_draw_border(uint8_t *fb, int stride, int x, int y, int w, int h, uint8_t color);
void ui_draw_char(uint8_t *fb, int stride, int x, int y, char c, uint8_t color);
void ui_draw_string(uint8_t *fb, int stride, int x, int y, const char *s, uint8_t color);
void ui_draw_string_truncated(uint8_t *fb, int stride, int x, int y,
                              const char *s, int max_chars, uint8_t color);

/* Mac-style inverted titlebar at (x,y) of width `w` and height
 * UI_HEADER_H, with a centered title. */
#define UI_HEADER_H 12
void ui_draw_header(uint8_t *fb, int stride, int x, int y, int w, const char *title);

/* Selectable list entry at (x,y), width `w`, height UI_LINE_H. When
 * `selected` is true the entry is drawn inverted. */
#define UI_LINE_H 10
void ui_draw_menu_item(uint8_t *fb, int stride, int x, int y, int w,
                       const char *text, int max_chars, bool selected);

/* Vertical scrollbar track + thumb. No-op if visible >= total. */
void ui_draw_scrollbar(uint8_t *fb, int stride, int x, int y, int h,
                       int total, int visible, int scroll_pos);

#endif /* UI_DRAW_H */
