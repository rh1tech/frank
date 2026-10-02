/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * console.h — character-grid console on the master's HDMI output.
 *
 * The HSTX driver scans out an 8-bit indexed framebuffer and pixel-
 * doubles it into 640x480, so the console works in a 320x240 buffer:
 * 53 columns by 30 rows of the 6x8 font borrowed from frank-msx.
 *
 * The screen is split in two. Rows 0..CONSOLE_LOG_TOP-1 are addressed
 * directly (console_at) and hold the static report — board identity,
 * master and slave peripheral results, the link results table. Rows
 * from CONSOLE_LOG_TOP down scroll (console_log) and hold progress
 * messages. Everything is written into a character grid first and
 * rasterised by console_flush(), so a partially-built report never
 * reaches the display.
 *
 * Every console_* text call also mirrors to stdio, so the USB CDC or
 * UART console shows the same run without a second set of format
 * strings to keep in sync.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#define CONSOLE_FB_WIDTH   320
#define CONSOLE_FB_HEIGHT  240
#define CONSOLE_COLS       53
#define CONSOLE_ROWS       30
#define CONSOLE_LOG_TOP    24   /* first scrolling row */

/* Palette slots the console claims. Index 0 is the background. */
enum {
    C_BG     = 0,
    C_TEXT   = 1,   /* off-white body text     */
    C_TITLE  = 2,   /* cyan headings           */
    C_OK     = 3,   /* green pass              */
    C_FAIL   = 4,   /* red fail                */
    C_WARN   = 5,   /* amber caution           */
    C_DIM    = 6,   /* grey labels             */
    C_ACCENT = 7,   /* magenta highlights      */
};

/* Sets the video resolution, installs the palette and hands the
 * framebuffer to the HDMI driver. Call after graphics_init(). */
void console_init(void);

void console_clear(void);

/* Write into the fixed report region. Clipped to the grid. */
void console_at(int row, int col, uint8_t color, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

/* Clear a row in the fixed region (useful when a line shrinks). */
void console_clear_row(int row);

/* Screen-only variant of console_at — does not mirror to stdio. For
 * placeholder text that would otherwise flood the serial log with
 * lines the reader does not need. */
void console_at_quiet(int row, int col, uint8_t color, const char *s);

/* Append to the scrolling log region. */
void console_log(uint8_t color, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Horizontal rule across the full width at `row`. */
void console_rule(int row, uint8_t color);

/* Rasterise the grid into the framebuffer. Cheap enough to call after
 * every logical update — a full 320x240 repaint is well under a
 * millisecond at 252 MHz. */
void console_flush(void);

#endif /* CONSOLE_H */
