/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "diag_link.h"
#include "console.h"
#include "report.h"
#include "heartbeat.h"

#include "frank_core2_board.h"
#include "link_bus.h"
#include "link_session.h"

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include <stdio.h>
#include <string.h>

/* The link owns pio0 outright: two SMs and both program slots. Audio
 * lives on pio1, the WS2812 heartbeat on pio2. */
#define LINK_PIO pio0

/* Dividers to sweep, 8.8 fixed point. 1.00 is the fastest the hardware
 * can go (sys_clk/4 bytes per second); the slower steps exist so a board
 * that fails at full rate still reports where it does work. */
static const uint16_t sweep_divs[] = {
    0x0100,  /* 1.00x */
    0x0140,  /* 1.25x */
    0x0180,  /* 1.50x */
    0x0200,  /* 2.00x */
};
#define SWEEP_COUNT (sizeof(sweep_divs) / sizeof(sweep_divs[0]))

/* The report reserves R_LINK_SWEEPS rows and diag_link_result_t reserves
 * that many slots; adding a divider without widening both would silently
 * drop the extra row off the screen. */
_Static_assert(SWEEP_COUNT == R_LINK_SWEEPS,
               "sweep_divs[] and R_LINK_SWEEPS must agree");

/* Throughput runs stream this many 32 KiB blocks — 8 MiB, about 130 ms
 * at full rate. Long enough for the timer resolution to be irrelevant,
 * short enough that a four-point sweep stays interactive. */
#define THROUGHPUT_BLOCKS 256

/* Verified runs are shorter because every byte is compared on the CPU. */
#define VERIFY_BLOCKS 16

#define PING_ROUNDS 64

static link_t          g_link;
static link_session_t  g_session;

static uint8_t g_ctrl_tx[LINK_CTRL_BYTES] __attribute__((aligned(4)));
static uint8_t g_ctrl_rx[LINK_CTRL_BYTES] __attribute__((aligned(4)));
static uint8_t g_bulk_tx[LINK_BULK_BYTES] LINK_BULK_ALIGN;
static uint8_t g_bulk_rx[LINK_BULK_BYTES] LINK_BULK_ALIGN;

#if SLAVE_RESET_BODGE
/* Optional hardware reset of the slave.
 *
 * This revision's GPIO43 -> slave RUN net is broken (see README), but the
 * two ends are both accessible pads: R3 pin 1 sits on the GPIO43 net and
 * S4 pin 1 sits on the slave's RUN net, so one wire restores the link.
 * Build with -DSLAVE_RESET_BODGE=ON once that wire is in.
 *
 * The pin is driven open-drain and NEVER driven high: to assert reset we
 * turn it into a low output, to release we return it to an input and let
 * R3's 10K pull-up do the work. Driving it high would fight S4 — which
 * shorts RUN to ground — and put the whole pin drive current through the
 * button every time somebody pressed it.
 *
 * Idle state is therefore also the safe state: at power-on, before any
 * firmware runs, the pin is an input and the slave is released. */
static void slave_reset_pin_init(void) {
    gpio_init(M_LINK_SLAVE_RUN);
    gpio_put(M_LINK_SLAVE_RUN, 0);          /* output register stays 0 */
    gpio_set_dir(M_LINK_SLAVE_RUN, GPIO_IN);/* ...but Hi-Z until we assert */
}

static void slave_reset_pulse(void) {
    gpio_set_dir(M_LINK_SLAVE_RUN, GPIO_OUT);   /* pull RUN low */
    sleep_ms(2);                                 /* RP2350 needs microseconds */
    gpio_set_dir(M_LINK_SLAVE_RUN, GPIO_IN);    /* release to the pull-up */
    sleep_ms(5);
}
#endif

void diag_link_init(void) {
    link_init(&g_link, LINK_PIO,
              M_LINK_A_DATA_BASE,   /* we transmit on bus A */
              M_LINK_B_DATA_BASE,   /* we receive on bus B  */
              M_LINK_DB_OUT, M_LINK_DB_IN,
              M_LINK_FS, true /* master drives FS */);

    /* Both sides fill their transmit buffer with the same pattern from
     * the same seed, so each can verify what it receives without the
     * pattern ever crossing the wire as reference data. */
    link_bulk_pattern(g_bulk_tx, LINK_BULK_BYTES, 0x12345678u);

    g_session.link    = &g_link;
    g_session.ctrl_tx = g_ctrl_tx;
    g_session.ctrl_rx = g_ctrl_rx;
    g_session.bulk_tx = g_bulk_tx;
    g_session.bulk_rx = g_bulk_rx;
    g_session.seq     = 0;
    g_session.handshake_timeout_us = 0;

#if SLAVE_RESET_BODGE
    slave_reset_pin_init();
#endif
}

/* Ask the slave to switch to `q88` and follow it there. The slave
 * acknowledges at the old rate and only then re-divides, so both sides
 * change over on the same message boundary. */
static bool set_rate(uint16_t q88) {
    if (!link_m_send_ctrl(&g_session, LINK_OP_RATE, q88, 0, NULL, 0)) return false;
    if (!link_m_recv_ctrl(&g_session)) return false;
    if (((const link_hdr_t *)g_ctrl_rx)->op != LINK_OP_RATE_ACK) return false;

    link_set_bulk_clkdiv(&g_link, (float)q88 / 256.0f);
    return true;
}

static uint32_t rate_from(uint64_t bytes, uint32_t us) {
    if (!us) return 0;
    return (uint32_t)((bytes * 1000000ull) / us);
}

void diag_link_run(diag_link_result_t *out) {
    memset(out, 0, sizeof(*out));
    heartbeat_set(HB_TEST);

    /* ---- Handshake ---- */
    console_log(C_DIM, "link: saying hello to slave...");

    if (!link_m_send_ctrl(&g_session, LINK_OP_HELLO, 0, 0, NULL, 0) ||
        !link_m_recv_ctrl(&g_session) ||
        ((const link_hdr_t *)g_ctrl_rx)->op != LINK_OP_HELLO_ACK) {
        console_log(C_FAIL, "link: no response from slave");
        /* Render anyway so the table and verdict rows are drawn rather
         * than left as whatever was on screen before. */
        diag_link_render(out);
        heartbeat_set(HB_ERROR);
        return;
    }

    out->contacted = true;
    memcpy(&out->slave_info, g_ctrl_rx + sizeof(link_hdr_t),
           sizeof(out->slave_info));
    console_log(C_OK, "link: slave up, %u MHz, proto ok",
                (unsigned)(out->slave_info.sys_clk_hz / 1000000u));

    /* ---- Remote peripheral self-test ---- */
    console_log(C_DIM, "link: requesting slave self-test...");
    if (link_m_send_ctrl(&g_session, LINK_OP_SELFTEST, 0, 0, NULL, 0) &&
        link_m_recv_ctrl(&g_session) &&
        ((const link_hdr_t *)g_ctrl_rx)->op == LINK_OP_SELFTEST_ACK) {
        memcpy(&out->slave_mem, g_ctrl_rx + sizeof(link_hdr_t),
               sizeof(out->slave_mem));
        out->slave_mem_valid = true;
        console_log(C_OK, "link: slave self-test received");
    } else {
        console_log(C_WARN, "link: slave self-test timed out");
    }

    /* ---- Latency ---- */
    if (link_m_ping(&g_session, PING_ROUNDS, &out->rtt_ns)) {
        console_log(C_DIM, "link: ctrl round-trip %u.%02u us",
                    (unsigned)(out->rtt_ns / 1000),
                    (unsigned)((out->rtt_ns % 1000) / 10));
    } else {
        /* Used to fail silently, which mattered: a half-finished ping
         * leaves the doorbells out of step and the next exchange fails
         * for reasons that look unrelated. */
        console_log(C_WARN, "link: ping failed - resyncing");
        link_m_resync(&g_session);
    }

    /* ---- Rate sweep ---- */
    out->all_passed = true;
    out->sweep_count = SWEEP_COUNT;

    for (unsigned i = 0; i < SWEEP_COUNT; i++) {
        uint16_t q88 = sweep_divs[i];
        uint64_t bytes = (uint64_t)THROUGHPUT_BLOCKS * LINK_BULK_BYTES;
        uint32_t us = 0;

        out->sweep[i].clkdiv_q88 = q88;

        if (!set_rate(q88)) {
            console_log(C_FAIL, "link: slave refused divider %u.%02u",
                        q88 >> 8, ((q88 & 0xFF) * 100) >> 8);
            out->all_passed = false;
            continue;
        }

        console_log(C_DIM, "link: %u.%02ux  streaming %u MiB each way...",
                    q88 >> 8, ((q88 & 0xFF) * 100) >> 8,
                    (unsigned)(bytes / (1024u * 1024u)));

        /* Master -> slave, gapless. */
        if (link_m_send_ctrl(&g_session, LINK_OP_BULK_M2S, THROUGHPUT_BLOCKS, 0, NULL, 0) &&
            link_m_bulk_send(&g_session, THROUGHPUT_BLOCKS, &us) &&
            link_m_recv_ctrl(&g_session)) {
            out->sweep[i].m2s_bytes_per_s = rate_from(bytes, us);
        } else {
            out->all_passed = false;
        }

        /* Slave -> master, gapless. */
        if (link_m_send_ctrl(&g_session, LINK_OP_BULK_S2M, THROUGHPUT_BLOCKS, 0, NULL, 0) &&
            link_m_bulk_recv(&g_session, THROUGHPUT_BLOCKS, &us)) {
            out->sweep[i].s2m_bytes_per_s = rate_from(bytes, us);
        } else {
            out->all_passed = false;
        }

        /* Both directions at once — the number the wiring was designed
         * for, and the one most likely to expose crosstalk. */
        if (link_m_send_ctrl(&g_session, LINK_OP_DUPLEX, THROUGHPUT_BLOCKS, 0, NULL, 0) &&
            link_m_duplex(&g_session, THROUGHPUT_BLOCKS, &us) &&
            link_m_recv_ctrl(&g_session)) {
            out->sweep[i].duplex_bytes_per_s = rate_from(bytes * 2, us);
        } else {
            out->all_passed = false;
        }

        /* Verified pass at the same rate: this is what decides "ok".
         *
         * A structural failure (the exchange never completed) is tracked
         * separately from byte errors rather than folded in as "+1".
         * Conflating them made a broken handshake look like two stray
         * bits, which is exactly the wrong conclusion to hand someone
         * chasing signal integrity. */
        uint32_t errors = 0;
        bool ran = true;
        link_bulk_result_t vr;

        if (link_m_send_ctrl(&g_session, LINK_OP_VERIFY_M2S, VERIFY_BLOCKS, 0, NULL, 0) &&
            link_m_integrity_send(&g_session, VERIFY_BLOCKS) &&
            link_m_recv_ctrl(&g_session) &&
            ((const link_hdr_t *)g_ctrl_rx)->op == LINK_OP_VERIFY_M2S_ACK) {
            memcpy(&vr, g_ctrl_rx + sizeof(link_hdr_t), sizeof(vr));
            errors += vr.byte_errors + vr.timeouts;
            if (vr.byte_errors)
                console_log(C_WARN, "link: M->S %u bad bytes in %u KiB",
                            (unsigned)vr.byte_errors,
                            (unsigned)(vr.bytes / 1024u));
        } else {
            ran = false;
            console_log(C_FAIL, "link: M->S verify did not complete");
            link_m_resync(&g_session);
        }

        if (link_m_send_ctrl(&g_session, LINK_OP_VERIFY_S2M, VERIFY_BLOCKS, 0, NULL, 0) &&
            link_m_integrity_recv(&g_session, VERIFY_BLOCKS, &vr) &&
            link_m_recv_ctrl(&g_session)) {
            errors += vr.byte_errors + vr.timeouts;
            if (vr.byte_errors)
                console_log(C_WARN, "link: S->M %u bad bytes in %u KiB",
                            (unsigned)vr.byte_errors,
                            (unsigned)(vr.bytes / 1024u));
        } else {
            ran = false;
            console_log(C_FAIL, "link: S->M verify did not complete");
            link_m_resync(&g_session);
        }

        out->sweep[i].byte_errors = errors;
        out->sweep[i].verify_ran  = ran;
        out->sweep[i].ok = ran && (errors == 0) &&
                           out->sweep[i].m2s_bytes_per_s &&
                           out->sweep[i].s2m_bytes_per_s;

        if (out->sweep[i].ok) {
            uint32_t agg = out->sweep[i].duplex_bytes_per_s;
            if (agg > out->best_bytes_per_s) out->best_bytes_per_s = agg;
        } else {
            out->all_passed = false;
        }
    }

    /* Leave the link parked at full rate for whatever runs next. */
    set_rate(0x0100);

    diag_link_render(out);
    heartbeat_set(out->all_passed ? HB_OK : HB_ERROR);
}

/* Idle ticks to skip after issuing a reset, at 5 s per tick. The slave
 * needs a second or two to boot and self-test; three ticks is generous
 * without making a genuinely absent slave slow to notice. */
#define RECONNECT_BACKOFF_TICKS 3

bool diag_link_try_reconnect(void) {
    static uint32_t fails;
    static uint32_t backoff;

    /* Short patience: this runs from the idle loop several times a
     * minute, and two seconds per doorbell would make the console feel
     * wedged whenever no slave is fitted. */
    g_session.handshake_timeout_us = 200000u;   /* 200 ms */

    bool alive = link_m_send_ctrl(&g_session, LINK_OP_HELLO, 0, 0, NULL, 0) &&
                 link_m_recv_ctrl(&g_session) &&
                 ((const link_hdr_t *)g_ctrl_rx)->op == LINK_OP_HELLO_ACK;

    g_session.handshake_timeout_us = 0;   /* back to the default */

    if (alive) {
        fails = 0;
        backoff = 0;
        return true;
    }

    /* Escalate rather than reaching for the biggest hammer immediately.
     * The three mechanisms recover different failures and none of them
     * subsumes the others:
     *
     *   do nothing   a slave that is merely still booting, or absent
     *   FS request   a slave whose foreground is stuck — its timer
     *                interrupt still runs, so it can reboot itself
     *   reset pulse  a slave in lockup with interrupts off, which no
     *                amount of software on either side can reach
     *
     * The slave's own 8 s watchdog runs underneath all of this. */
    fails++;

    /* Back off after asking for a reset. The slave needs a moment to
     * boot and run its self-test, and probing again before it is ready
     * just resets it a second time — at the wrong interval that becomes
     * a reboot loop the operator sees as "the slave never comes up". */
    if (backoff) {
        backoff--;
        return false;
    }

    if ((fails % 2) == 0) {
        link_m_request_slave_reset(&g_session);
        backoff = RECONNECT_BACKOFF_TICKS;
    }

#if SLAVE_RESET_BODGE
    if ((fails % 4) == 0) {
        slave_reset_pulse();
        backoff = RECONNECT_BACKOFF_TICKS;
    }
#endif

    return false;
}

/* Short-timeout HELLO, used by the reconnect probe and the reset test. */
static bool slave_answers(void) {
    g_session.handshake_timeout_us = 200000u;   /* 200 ms */
    bool alive = link_m_send_ctrl(&g_session, LINK_OP_HELLO, 0, 0, NULL, 0) &&
                 link_m_recv_ctrl(&g_session) &&
                 ((const link_hdr_t *)g_ctrl_rx)->op == LINK_OP_HELLO_ACK;
    g_session.handshake_timeout_us = 0;
    return alive;
}

bool diag_link_reset_slave(void) {
    if (!slave_answers()) {
        console_log(C_WARN, "reset: slave not answering to begin with");
        return false;
    }
    console_log(C_DIM, "reset: slave alive, pulsing FS for %u ms...",
                (unsigned)LINK_RESET_PULSE_MS);

    link_m_request_slave_reset(&g_session);

    /* It has to actually stop answering, or we have proved nothing. */
    bool went_away = false;
    for (int i = 0; i < 30 && !went_away; i++) {
        if (!slave_answers()) went_away = true;
        else sleep_ms(50);
    }
    if (!went_away) {
        console_log(C_FAIL, "reset: slave never stopped answering - FS ignored");
        return false;
    }
    console_log(C_OK, "reset: slave went down");

    /* Back within a few seconds: it skips its console-attach window
     * after a watchdog reboot but still reruns its whole self-test. */
    absolute_time_t t0 = get_absolute_time();
    for (int i = 0; i < 100; i++) {
        if (slave_answers()) {
            int64_t ms = absolute_time_diff_us(t0, get_absolute_time()) / 1000;
            console_log(C_OK, "reset: slave back after %u ms", (unsigned)ms);
            return true;
        }
        sleep_ms(100);
    }
    console_log(C_FAIL, "reset: slave did not come back");
    return false;
}

void diag_link_render(const diag_link_result_t *r) {
    char a[16], b[16], c[16];

    console_rule(R_RULE_LINK, C_DIM);
    console_at(R_LINK_HDR, 0, C_TITLE,
               " rate     M->S        S->M        duplex      err");

    for (int i = 0; i < R_LINK_SWEEPS; i++) {
        int row = R_LINK_ROW0 + i;
        console_clear_row(row);

        if (i >= r->sweep_count) continue;

        uint16_t q = r->sweep[i].clkdiv_q88;
        report_format_bps(a, sizeof(a), r->sweep[i].m2s_bytes_per_s);
        report_format_bps(b, sizeof(b), r->sweep[i].s2m_bytes_per_s);
        report_format_bps(c, sizeof(c), r->sweep[i].duplex_bytes_per_s);

        uint8_t col = r->sweep[i].ok ? C_TEXT : C_FAIL;
        char err[12];
        if (!r->sweep[i].verify_ran)
            snprintf(err, sizeof(err), "no-run");
        else
            snprintf(err, sizeof(err), "%u", (unsigned)r->sweep[i].byte_errors);

        console_at(row, 0, col, " %u.%02ux  %-11s %-11s %-11s %s",
                   q >> 8, ((q & 0xFF) * 100) >> 8, a, b, c, err);
    }

    console_clear_row(R_LINK_VERIFY);
    if (r->rtt_ns) {
        report_format_bps(a, sizeof(a), r->best_bytes_per_s);
        console_at(R_LINK_VERIFY, 0, C_DIM,
                   " round-trip %u.%02u us    peak duplex %s",
                   (unsigned)(r->rtt_ns / 1000),
                   (unsigned)((r->rtt_ns % 1000) / 10), a);
    }

    console_clear_row(R_VERDICT);
    if (!r->contacted) {
        console_at(R_VERDICT, 0, C_FAIL, " LINK DOWN - slave did not answer");
    } else if (r->all_passed) {
        report_format_bps(a, sizeof(a), r->best_bytes_per_s);
        console_at(R_VERDICT, 0, C_OK,
                   " LINK OK - error free to %s aggregate", a);
    } else {
        console_at(R_VERDICT, 0, C_WARN,
                   " LINK DEGRADED - see error column above");
    }

    console_rule(R_RULE_BOT, C_DIM);
    console_flush();
}
