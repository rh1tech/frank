/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * link_session.h — the doorbell-sequenced conversation between the two
 * MCUs, built on link_bus (wire) and link_proto (frames).
 *
 * There is exactly one invariant to keep in your head: DB_MS (driven by
 * the master) means "master is ready for the next step", DB_SM (driven
 * by the slave) means "slave is ready / slave is done". Every phase
 * starts with both low, raises both, does its data transfer, then drops
 * both. Nothing depends on the two chips agreeing about absolute time,
 * which is what lets the slave boot seconds after the master and still
 * join cleanly.
 *
 * Throughput and integrity are measured separately on purpose:
 *
 *   throughput  one uninterrupted ring DMA, no per-block handshake and
 *               no CPU verification, so the number is the wire and
 *               nothing else;
 *   integrity   block-at-a-time with a handshake between each, every
 *               byte compared against the expected pattern.
 *
 * Mixing them would either understate the speed (verification in the
 * hot path) or overstate the confidence (unverified bytes).
 */
#ifndef LINK_SESSION_H
#define LINK_SESSION_H

#include "link_bus.h"
#include "link_proto.h"

/* Buffers the caller supplies. bulk_tx/bulk_rx must be aligned to their
 * own size (LINK_BULK_BYTES) because the ring DMA wraps on that
 * boundary; the LINK_BULK_ALIGN attribute below does that for you. */
#define LINK_BULK_ALIGN __attribute__((aligned(LINK_BULK_BYTES)))
#define LINK_BULK_RING_BITS 15   /* log2(LINK_BULK_BYTES) == log2(32768) */

typedef struct {
    link_t   *link;
    uint8_t  *ctrl_tx;    /* LINK_CTRL_BYTES, 4-byte aligned  */
    uint8_t  *ctrl_rx;
    uint8_t  *bulk_tx;    /* LINK_BULK_BYTES, LINK_BULK_ALIGN */
    uint8_t  *bulk_rx;
    uint32_t  seq;

    /* Doorbell patience, in microseconds. 0 selects the default. The
     * idle-loop reconnect probe drops this to a few hundred
     * milliseconds so a missing slave costs a blink rather than two
     * seconds of a stalled foreground. */
    uint32_t  handshake_timeout_us;
} link_session_t;

/* Default timeouts. Control frames are 128 bytes even at the slowest
 * divider we sweep, so a second is enormously generous; it exists to
 * turn "slave not programmed" into a clean failure rather than a hang. */
#define LINK_CTRL_TIMEOUT_US   1000000u
#define LINK_HANDSHAKE_TIMEOUT_US 2000000u

/* ---------------- Master side ---------------- */

/* Send one control frame. Returns false if the slave never answered the
 * doorbell or the transmit stalled. */
bool link_m_send_ctrl(link_session_t *s, uint16_t op,
                      uint32_t arg0, uint32_t arg1,
                      const void *payload, uint32_t payload_len);

/* Receive one control frame into s->ctrl_rx. Validates magic and CRC. */
bool link_m_recv_ctrl(link_session_t *s);

/* Gapless master -> slave stream of `blocks` * LINK_BULK_BYTES.
 * `elapsed_us` is the master's own view of the transmit window. */
bool link_m_bulk_send(link_session_t *s, uint32_t blocks, uint32_t *elapsed_us);

/* Gapless slave -> master stream. `elapsed_us` is measured by the
 * master from "go" to the last byte landing. */
bool link_m_bulk_recv(link_session_t *s, uint32_t blocks, uint32_t *elapsed_us);

/* Both directions at once. `elapsed_us` covers the longer of the two. */
bool link_m_duplex(link_session_t *s, uint32_t blocks, uint32_t *elapsed_us);

/* Block-at-a-time master -> slave; the slave verifies and returns its
 * tally in the following LINK_OP_BULK_M2S_ACK frame. */
bool link_m_integrity_send(link_session_t *s, uint32_t blocks);

/* Block-at-a-time slave -> master; the master verifies locally. */
bool link_m_integrity_recv(link_session_t *s, uint32_t blocks,
                           link_bulk_result_t *out);

/* Round-trip time of a 128-byte control frame each way, in
 * microseconds, averaged over `rounds`. Returns false on any timeout. */
bool link_m_ping(link_session_t *s, uint32_t rounds, uint32_t *avg_ns);

/* Return both sides to a known idle state after a failed exchange.
 *
 * A control frame that fails mid-handshake leaves the two doorbells out
 * of step: the master has given up while the slave is still waiting for
 * the other half of the sequence. Everything after that fails for
 * reasons that look unrelated to the original fault. Dropping our
 * doorbell and idling for longer than the slave's own worst-case
 * timeout lets its pending waits expire and both sides converge on
 * "both doorbells low, nothing in flight". */
void link_m_resync(link_session_t *s);

/* Ask the slave to reboot itself by holding FS high.
 *
 * Master GPIO43 is labelled as slave reset in the schematic but does not
 * reach the slave's RUN pin (see README), so there is no hardware reset
 * path. FS is wired, tested and otherwise unused, and the slave samples
 * it from a timer interrupt — so this still works when the slave's main
 * loop is stuck, which is exactly when it is worth having.
 *
 * Returns after the pulse; the slave needs roughly a second to come back
 * and rerun its self-test. */
void link_m_request_slave_reset(link_session_t *s);

/* How long the master holds FS to request a reset, and how long the
 * slave must observe it before acting. The slave's threshold is shorter
 * so a marginally-timed pulse is still caught. */
#define LINK_RESET_PULSE_MS  250u
#define LINK_RESET_DETECT_MS 120u

/* ---------------- Slave side ---------------- */

/* Serve exactly one master request. Blocks until the master rings the
 * doorbell or `timeout_us` expires. Returns the opcode handled, or 0 on
 * timeout / bad frame. `info` and `mem` are the payloads the slave hands
 * back for HELLO and SELFTEST. */
uint16_t link_s_serve(link_session_t *s, uint32_t timeout_us,
                      const link_node_info_t *info,
                      const link_mem_result_t *mem);

#endif /* LINK_SESSION_H */
