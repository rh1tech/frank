/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * mem_test.h — flash and PSRAM exercises shared by both MCUs.
 *
 * Both halves of the board carry the same parts (W25Q128 16 MB QSPI
 * flash + ESP-PSRAM64H 8 MB PSRAM), so one implementation covers both;
 * the slave ships its results back to the master over the link and the
 * master renders them side by side.
 *
 * ORDERING CONSTRAINT: mem_test_flash_identify() briefly takes the QMI
 * out of XIP mode to issue a raw 0x9F. It must therefore run while the
 * other core is parked — on the master that means before graphics_init()
 * launches core 1 for HSTX scanout. The throughput and pattern tests
 * below are plain loads/stores through the XIP windows and are safe to
 * run at any time.
 */
#ifndef MEM_TEST_H
#define MEM_TEST_H

#include <stdbool.h>
#include <stdint.h>

#include "link_proto.h"

/* Read the flash JEDEC ID (0x9F) and the 64-bit unique ID.
 * Must run with the other core parked. */
void mem_test_flash_identify(uint32_t *jedec_id, uint8_t unique_id[8]);

/* Capacity implied by the JEDEC ID's third byte (2^N bytes). Returns 0
 * when the ID looks implausible. */
uint32_t mem_test_flash_capacity(uint32_t jedec_id);

/* Bring up PSRAM on `cs_pin` and work out how much is actually there by
 * looking for address aliasing. Returns 0 if no PSRAM responded. */
uint32_t mem_test_psram_probe(uint32_t cs_pin);

/* Sequential read throughput of the flash XIP window, in KiB/s. Reads
 * `bytes` starting at the base of XIP; the region is far larger than
 * the 8 KiB XIP cache so this measures the QMI, not the cache. */
uint32_t mem_test_flash_read_kbps(uint32_t bytes);

/* CRC-32 over the first `bytes` of the flash XIP window. Two runs on
 * the same firmware must agree — a mismatch means unstable QSPI timing. */
uint32_t mem_test_flash_crc(uint32_t bytes);

/* Full write-then-verify sweep of `psram_bytes` with an LFSR pattern,
 * timing both passes. `bit_errors` counts mismatching bytes. Returns
 * true when every byte read back matched. */
bool mem_test_psram_sweep(uint32_t psram_bytes,
                          uint32_t *write_kbps, uint32_t *read_kbps,
                          uint32_t *byte_errors);

/* Run the whole battery and fill in a result struct for the link. */
void mem_test_run_all(link_mem_result_t *out,
                      uint32_t flash_bytes, uint32_t psram_bytes);

#endif /* MEM_TEST_H */
