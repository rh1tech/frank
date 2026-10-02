/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * diag_link.h — the master's half of the inter-processor test.
 *
 * Brings the link up, interrogates the slave, sweeps the wire rate and
 * writes the results straight into the console report.
 */
#ifndef DIAG_LINK_H
#define DIAG_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "link_proto.h"

typedef struct {
    bool               contacted;      /* slave answered HELLO           */
    link_node_info_t   slave_info;
    bool               slave_mem_valid;
    link_mem_result_t  slave_mem;

    /* Per-divider sweep results. */
    struct {
        uint16_t clkdiv_q88;           /* divider in 8.8 fixed point     */
        uint32_t m2s_bytes_per_s;
        uint32_t s2m_bytes_per_s;
        uint32_t duplex_bytes_per_s;
        uint32_t byte_errors;          /* bytes that came back wrong     */
        bool     verify_ran;           /* the verified pass completed    */
        bool     ok;
    } sweep[4];
    int  sweep_count;

    uint32_t rtt_ns;                   /* 128-byte control round trip    */
    uint32_t best_bytes_per_s;         /* fastest error-free divider     */
    bool     all_passed;
} diag_link_result_t;

/* Claim PIO/DMA and configure the master's link pins. Call once. */
void diag_link_init(void);

/* Run the whole sequence: handshake, remote self-test, rate sweep,
 * verified transfers, latency. Renders progress into the console log
 * and the results into the fixed report rows as it goes. */
void diag_link_run(diag_link_result_t *out);

/* Repaint the link table from a previous run (used after a redraw). */
void diag_link_render(const diag_link_result_t *r);

/* Cheap "is the slave there yet?" probe for the idle loop.
 *
 * The master cannot reset the slave in hardware (see README), so a slave
 * that boots late, is reflashed, or reboots on its own has to be picked
 * up by the master noticing rather than by the master power-cycling it.
 * Uses a short doorbell timeout so a genuinely absent slave costs a
 * blink rather than a stalled foreground, and pulses FS to ask a wedged
 * slave to reboot itself before giving up on this attempt.
 *
 * Returns true when the slave answered — the caller should then re-run
 * the full diagnostic. */
bool diag_link_try_reconnect(void);

/* Ask the slave to reboot and prove that it did.
 *
 * Confirms the slave is answering, pulses FS, then watches for it to
 * stop answering and come back. "Stopped answering" is the part that
 * matters: without it a slave that simply never rebooted is
 * indistinguishable from one that rebooted too fast to notice.
 *
 * Returns true only if the slave went away and returned. */
bool diag_link_reset_slave(void);

#endif /* DIAG_LINK_H */
