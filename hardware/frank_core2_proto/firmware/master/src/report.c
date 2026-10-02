/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "report.h"

#include <stdio.h>

/* All formatting goes through integer maths. Pulling in the soft-float
 * printf for a handful of one-decimal figures would cost several KB of
 * flash and make the numbers no more accurate.
 *
 * Rates are binary (MiB/s) and labelled as such. They used to say "MB/s"
 * while dividing by 1048576, which made every measurement look 4.8%
 * below the predicted decimal-MB figure and sent me hunting for a
 * shortfall that did not exist. sys_clk/4 = 63.0 MB/s = 60.1 MiB/s. */

void report_format_size(char *buf, uint32_t buf_len, uint64_t bytes) {
    if (bytes == 0) {
        snprintf(buf, buf_len, "-");
    } else if (bytes >= 1024u * 1024u) {
        uint64_t mb10 = (bytes * 10u) / (1024u * 1024u);
        if (mb10 % 10 == 0) snprintf(buf, buf_len, "%u MB", (unsigned)(mb10 / 10));
        else snprintf(buf, buf_len, "%u.%u MB", (unsigned)(mb10 / 10), (unsigned)(mb10 % 10));
    } else if (bytes >= 1024u) {
        snprintf(buf, buf_len, "%u KB", (unsigned)(bytes / 1024u));
    } else {
        snprintf(buf, buf_len, "%u B", (unsigned)bytes);
    }
}

void report_format_rate(char *buf, uint32_t buf_len,
                        uint64_t bytes, uint32_t elapsed_us) {
    if (!elapsed_us) {
        snprintf(buf, buf_len, "-");
        return;
    }
    /* bytes/us * 1e6 = bytes/s, then scaled by 10 for one decimal.
     * Done in one expression so the truncation happens once, at the end. */
    uint64_t tenths = (bytes * 10000000ull) / ((uint64_t)elapsed_us * 1048576ull);
    snprintf(buf, buf_len, "%u.%u MiB/s",
             (unsigned)(tenths / 10), (unsigned)(tenths % 10));
}

void report_format_kbps(char *buf, uint32_t buf_len, uint32_t kbps) {
    if (!kbps) {
        snprintf(buf, buf_len, "-");
        return;
    }
    uint32_t tenths = (uint32_t)(((uint64_t)kbps * 10u) / 1024u);
    snprintf(buf, buf_len, "%u.%u MiB/s", (unsigned)(tenths / 10), (unsigned)(tenths % 10));
}

void report_format_bps(char *buf, uint32_t buf_len, uint32_t bytes_per_s) {
    if (!bytes_per_s) {
        snprintf(buf, buf_len, "-");
        return;
    }
    uint32_t tenths = (uint32_t)(((uint64_t)bytes_per_s * 10u) / 1048576u);
    snprintf(buf, buf_len, "%u.%u MiB/s", (unsigned)(tenths / 10), (unsigned)(tenths % 10));
}
