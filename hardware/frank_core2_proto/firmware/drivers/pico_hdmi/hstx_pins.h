/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * FRANK NES - NES Emulator for RP2350
 * Originally from pico_hdmi by fliperama86 (https://github.com/fliperama86/pico_hdmi)
 * https://rh1.tech | https://github.com/rh1tech/frank-nes
 * SPDX-License-Identifier: Unlicense
 */

#ifndef HSTX_PINS_H
#define HSTX_PINS_H

// =============================================================================
// DVI/HSTX Output Pins (GPIO 12-19)
// =============================================================================
// These are the default pins for the RP2350's HSTX peripheral.
#define PIN_HSTX_CLK 12 // Clock pair base (12=CLK-, 13=CLK+)
#define PIN_HSTX_D0 14  // Data 0 pair base (14=D0-, 15=D0+)
#define PIN_HSTX_D1 16  // Data 1 pair base (16=D1-, 17=D1+)
#define PIN_HSTX_D2 18  // Data 2 pair base (18=D2-, 19=D2+)

#endif // HSTX_PINS_H
