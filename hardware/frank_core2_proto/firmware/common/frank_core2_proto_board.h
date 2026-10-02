/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * frank_core2_proto_board.h — GPIO map for the FRANK Core 2 board.
 *
 * Every assignment below was extracted from the KiCad netlist
 * (frank_core2.kicad_sch -> kicad-cli sch export netlist), so this file
 * is the single source of truth shared by the master and slave builds.
 *
 * U3 = RP2350B (master, QFN-80)   U6 = RP2350  (slave, QFN-60)
 *
 * The master carries all the user-facing peripherals: HDMI (J5), microSD
 * (J7), TDA1387 I2S DAC (U8), USB-C host/device (J8), WS2812B status LED
 * (LD1) and 8 MB PSRAM (U2) + 16 MB flash (U1).
 *
 * The slave carries its own 8 MB PSRAM (U5) + 16 MB flash (U4), a USB-C
 * port (J9), a blue status LED (LD2) and a UART header (J4). Everything
 * else it needs it gets over the inter-processor link.
 */
#ifndef FRANK_CORE2_BOARD_H
#define FRANK_CORE2_BOARD_H

/* =====================================================================
 * Inter-processor link
 *
 * Two independent 8-bit source-synchronous buses, each with its own
 * clock and VALID strobe, plus three single-wire control signals.
 *
 *   Bus A  master -> slave   M.GPIO20..27 -> S.GPIO1..8
 *                            M.GPIO28 (CLK, 33R R1) -> S.GPIO9
 *                            M.GPIO29 (VALID)       -> S.GPIO10
 *
 *   Bus B  slave -> master   S.GPIO11..18 -> M.GPIO30..37
 *                            S.GPIO19 (CLK, 33R R2) -> M.GPIO38
 *                            S.GPIO20 (VALID)       -> M.GPIO39
 *
 * Both buses share the same relative layout, which the PIO programs
 * rely on: CLK == DATA_BASE + 8, VALID == DATA_BASE + 9. That lets one
 * pair of PIO programs serve both directions on both chips with nothing
 * but a different pin base.
 *
 * Control signals (plain SIO, no PIO):
 *   FS      M.GPIO40 -> S.GPIO21   frame-sync / phase strobe (master out)
 *   DB_MS   M.GPIO41 -> S.GPIO22   doorbell, master -> slave
 *   DB_SM   M.GPIO42 <- S.GPIO23   doorbell, slave  -> master
 *
 * NOTE ON GPIO43: the schematic labels M.GPIO43 as "RUNA/SR" (slave
 * reset), but the net only reaches a 10K pull-up to +3V3 (R3) — it is
 * NOT wired to the slave's RUN pin (U6.26), which only sees reset
 * button S4. The firmware therefore cannot reset the slave in hardware;
 * both MCUs must be reset/flashed independently. See README.md.
 * ===================================================================== */

/* ---- Master side (RP2350B / U3) ---- */
#define M_LINK_A_DATA_BASE   20   /* GPIO20..27, master -> slave  */
#define M_LINK_A_CLK         28   /* == DATA_BASE + 8             */
#define M_LINK_A_VALID       29   /* == DATA_BASE + 9             */

#define M_LINK_B_DATA_BASE   30   /* GPIO30..37, slave -> master  */
#define M_LINK_B_CLK         38
#define M_LINK_B_VALID       39

#define M_LINK_FS            40   /* out */
#define M_LINK_DB_OUT        41   /* DB_MS, out */
#define M_LINK_DB_IN         42   /* DB_SM, in  */
#define M_LINK_SLAVE_RUN     43   /* pull-up only — not connected, see above */

/* ---- Slave side (RP2350 / U6) ---- */
#define S_LINK_A_DATA_BASE    1   /* GPIO1..8, master -> slave (RX) */
#define S_LINK_A_CLK          9
#define S_LINK_A_VALID       10

#define S_LINK_B_DATA_BASE   11   /* GPIO11..18, slave -> master (TX) */
#define S_LINK_B_CLK         19
#define S_LINK_B_VALID       20

#define S_LINK_FS            21   /* in  */
#define S_LINK_DB_IN         22   /* DB_MS, in  */
#define S_LINK_DB_OUT        23   /* DB_SM, out */

/* =====================================================================
 * Master peripherals
 * ===================================================================== */

/* HDMI (J5) via HSTX — clock pair first, N before P, matches Murmulator 2 */
#define HDMI_BASE_PIN        12
#define HDMI_PIN_CLKN        12
#define HDMI_PIN_CLKP        13
#define HDMI_PIN_D0N         14
#define HDMI_PIN_D0P         15
#define HDMI_PIN_D1N         16
#define HDMI_PIN_D1P         17
#define HDMI_PIN_D2N         18
#define HDMI_PIN_D2P         19

/* microSD (J7) on SPI0 — SDIO pin names from the schematic in comments */
#define SDCARD_PIN_SPI0_MISO  4   /* SD DAT0    */
#define SDCARD_PIN_SPI0_CS    5   /* SD DAT3/CD */
#define SDCARD_PIN_SPI0_SCK   6   /* SD CLK     */
#define SDCARD_PIN_SPI0_MOSI  7   /* SD CMD     */

/* TDA1387T I2S DAC (U8) */
#define I2S_DATA_PIN          9   /* U8.3 DATA */
#define I2S_CLOCK_PIN_BASE   10   /* U8.1 SCLK = 10, U8.2 LRCK = 11 */

/* WS2812B status LED (LD1) via 330R R11 */
#define M_LED_WS2812_PIN     46

/* 8 MB PSRAM (U2 ESP-PSRAM64H) chip select */
#define M_PSRAM_CS_PIN       47

/* Debug UART header J2: pin1 = GPIO0 (TX), pin2 = GND, pin3 = GPIO1 (RX) */
#define M_UART_ID            uart0
#define M_UART_TX_PIN         0
#define M_UART_RX_PIN         1

/* Spare / unrouted master GPIOs (available for probing) */
#define M_SPARE_GPIO_LIST    { 2, 3, 8, 44, 45 }

/* =====================================================================
 * Slave peripherals
 * ===================================================================== */

/* 8 MB PSRAM (U5 ESP-PSRAM64H) chip select */
#define S_PSRAM_CS_PIN        0

/* Blue status LED (LD2) via 1K R19 — active high */
#define S_LED_PIN            26

/* Debug UART header J4: pin1 = GPIO24 (TX), pin2 = GND, pin3 = GPIO25 (RX) */
#define S_UART_ID            uart1
#define S_UART_TX_PIN        24
#define S_UART_RX_PIN        25

/* Spare / unrouted slave GPIOs */
#define S_SPARE_GPIO_LIST    { 27, 28, 29 }

/* =====================================================================
 * Memory devices (identical part numbers on both sides)
 * ===================================================================== */
#define FRANK_FLASH_PART     "W25Q128JVPIQ"
#define FRANK_FLASH_BYTES    (16u * 1024u * 1024u)
#define FRANK_PSRAM_PART     "ESP-PSRAM64H"
#define FRANK_PSRAM_BYTES    (8u * 1024u * 1024u)

/* XIP windows: CS0 = flash, CS1 = PSRAM (psram_init maps it there).
 *
 * The 0x14000000 aliases bypass the XIP cache entirely. That matters for
 * the PSRAM presence and size probes: through the cached window a write
 * followed by a read of the same address is answered out of the 8 KiB
 * cache, so a missing chip looks present and an address that aliases
 * back onto offset 0 looks like it does not. Throughput measurements
 * deliberately use the cached window, because that is how real code
 * reaches these devices. */
#define FRANK_XIP_FLASH_BASE         0x10000000u
#define FRANK_XIP_PSRAM_BASE         0x11000000u
#define FRANK_XIP_FLASH_NOCACHE_BASE 0x14000000u
#define FRANK_XIP_PSRAM_NOCACHE_BASE 0x15000000u

#endif /* FRANK_CORE2_BOARD_H */
