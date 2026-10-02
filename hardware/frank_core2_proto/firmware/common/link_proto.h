/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * link_proto.h — the message layer both MCUs speak over link_bus.
 *
 * Every control exchange is a fixed 128-byte frame so neither side ever
 * has to negotiate a length: the receiver arms for exactly
 * LINK_CTRL_BYTES and the sender writes exactly that many. Bulk data
 * transfers are separate — they carry no header at all, just
 * LINK_BULK_BYTES of pattern, because the whole point is to measure the
 * wire and not the framing.
 *
 * Sequencing is doorbell-driven so the two sides never have to agree on
 * absolute timing:
 *
 *   master                              slave
 *   ------                              -----
 *   DB_MS = 1  ------------------------>  sees DB_MS, arms RX
 *   waits for DB_SM  <-----------------   DB_SM = 1  ("armed")
 *   VALID_A=1, stream, VALID_A=0
 *   DB_MS = 0  ------------------------>  RX DMA completes
 *                    <-----------------   DB_SM = 0
 *
 * The reply travels the same way with the roles of the doorbells kept
 * as-is (DB_MS is always master-driven), so the master arms its receiver
 * before raising DB_MS for a reply-bearing opcode.
 */
#ifndef LINK_PROTO_H
#define LINK_PROTO_H

#include <stdint.h>

#define LINK_MAGIC        0x324B5246u   /* "FRK2" little-endian */
#define LINK_PROTO_VER    1u

/* Fixed control-frame size. Must be a multiple of 4 (PIO autopush is
 * 32-bit) and large enough for the biggest payload struct below. */
#define LINK_CTRL_BYTES   128u

/* Bulk block size used by every throughput measurement. 32 KiB keeps
 * both a TX pattern and an RX landing buffer comfortably in SRAM on
 * either chip while still being ~500 us per block at full rate, which
 * is long enough that per-block DMA restart overhead is under 0.1%. */
#define LINK_BULK_BYTES   (32u * 1024u)

/* ---- Opcodes ---- */
enum {
    LINK_OP_HELLO        = 0x0001,  /* M->S, payload: none              */
    LINK_OP_HELLO_ACK    = 0x0002,  /* S->M, payload: link_node_info_t  */

    LINK_OP_SELFTEST     = 0x0010,  /* M->S, run local memory tests     */
    LINK_OP_SELFTEST_ACK = 0x0011,  /* S->M, payload: link_mem_result_t */

    LINK_OP_RATE         = 0x0020,  /* M->S, arg0 = clkdiv * 256        */
    LINK_OP_RATE_ACK     = 0x0021,  /* S->M                             */

    LINK_OP_BULK_M2S     = 0x0030,  /* M->S, arg0 = block count         */
    LINK_OP_BULK_M2S_ACK = 0x0031,  /* S->M, payload: link_bulk_result_t*/

    LINK_OP_BULK_S2M     = 0x0032,  /* M->S, arg0 = block count         */
    LINK_OP_BULK_S2M_ACK = 0x0033,  /* S->M — sent before the bulk data */

    LINK_OP_DUPLEX       = 0x0034,  /* M->S, arg0 = block count         */
    LINK_OP_DUPLEX_ACK   = 0x0035,  /* S->M, payload: link_bulk_result_t*/

    /* Verified transfers: one block per handshake, every byte compared
     * against the expected pattern by whichever side receives. */
    LINK_OP_VERIFY_M2S     = 0x0036,  /* M->S, arg0 = block count         */
    LINK_OP_VERIFY_M2S_ACK = 0x0037,  /* S->M, payload: link_bulk_result_t*/
    LINK_OP_VERIFY_S2M     = 0x0038,  /* M->S, arg0 = block count         */
    LINK_OP_VERIFY_S2M_ACK = 0x0039,  /* S->M, sent after the data phase  */

    LINK_OP_PING         = 0x0040,  /* M->S, latency probe              */
    LINK_OP_PONG         = 0x0041,  /* S->M                             */
};

/* ---- Frame header (24 bytes), followed by payload, zero-padded to
 *      LINK_CTRL_BYTES. ---- */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t op;
    uint16_t proto_ver;
    uint32_t seq;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t crc;      /* CRC-32 of the whole frame with crc treated as 0 */
} link_hdr_t;

#define LINK_PAYLOAD_BYTES (LINK_CTRL_BYTES - sizeof(link_hdr_t))

/* ---- Payload: who am I ---- */
typedef struct __attribute__((packed)) {
    uint8_t  chip_id[8];       /* flash unique ID — identifies the board half */
    uint8_t  package_is_a;     /* 1 = QFN-60 (RP2350A), 0 = QFN-80 (RP2350B)  */
    uint8_t  rp2350_rev;       /* chip revision from SYSINFO                  */
    uint16_t fw_version;       /* (major << 8) | minor                        */
    uint32_t sys_clk_hz;
    uint32_t flash_jedec_id;   /* 0x00MMTTCC — manufacturer / type / capacity */
    uint32_t flash_bytes;
    uint32_t psram_bytes;      /* 0 if the probe failed                       */
    uint32_t rom_version;
} link_node_info_t;

/* ---- Payload: local peripheral self-test results ---- */
typedef struct __attribute__((packed)) {
    uint8_t  flash_ok;
    uint8_t  psram_ok;
    uint16_t reserved;
    uint32_t flash_bytes;
    uint32_t flash_read_kbps;    /* XIP sequential read, KiB/s */
    uint32_t flash_crc;          /* CRC-32 of the first 64 KiB of XIP  */
    uint32_t psram_bytes;
    uint32_t psram_write_kbps;
    uint32_t psram_read_kbps;
    uint32_t psram_bit_errors;   /* bytes that failed the pattern check */
} link_mem_result_t;

/* ---- Payload: bulk transfer outcome, as seen by the receiver ---- */
typedef struct __attribute__((packed)) {
    uint32_t blocks;
    uint32_t bytes;
    uint32_t byte_errors;        /* bytes differing from the pattern    */
    uint32_t bit_errors;         /* popcount of the XOR differences     */
    uint32_t timeouts;           /* blocks the DMA never finished       */
    uint32_t elapsed_us;         /* as measured by the receiving side   */
} link_bulk_result_t;

/* ---- Helpers shared by both firmwares ---- */
uint32_t link_crc32(const void *data, uint32_t len);

/* Build a control frame into `frame` (must be LINK_CTRL_BYTES, 4-byte
 * aligned). Copies `payload_len` bytes of payload and fills in the CRC. */
void link_frame_build(void *frame, uint16_t op, uint32_t seq,
                      uint32_t arg0, uint32_t arg1,
                      const void *payload, uint32_t payload_len);

/* Validate magic, version and CRC. Returns 1 on success. */
int link_frame_check(const void *frame);

/* Fill in the 32 KiB bulk pattern used by every throughput test. The
 * generator is a 32-bit maximal-length LFSR seeded from `seed`, which
 * gives a spectrally rich pattern (lots of simultaneous-switching
 * transitions) rather than an easy walking-ones sequence. */
void link_bulk_pattern(void *buf, uint32_t bytes, uint32_t seed);

/* Compare a received block against the expected pattern, accumulating
 * into a link_bulk_result_t. */
void link_bulk_verify(link_bulk_result_t *acc,
                      const void *received, const void *expected,
                      uint32_t bytes);

#endif /* LINK_PROTO_H */
