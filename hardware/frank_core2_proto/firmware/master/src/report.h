/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * report.h — fixed screen layout for the diagnostic report.
 *
 * main.c owns the identity and peripheral rows, diag_link.c owns the
 * link table. Keeping the row numbers in one header is what stops the
 * two from quietly overwriting each other as the report grows.
 */
#ifndef REPORT_H
#define REPORT_H

#include <stdint.h>

/* Column origins inside the 53-character grid. */
#define R_LABEL_COL   0
#define R_MASTER_COL  14
#define R_SLAVE_COL   34

/* Identity / memory block — one row per fact, master and slave side by
 * side so a mismatch between the two halves is obvious at a glance. */
#define R_TITLE        0
#define R_RULE_TOP     1
#define R_COLHDR       2
#define R_CHIP_ID      3
#define R_PACKAGE      4
#define R_SYSCLK       5
#define R_FLASH_ID     6
#define R_FLASH_READ   7
#define R_FLASH_CRC    8
#define R_PSRAM_SIZE   9
#define R_PSRAM_WRITE 10
#define R_PSRAM_READ  11
#define R_PSRAM_ERR   12

#define R_RULE_MID    13
#define R_PERIPH      14   /* SD / USB / audio — master only */

#define R_RULE_LINK   15
#define R_LINK_HDR    16
#define R_LINK_ROW0   17   /* four sweep rows: 17..20 */
#define R_LINK_SWEEPS  4
#define R_LINK_VERIFY 21
#define R_VERDICT     22
#define R_RULE_BOT    23

/* Human-readable size, e.g. "16 MB" / "8 MB" / "512 KB". */
void report_format_size(char *buf, uint32_t buf_len, uint64_t bytes);

/* Throughput as "63.0 MB/s", from a byte count and an elapsed time. */
void report_format_rate(char *buf, uint32_t buf_len,
                        uint64_t bytes, uint32_t elapsed_us);

/* Throughput as "63.0 MB/s" straight from a KiB/s figure. */
void report_format_kbps(char *buf, uint32_t buf_len, uint32_t kbps);

/* Throughput as "63.0 MB/s" from a bytes-per-second figure. */
void report_format_bps(char *buf, uint32_t buf_len, uint32_t bytes_per_s);

#endif /* REPORT_H */
