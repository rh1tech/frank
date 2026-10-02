/*
 * FRANK Core 2 — dual-RP2350 test firmware
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * board_config.h — compatibility shim for the drivers copied verbatim
 * from frank-msx.
 *
 * Those drivers (audio.c in particular) include "board_config.h" to
 * pick up pin numbers and clock defaults. Rather than patch them —
 * which would make future re-syncs from frank-msx painful — this header
 * provides the same symbols from the FRANK Core 2 board map.
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "pico.h"
#include "hardware/structs/sysinfo.h"
#include "hardware/vreg.h"

#include "frank_core2_board.h"

/* ---- Capabilities the frank-msx drivers gate on ---- */
#define HAS_HSTX 1
#define HAS_I2S  1

/* HDMI_BASE_PIN / I2S_* / SDCARD_PIN_* all come from the board map.
 * The CMakeLists also passes the SD and I2S pins as -D flags, because
 * the sdcard and audio libraries are compiled as separate targets that
 * never see this header. Guard against the resulting redefinition. */
#ifndef I2S_DATA_PIN
#define I2S_DATA_PIN       9
#endif
#ifndef I2S_CLOCK_PIN_BASE
#define I2S_CLOCK_PIN_BASE 10
#endif

/* ---- Clocking defaults ---- */
#ifndef CPU_CLOCK_MHZ
#define CPU_CLOCK_MHZ 252
#endif

#ifndef PSRAM_MAX_FREQ_MHZ
#define PSRAM_MAX_FREQ_MHZ 133
#endif

#ifndef FLASH_MAX_FREQ_MHZ
#define FLASH_MAX_FREQ_MHZ 66
#endif

#ifndef CPU_VOLTAGE
#  if CPU_CLOCK_MHZ >= 504
#    define CPU_VOLTAGE VREG_VOLTAGE_1_65
#  elif CPU_CLOCK_MHZ >= 300
#    define CPU_VOLTAGE VREG_VOLTAGE_1_60
#  else
#    define CPU_VOLTAGE VREG_VOLTAGE_1_50
#  endif
#endif

/* ---- PSRAM chip select ----
 *
 * The two halves of the board use different pins (master GPIO47, slave
 * GPIO0), and the packages differ too, so the package-select bit is a
 * reliable way for shared code to pick the right one. */
#define PSRAM_CS_PIN_RP2350A S_PSRAM_CS_PIN
#define PSRAM_CS_PIN_RP2350B M_PSRAM_CS_PIN

static inline unsigned get_psram_pin(void) {
    uint32_t package_sel = *((io_ro_32 *)(SYSINFO_BASE + SYSINFO_PACKAGE_SEL_OFFSET));
    return (package_sel & 1u) ? PSRAM_CS_PIN_RP2350A : PSRAM_CS_PIN_RP2350B;
}

#endif /* BOARD_CONFIG_H */
