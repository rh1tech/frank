/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "link_session.h"

#include "pico/stdlib.h"
#include "pico/time.h"

#include <string.h>

/* Doorbell patience for this session. */
static inline uint32_t hs_timeout(const link_session_t *s) {
    return s->handshake_timeout_us ? s->handshake_timeout_us
                                   : LINK_HANDSHAKE_TIMEOUT_US;
}

/* Time budget for a bulk phase, derived from the current wire rate with
 * a 4x cushion so a slow divider or a stalled peer still fails cleanly
 * instead of hanging the diagnostic. */
static uint32_t bulk_timeout_us(const link_t *l, uint32_t blocks) {
    uint64_t bytes = (uint64_t)blocks * LINK_BULK_BYTES;
    uint32_t rate  = link_byte_rate(l);
    if (!rate) return LINK_HANDSHAKE_TIMEOUT_US;

    uint64_t us = (bytes * 1000000ull) / rate;
    us = us * 4 + 100000ull;
    if (us > 30000000ull) us = 30000000ull;
    return (uint32_t)us;
}

/* ================================================================== */
/* Master                                                             */
/* ================================================================== */

bool link_m_send_ctrl(link_session_t *s, uint16_t op,
                      uint32_t arg0, uint32_t arg1,
                      const void *payload, uint32_t payload_len) {
    link_frame_build(s->ctrl_tx, op, ++s->seq, arg0, arg1, payload, payload_len);

    /* "Master ready to send." */
    link_db_set(s->link, true);
    if (!link_db_wait(s->link, true, hs_timeout(s))) {
        link_db_set(s->link, false);
        return false;
    }

    link_use_ctrl_rate(s->link);
    link_tx_start(s->link, s->ctrl_tx, LINK_CTRL_BYTES);
    bool ok = link_tx_finish(s->link, LINK_CTRL_TIMEOUT_US);

    link_db_set(s->link, false);
    if (!link_db_wait(s->link, false, hs_timeout(s))) return false;

    return ok;
}

bool link_m_recv_ctrl(link_session_t *s) {
    /* Arm before saying "go" so the receive shift counter is aligned
     * with the first word the slave pushes. */
    link_rx_arm(s->link, s->ctrl_rx, LINK_CTRL_BYTES);

    link_db_set(s->link, true);
    if (!link_db_wait(s->link, true, hs_timeout(s))) {
        link_db_set(s->link, false);
        link_rx_abort(s->link);
        return false;
    }

    bool ok = (link_rx_wait(s->link, LINK_CTRL_TIMEOUT_US) == 0);

    link_db_set(s->link, false);
    link_db_wait(s->link, false, hs_timeout(s));

    return ok && link_frame_check(s->ctrl_rx);
}

bool link_m_bulk_send(link_session_t *s, uint32_t blocks, uint32_t *elapsed_us) {
    uint32_t tmo = bulk_timeout_us(s->link, blocks);

    /* The slave arms its ring receiver, then raises DB_SM. */
    link_db_set(s->link, true);
    if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) {
        link_db_set(s->link, false);
        return false;
    }

    link_use_bulk_rate(s->link);
    absolute_time_t t0 = get_absolute_time();
    link_tx_start_ring(s->link, s->bulk_tx, LINK_BULK_RING_BITS,
                       (size_t)blocks * LINK_BULK_BYTES);
    bool ok = link_tx_finish(s->link, tmo);
    int64_t us = absolute_time_diff_us(t0, get_absolute_time());

    link_db_set(s->link, false);
    link_db_wait(s->link, false, tmo);

    if (elapsed_us) *elapsed_us = us > 0 ? (uint32_t)us : 0;
    return ok;
}

bool link_m_bulk_recv(link_session_t *s, uint32_t blocks, uint32_t *elapsed_us) {
    uint32_t tmo = bulk_timeout_us(s->link, blocks);

    link_rx_arm_ring(s->link, s->bulk_rx, LINK_BULK_RING_BITS,
                     (size_t)blocks * LINK_BULK_BYTES);

    absolute_time_t t0 = get_absolute_time();
    link_db_set(s->link, true);

    bool ok = (link_rx_wait(s->link, tmo) == 0);
    int64_t us = absolute_time_diff_us(t0, get_absolute_time());

    /* The slave raises DB_SM once its transmit has drained. */
    if (ok) ok = link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US);

    link_db_set(s->link, false);
    link_db_wait(s->link, false, LINK_HANDSHAKE_TIMEOUT_US);

    if (elapsed_us) *elapsed_us = us > 0 ? (uint32_t)us : 0;
    return ok;
}

bool link_m_duplex(link_session_t *s, uint32_t blocks, uint32_t *elapsed_us) {
    uint32_t tmo = bulk_timeout_us(s->link, blocks);
    size_t total = (size_t)blocks * LINK_BULK_BYTES;

    link_rx_arm_ring(s->link, s->bulk_rx, LINK_BULK_RING_BITS, total);

    /* "Master armed." The slave arms its own receiver and answers. */
    link_db_set(s->link, true);
    if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) {
        link_db_set(s->link, false);
        link_rx_abort(s->link);
        return false;
    }

    link_use_bulk_rate(s->link);
    absolute_time_t t0 = get_absolute_time();
    link_tx_start_ring(s->link, s->bulk_tx, LINK_BULK_RING_BITS, total);

    bool ok = link_tx_finish(s->link, tmo);
    ok = (link_rx_wait(s->link, tmo) == 0) && ok;
    int64_t us = absolute_time_diff_us(t0, get_absolute_time());

    link_db_set(s->link, false);
    link_db_wait(s->link, false, tmo);

    if (elapsed_us) *elapsed_us = us > 0 ? (uint32_t)us : 0;
    return ok;
}

bool link_m_integrity_send(link_session_t *s, uint32_t blocks) {
    uint32_t tmo = bulk_timeout_us(s->link, 1);

    for (uint32_t i = 0; i < blocks; i++) {
        /* Slave arms for one block and answers. */
        link_db_set(s->link, true);
        if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) {
            link_db_set(s->link, false);
            return false;
        }

        link_use_bulk_rate(s->link);
        link_tx_start(s->link, s->bulk_tx, LINK_BULK_BYTES);
        bool ok = link_tx_finish(s->link, tmo);

        link_db_set(s->link, false);
        /* The slave holds DB_SM high while it verifies the block. */
        if (!link_db_wait(s->link, false, tmo)) return false;
        if (!ok) return false;
    }
    return true;
}

bool link_m_integrity_recv(link_session_t *s, uint32_t blocks,
                           link_bulk_result_t *out) {
    uint32_t tmo = bulk_timeout_us(s->link, 1);

    memset(out, 0, sizeof(*out));
    absolute_time_t t0 = get_absolute_time();

    for (uint32_t i = 0; i < blocks; i++) {
        link_rx_arm(s->link, s->bulk_rx, LINK_BULK_BYTES);

        link_db_set(s->link, true);
        if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) {
            link_db_set(s->link, false);
            link_rx_abort(s->link);
            return false;
        }

        if (link_rx_wait(s->link, tmo) != 0) {
            out->timeouts++;
            out->blocks++;
            out->bytes += LINK_BULK_BYTES;
        } else {
            link_bulk_verify(out, s->bulk_rx, s->bulk_tx, LINK_BULK_BYTES);
        }

        link_db_set(s->link, false);
        if (!link_db_wait(s->link, false, tmo)) return false;
    }

    int64_t us = absolute_time_diff_us(t0, get_absolute_time());
    out->elapsed_us = us > 0 ? (uint32_t)us : 0;
    return true;
}

void link_m_resync(link_session_t *s) {
    link_db_set(s->link, false);
    link_rx_abort(s->link);
    link_use_ctrl_rate(s->link);

    /* Longer than the slave's LINK_HANDSHAKE_TIMEOUT_US so any wait it
     * is parked in has definitely expired before we speak again. */
    sleep_ms(LINK_HANDSHAKE_TIMEOUT_US / 1000u + 500u);

    link_db_wait(s->link, false, LINK_HANDSHAKE_TIMEOUT_US);
}

void link_m_request_slave_reset(link_session_t *s) {
    link_fs_set(s->link, true);
    busy_wait_us(LINK_RESET_PULSE_MS * 1000u);
    link_fs_set(s->link, false);
}

bool link_m_ping(link_session_t *s, uint32_t rounds, uint32_t *avg_ns) {
    if (!rounds) return false;

    absolute_time_t t0 = get_absolute_time();
    for (uint32_t i = 0; i < rounds; i++) {
        if (!link_m_send_ctrl(s, LINK_OP_PING, i, 0, NULL, 0)) return false;
        if (!link_m_recv_ctrl(s)) return false;
        if (((const link_hdr_t *)s->ctrl_rx)->op != LINK_OP_PONG) return false;
    }
    int64_t us = absolute_time_diff_us(t0, get_absolute_time());

    if (avg_ns) *avg_ns = (uint32_t)((us * 1000ll) / (int64_t)rounds);
    return true;
}

/* ================================================================== */
/* Slave                                                              */
/* ================================================================== */

/* Receive one control frame from the master. Assumes DB_MS is already
 * high (the caller detected it) and leaves both doorbells low. */
static bool slave_take_ctrl(link_session_t *s) {
    link_rx_arm(s->link, s->ctrl_rx, LINK_CTRL_BYTES);

    link_db_set(s->link, true);                  /* "armed" */
    bool ok = (link_rx_wait(s->link, LINK_CTRL_TIMEOUT_US) == 0);

    /* Master drops DB_MS once its frame has drained. */
    link_db_wait(s->link, false, LINK_HANDSHAKE_TIMEOUT_US);
    link_db_set(s->link, false);

    return ok && link_frame_check(s->ctrl_rx);
}

/* Send one control frame back. The master arms first and signals with
 * DB_MS; we transmit, then raise DB_SM to say "sent". */
static bool slave_put_ctrl(link_session_t *s, uint16_t op,
                           uint32_t arg0, uint32_t arg1,
                           const void *payload, uint32_t payload_len) {
    link_frame_build(s->ctrl_tx, op, ++s->seq, arg0, arg1, payload, payload_len);

    if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) return false;

    link_use_ctrl_rate(s->link);
    link_tx_start(s->link, s->ctrl_tx, LINK_CTRL_BYTES);
    bool ok = link_tx_finish(s->link, LINK_CTRL_TIMEOUT_US);

    link_db_set(s->link, true);                  /* "sent" */
    link_db_wait(s->link, false, LINK_HANDSHAKE_TIMEOUT_US);
    link_db_set(s->link, false);

    return ok;
}

uint16_t link_s_serve(link_session_t *s, uint32_t timeout_us,
                      const link_node_info_t *info,
                      const link_mem_result_t *mem) {
    if (!link_db_wait(s->link, true, timeout_us)) return 0;
    if (!slave_take_ctrl(s)) return 0;

    const link_hdr_t *h = (const link_hdr_t *)s->ctrl_rx;
    uint16_t op     = h->op;
    uint32_t blocks = h->arg0;
    uint32_t tmo    = bulk_timeout_us(s->link, blocks ? blocks : 1);

    switch (op) {
    case LINK_OP_HELLO:
        slave_put_ctrl(s, LINK_OP_HELLO_ACK, 0, 0, info, sizeof(*info));
        break;

    case LINK_OP_SELFTEST:
        slave_put_ctrl(s, LINK_OP_SELFTEST_ACK, 0, 0, mem, sizeof(*mem));
        break;

    case LINK_OP_PING:
        slave_put_ctrl(s, LINK_OP_PONG, h->arg0, 0, NULL, 0);
        break;

    case LINK_OP_RATE: {
        /* arg0 is the divider in 8.8 fixed point. Only bulk traffic is
         * affected: control frames stay at LINK_CTRL_CLKDIV so a rate
         * under test can never corrupt the protocol negotiating it. */
        uint32_t q88 = h->arg0;
        slave_put_ctrl(s, LINK_OP_RATE_ACK, q88, 0, NULL, 0);
        link_set_bulk_clkdiv(s->link, (float)q88 / 256.0f);
        break;
    }

    case LINK_OP_BULK_M2S: {
        /* Gapless throughput receive: ring DMA, no verification. */
        link_rx_arm_ring(s->link, s->bulk_rx, LINK_BULK_RING_BITS,
                         (size_t)blocks * LINK_BULK_BYTES);

        link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US);
        link_db_set(s->link, true);              /* "armed, send" */

        absolute_time_t t0 = get_absolute_time();
        bool ok = (link_rx_wait(s->link, tmo) == 0);
        int64_t us = absolute_time_diff_us(t0, get_absolute_time());

        link_db_wait(s->link, false, tmo);
        link_db_set(s->link, false);

        link_bulk_result_t r;
        memset(&r, 0, sizeof(r));
        r.blocks     = blocks;
        r.bytes      = blocks * LINK_BULK_BYTES;
        r.timeouts   = ok ? 0 : 1;
        r.elapsed_us = us > 0 ? (uint32_t)us : 0;
        slave_put_ctrl(s, LINK_OP_BULK_M2S_ACK, 0, 0, &r, sizeof(r));
        break;
    }

    case LINK_OP_BULK_S2M: {
        /* Master arms, then raises DB_MS to say "go". */
        if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) break;

        link_use_bulk_rate(s->link);
        link_tx_start_ring(s->link, s->bulk_tx, LINK_BULK_RING_BITS,
                           (size_t)blocks * LINK_BULK_BYTES);
        link_tx_finish(s->link, tmo);

        link_db_set(s->link, true);              /* "sent" */
        link_db_wait(s->link, false, tmo);
        link_db_set(s->link, false);
        break;
    }

    case LINK_OP_DUPLEX: {
        size_t total = (size_t)blocks * LINK_BULK_BYTES;

        if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) break;
        link_rx_arm_ring(s->link, s->bulk_rx, LINK_BULK_RING_BITS, total);
        link_db_set(s->link, true);              /* "both armed" */

        link_use_bulk_rate(s->link);
        link_tx_start_ring(s->link, s->bulk_tx, LINK_BULK_RING_BITS, total);

        absolute_time_t t0 = get_absolute_time();
        bool ok = link_tx_finish(s->link, tmo);
        ok = (link_rx_wait(s->link, tmo) == 0) && ok;
        int64_t us = absolute_time_diff_us(t0, get_absolute_time());

        link_db_wait(s->link, false, tmo);
        link_db_set(s->link, false);

        link_bulk_result_t r;
        memset(&r, 0, sizeof(r));
        r.blocks     = blocks;
        r.bytes      = blocks * LINK_BULK_BYTES;
        r.timeouts   = ok ? 0 : 1;
        r.elapsed_us = us > 0 ? (uint32_t)us : 0;
        slave_put_ctrl(s, LINK_OP_DUPLEX_ACK, 0, 0, &r, sizeof(r));
        break;
    }

    case LINK_OP_VERIFY_M2S: {
        /* One block per handshake, every byte checked against the
         * pattern we hold locally. Slower than the ring path by design. */
        link_bulk_result_t r;
        memset(&r, 0, sizeof(r));

        for (uint32_t i = 0; i < blocks; i++) {
            if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) break;

            link_rx_arm(s->link, s->bulk_rx, LINK_BULK_BYTES);
            link_db_set(s->link, true);          /* "armed" */

            bool ok = (link_rx_wait(s->link, tmo) == 0);
            link_db_wait(s->link, false, tmo);

            if (ok) {
                link_bulk_verify(&r, s->bulk_rx, s->bulk_tx, LINK_BULK_BYTES);
            } else {
                r.blocks++;
                r.bytes += LINK_BULK_BYTES;
                r.timeouts++;
            }

            link_db_set(s->link, false);         /* "verified, next" */
        }

        slave_put_ctrl(s, LINK_OP_VERIFY_M2S_ACK, 0, 0, &r, sizeof(r));
        break;
    }

    case LINK_OP_VERIFY_S2M: {
        /* Mirror image: one block per handshake, the master verifies. */
        for (uint32_t i = 0; i < blocks; i++) {
            if (!link_db_wait(s->link, true, LINK_HANDSHAKE_TIMEOUT_US)) break;

            link_use_bulk_rate(s->link);
            link_tx_start(s->link, s->bulk_tx, LINK_BULK_BYTES);
            link_tx_finish(s->link, tmo);

            link_db_set(s->link, true);          /* "block sent" */
            link_db_wait(s->link, false, tmo);
            link_db_set(s->link, false);
        }

        slave_put_ctrl(s, LINK_OP_VERIFY_S2M_ACK, blocks, 0, NULL, 0);
        break;
    }

    default:
        break;
    }

    return op;
}
