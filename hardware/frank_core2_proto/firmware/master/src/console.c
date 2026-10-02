/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "console.h"
#include "ui_draw.h"
#include "HDMI.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Character grid. Rasterised into `fb` on demand. */
static char    grid[CONSOLE_ROWS][CONSOLE_COLS];
static uint8_t attr[CONSOLE_ROWS][CONSOLE_COLS];
static uint8_t fb[CONSOLE_FB_WIDTH * CONSOLE_FB_HEIGHT];

static int log_row = CONSOLE_LOG_TOP;

static const uint32_t palette[] = {
    [C_BG]     = 0x0A0E14,   /* near-black with a blue cast */
    [C_TEXT]   = 0xD8DEE9,
    [C_TITLE]  = 0x5FD7D7,
    [C_OK]     = 0x6FD86F,
    [C_FAIL]   = 0xF2585B,
    [C_WARN]   = 0xF0B429,
    [C_DIM]    = 0x74808F,
    [C_ACCENT] = 0xC792EA,
};

void console_init(void) {
    for (size_t i = 0; i < sizeof(palette) / sizeof(palette[0]); i++)
        graphics_set_palette((uint8_t)i, palette[i]);
    graphics_set_bgcolor(palette[C_BG]);

    graphics_set_res(CONSOLE_FB_WIDTH, CONSOLE_FB_HEIGHT);
    graphics_set_buffer(fb);

    console_clear();
    console_flush();
}

void console_clear(void) {
    memset(grid, ' ', sizeof(grid));
    memset(attr, C_TEXT, sizeof(attr));
    log_row = CONSOLE_LOG_TOP;
}

void console_clear_row(int row) {
    if (row < 0 || row >= CONSOLE_ROWS) return;
    memset(grid[row], ' ', CONSOLE_COLS);
    memset(attr[row], C_TEXT, CONSOLE_COLS);
}

/* Place a formatted string into the grid, clipping at the right edge. */
static void put_at(int row, int col, uint8_t color, const char *s) {
    if (row < 0 || row >= CONSOLE_ROWS) return;
    for (int i = 0; s[i] && col + i < CONSOLE_COLS; i++) {
        if (col + i < 0) continue;
        grid[row][col + i] = s[i];
        attr[row][col + i] = color;
    }
}

void console_at(int row, int col, uint8_t color, const char *fmt, ...) {
    char buf[CONSOLE_COLS + 1];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    put_at(row, col, color, buf);
    printf("%s\n", buf);
}

void console_at_quiet(int row, int col, uint8_t color, const char *s) {
    put_at(row, col, color, s);
}

void console_log(uint8_t color, const char *fmt, ...) {
    char buf[CONSOLE_COLS + 1];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (log_row >= CONSOLE_ROWS) {
        /* Scroll the log region up by one line. */
        for (int r = CONSOLE_LOG_TOP; r < CONSOLE_ROWS - 1; r++) {
            memcpy(grid[r], grid[r + 1], CONSOLE_COLS);
            memcpy(attr[r], attr[r + 1], CONSOLE_COLS);
        }
        log_row = CONSOLE_ROWS - 1;
        console_clear_row(log_row);
    }

    put_at(log_row, 0, color, buf);
    log_row++;

    printf("%s\n", buf);
    console_flush();
}

void console_rule(int row, uint8_t color) {
    if (row < 0 || row >= CONSOLE_ROWS) return;
    memset(grid[row], '-', CONSOLE_COLS);
    memset(attr[row], color, CONSOLE_COLS);
}

void console_flush(void) {
    memset(fb, C_BG, sizeof(fb));

    for (int r = 0; r < CONSOLE_ROWS; r++) {
        int y = r * UI_CHAR_H;
        for (int c = 0; c < CONSOLE_COLS; c++) {
            char ch = grid[r][c];
            if (ch == ' ') continue;
            ui_draw_char(fb, CONSOLE_FB_WIDTH, c * UI_CHAR_W, y, ch, attr[r][c]);
        }
    }
}
