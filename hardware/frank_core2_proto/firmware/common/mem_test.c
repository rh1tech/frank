/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mem_test.h"
#include "frank_core2_board.h"
#include "psram_init.h"

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"
#include "hardware/xip_cache.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <string.h>

/* Cached XIP windows — what ordinary code sees, and therefore what the
 * throughput figures should reflect. */
#define FLASH_XIP  ((volatile const uint32_t *)FRANK_XIP_FLASH_BASE)
#define PSRAM_XIP  ((volatile uint32_t *)FRANK_XIP_PSRAM_BASE)

/* Uncached alias, used only by the probes below. Reading back through
 * the cache would answer from the 8 KiB XIP cache instead of the device,
 * which defeats both a presence check and an aliasing check. */
#define PSRAM_RAW  ((volatile uint32_t *)FRANK_XIP_PSRAM_NOCACHE_BASE)

/* How much of each device the throughput probes touch. 1 MiB is two
 * orders of magnitude bigger than the 8 KiB XIP cache, so cache hits
 * contribute well under 1% and the number really is device bandwidth. */
#define THROUGHPUT_BYTES (1024u * 1024u)

/* ------------------------------------------------------------------ */
/* Flash                                                              */
/* ------------------------------------------------------------------ */

void mem_test_flash_identify(uint32_t *jedec_id, uint8_t unique_id[8]) {
    /* 0x9F + three ID bytes. flash_do_cmd drops out of XIP for the
     * duration, so this must not race code executing from flash on the
     * other core — see the ordering note in mem_test.h. */
    uint8_t tx[4] = { 0x9F, 0, 0, 0 };
    uint8_t rx[4] = { 0 };

    uint32_t save = save_and_disable_interrupts();
    flash_do_cmd(tx, rx, 4);
    restore_interrupts(save);

    if (jedec_id)
        *jedec_id = ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];

    if (unique_id) {
        save = save_and_disable_interrupts();
        flash_get_unique_id(unique_id);
        restore_interrupts(save);
    }
}

uint32_t mem_test_flash_capacity(uint32_t jedec_id) {
    uint32_t cap = jedec_id & 0xFFu;
    /* Winbond encodes capacity as log2(bytes); 0x18 = 16 MB for the
     * W25Q128 on this board. Anything outside 64 KB..64 MB is junk. */
    if (cap < 16 || cap > 26) return 0;
    return 1u << cap;
}

uint32_t mem_test_flash_read_kbps(uint32_t bytes) {
    volatile uint32_t sink = 0;
    uint32_t words = bytes / 4;

    absolute_time_t t0 = get_absolute_time();
    for (uint32_t i = 0; i < words; i += 8) {
        /* Eight words per iteration keeps the loop overhead well below
         * the XIP fill time so the measurement tracks the QMI. */
        sink += FLASH_XIP[i + 0] + FLASH_XIP[i + 1]
              + FLASH_XIP[i + 2] + FLASH_XIP[i + 3]
              + FLASH_XIP[i + 4] + FLASH_XIP[i + 5]
              + FLASH_XIP[i + 6] + FLASH_XIP[i + 7];
    }
    int64_t us = absolute_time_diff_us(t0, get_absolute_time());
    (void)sink;

    if (us <= 0) return 0;
    /* KiB/s: scale to microseconds first, then divide by 1024 so the
     * result is a binary rate consistent with how the sizes are shown. */
    return (uint32_t)(((uint64_t)bytes * 1000000ull) / 1024ull / (uint64_t)us);
}

uint32_t mem_test_flash_crc(uint32_t bytes) {
    return link_crc32((const void *)FRANK_XIP_FLASH_BASE, bytes);
}

/* ------------------------------------------------------------------ */
/* PSRAM                                                              */
/* ------------------------------------------------------------------ */

uint32_t mem_test_psram_probe(uint32_t cs_pin) {
    psram_init(cs_pin);

    /* Presence: a chip that isn't there won't hold two complementary
     * patterns. Two values rather than one, so a bus stuck high or low
     * cannot pass. */
    const uint32_t canary = 0x5A3CC35Au;
    PSRAM_RAW[0] = canary;
    PSRAM_RAW[1] = ~canary;
    __dsb();
    if (PSRAM_RAW[0] != canary || PSRAM_RAW[1] != ~canary) return 0;

    /* Size: walk power-of-two offsets looking for the write to alias
     * back onto offset 0, which is what a part smaller than the address
     * space does when the high address bits fall off the end. The
     * ESP-PSRAM64H on this board should report 8 MB. */
    for (uint32_t mb = 1; mb <= 32; mb <<= 1) {
        uint32_t word_off = (mb * 1024u * 1024u) / 4u;

        PSRAM_RAW[0] = canary;
        __dsb();
        PSRAM_RAW[word_off] = mb;
        __dsb();

        /* Aliased onto offset 0, or simply didn't stick: either way the
         * part does not extend this far. */
        if (PSRAM_RAW[0] != canary || PSRAM_RAW[word_off] != mb)
            return mb * 1024u * 1024u;
    }

    /* Never aliased within 32 MB — larger than anything this board
     * carries, so report the ceiling rather than guessing. */
    return 32u * 1024u * 1024u;
}

bool mem_test_psram_sweep(uint32_t psram_bytes,
                          uint32_t *write_kbps, uint32_t *read_kbps,
                          uint32_t *byte_errors) {
    uint32_t words = psram_bytes / 4;
    uint32_t errors = 0;

    /* Write pass — LFSR generated inline so we never need an 8 MB
     * reference buffer in SRAM. */
    uint32_t s = 0xC0FFEEu;
    absolute_time_t t0 = get_absolute_time();
    for (uint32_t i = 0; i < words; i++) {
        uint32_t lsb = s & 1u;
        s >>= 1;
        if (lsb) s ^= 0xA3000000u;
        PSRAM_XIP[i] = s;
    }
    __dsb();
    int64_t wus = absolute_time_diff_us(t0, get_absolute_time());

    /* Push every dirty line out to the part and drop the clean copies,
     * so the read-back below genuinely re-fetches from PSRAM instead of
     * grading the cache's homework. Done between the two timed regions
     * so it distorts neither figure. */
    xip_cache_clean_all();
    xip_cache_invalidate_all();

    /* Read-back pass — regenerate the same sequence and compare. The
     * LFSR step is a handful of cycles, far cheaper than the PSRAM
     * access it is checking, so the timing still reflects the device. */
    s = 0xC0FFEEu;
    t0 = get_absolute_time();
    for (uint32_t i = 0; i < words; i++) {
        uint32_t lsb = s & 1u;
        s >>= 1;
        if (lsb) s ^= 0xA3000000u;
        uint32_t got = PSRAM_XIP[i];
        if (got != s) {
            uint32_t diff = got ^ s;
            for (int b = 0; b < 4; b++)
                if ((diff >> (b * 8)) & 0xFFu) errors++;
        }
    }
    int64_t rus = absolute_time_diff_us(t0, get_absolute_time());

    if (write_kbps)
        *write_kbps = wus > 0 ? (uint32_t)(((uint64_t)psram_bytes * 1000000ull) / 1024ull / (uint64_t)wus) : 0;
    if (read_kbps)
        *read_kbps  = rus > 0 ? (uint32_t)(((uint64_t)psram_bytes * 1000000ull) / 1024ull / (uint64_t)rus) : 0;
    if (byte_errors) *byte_errors = errors;

    return errors == 0;
}

/* ------------------------------------------------------------------ */

void mem_test_run_all(link_mem_result_t *out,
                      uint32_t flash_bytes, uint32_t psram_bytes) {
    memset(out, 0, sizeof(*out));

    out->flash_bytes     = flash_bytes;
    out->flash_read_kbps = mem_test_flash_read_kbps(THROUGHPUT_BYTES);
    out->flash_crc       = mem_test_flash_crc(64u * 1024u);
    /* A second CRC over the same region must agree; if it doesn't, the
     * QSPI read timing is marginal and every other number is suspect. */
    out->flash_ok        = (flash_bytes != 0) &&
                           (out->flash_crc == mem_test_flash_crc(64u * 1024u));

    out->psram_bytes = psram_bytes;
    if (psram_bytes) {
        /* Collect into locals: link_mem_result_t is packed for the wire,
         * so pointers into it are not guaranteed word-aligned. */
        uint32_t w = 0, r = 0, errs = 0;
        bool ok = mem_test_psram_sweep(psram_bytes, &w, &r, &errs);

        out->psram_write_kbps = w;
        out->psram_read_kbps  = r;
        out->psram_bit_errors = errs;
        out->psram_ok         = ok ? 1 : 0;
    } else {
        out->psram_ok = 0;
    }
}
