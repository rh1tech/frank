/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * USB HID wrapper for fMSX on RP2350.
 *
 * Bridges the TinyUSB host driver (hid_app.c) to the platform layer:
 *   - Converts USB HID keyboard events into the Duke3D-style PS/2
 *     scancodes used by drivers/ps2/ps2kbd_wrapper.c so the rest of the
 *     platform treats USB and PS/2 keys identically.
 *   - Merges USB gamepad slots into a single EMULib BTN_* joystick mask.
 *   - Accumulates mouse deltas into an MSX-format cumulative position
 *     word suitable for returning from Mouse().
 */

#include "usbhid_wrapper.h"
#include "usbhid.h"

#include <stdint.h>
#include <string.h>

/* EMULib BTN_* constants (mirror src/EMULib/EMULib.h). Inlined so this
 * TU doesn't need the EMULib include path. */
#define BTN_LEFT     0x0001
#define BTN_RIGHT    0x0002
#define BTN_UP       0x0004
#define BTN_DOWN     0x0008
#define BTN_FIREA    0x0010
#define BTN_FIREB    0x0020
#define BTN_FIREL    0x0040
#define BTN_FIRER    0x0080
#define BTN_START    0x0100
#define BTN_SELECT   0x0200
#define BTN_FIREX    0x0800
#define BTN_FIREY    0x1000

/* Duke3D scancodes — must match the enum in drivers/ps2/ps2kbd_wrapper.c
 * and the PSC_* enum in src/Pico/platform.c. Only the subset we can map
 * from a USB HID keycode is listed here. */
#define PSC_Escape      0x01
#define PSC_1           0x02
#define PSC_2           0x03
#define PSC_3           0x04
#define PSC_4           0x05
#define PSC_5           0x06
#define PSC_6           0x07
#define PSC_7           0x08
#define PSC_8           0x09
#define PSC_9           0x0A
#define PSC_0           0x0B
#define PSC_Minus       0x0C
#define PSC_Equals      0x0D
#define PSC_BackSpace   0x0E
#define PSC_Tab         0x0F
#define PSC_Q           0x10
#define PSC_W           0x11
#define PSC_E           0x12
#define PSC_R           0x13
#define PSC_T           0x14
#define PSC_Y           0x15
#define PSC_U           0x16
#define PSC_I           0x17
#define PSC_O           0x18
#define PSC_P           0x19
#define PSC_LBr         0x1A
#define PSC_RBr         0x1B
#define PSC_Return      0x1C
#define PSC_LCtrl       0x1D
#define PSC_A           0x1E
#define PSC_S           0x1F
#define PSC_D           0x20
#define PSC_F           0x21
#define PSC_G           0x22
#define PSC_H           0x23
#define PSC_J           0x24
#define PSC_K           0x25
#define PSC_L           0x26
#define PSC_Semi        0x27
#define PSC_Quote       0x28
#define PSC_Tilde       0x29
#define PSC_LShift      0x2A
#define PSC_BkSlash     0x2B
#define PSC_Z           0x2C
#define PSC_X           0x2D
#define PSC_C           0x2E
#define PSC_V           0x2F
#define PSC_B           0x30
#define PSC_N           0x31
#define PSC_M           0x32
#define PSC_Comma       0x33
#define PSC_Period      0x34
#define PSC_Slash       0x35
#define PSC_RShift      0x36
#define PSC_LAlt        0x38
#define PSC_Space       0x39
#define PSC_CapsLk      0x3A
#define PSC_F1          0x3B
#define PSC_F2          0x3C
#define PSC_F3          0x3D
#define PSC_F4          0x3E
#define PSC_F5          0x3F
#define PSC_F6          0x40
#define PSC_F7          0x41
#define PSC_F8          0x42
#define PSC_F9          0x43
#define PSC_F10         0x44
#define PSC_F11         0x57
#define PSC_F12         0x58
#define PSC_UpA         0x5A
#define PSC_Insert      0x5E
#define PSC_Delete      0x5F
#define PSC_Home        0x61
#define PSC_End         0x62
#define PSC_PgUp        0x63
#define PSC_PgDn        0x64
#define PSC_RAlt        0x65
#define PSC_RCtrl       0x66
#define PSC_DownA       0x6A
#define PSC_LeftA       0x6B
#define PSC_RightA      0x6C

/* --- Mouse accumulator ------------------------------------------------
 *
 * MSX.c's Mouse() contract is cumulative: it stores the returned X/Y
 * and subtracts the previous sample to derive motion. We keep an 8-bit
 * counter that wraps naturally, and let fMSX compute `Old - New`.
 */
static uint8_t  s_mouse_x = 0;
static uint8_t  s_mouse_y = 0;
static uint8_t  s_mouse_buttons = 0;   /* bit 0 = left, bit 1 = right */

static int s_initialized = 0;

/* Clamp per-tick delta so a runaway report doesn't teleport the pointer
 * several screens. 64 counts/tick is plenty for a 60 Hz poll. */
#define MOUSE_MAX_DELTA 64

static inline int8_t clamp_delta(int16_t v) {
    if (v >  MOUSE_MAX_DELTA) return  MOUSE_MAX_DELTA;
    if (v < -MOUSE_MAX_DELTA) return -MOUSE_MAX_DELTA;
    return (int8_t)v;
}

/* HID Usage ID -> PS/2 Duke3D scancode. Covers the keys platform.c
 * already maps to the MSX matrix plus the UI overlay keysyms. */
static unsigned char hid_to_psc(uint8_t hid) {
    /* Modifier pseudo-keycodes emitted by hid_app.c */
    switch (hid) {
        case 0xE0: return PSC_LCtrl;   /* Ctrl  (shared L/R) */
        case 0xE1: return PSC_LShift;  /* Shift (shared L/R) */
        case 0xE2: return PSC_LAlt;    /* Alt   (shared L/R) */
    }

    /* Letters A..Z (0x04..0x1D) */
    if (hid >= 0x04 && hid <= 0x1D) {
        static const unsigned char letters[26] = {
            PSC_A, PSC_B, PSC_C, PSC_D, PSC_E, PSC_F, PSC_G, PSC_H,
            PSC_I, PSC_J, PSC_K, PSC_L, PSC_M, PSC_N, PSC_O, PSC_P,
            PSC_Q, PSC_R, PSC_S, PSC_T, PSC_U, PSC_V, PSC_W, PSC_X,
            PSC_Y, PSC_Z,
        };
        return letters[hid - 0x04];
    }

    /* Number row: 1..9 then 0 (HID 0x1E..0x27) */
    if (hid >= 0x1E && hid <= 0x26) {
        static const unsigned char nums[9] = {
            PSC_1, PSC_2, PSC_3, PSC_4, PSC_5, PSC_6, PSC_7, PSC_8, PSC_9,
        };
        return nums[hid - 0x1E];
    }
    if (hid == 0x27) return PSC_0;

    /* Function keys F1..F12 */
    if (hid >= 0x3A && hid <= 0x45) {
        static const unsigned char fns[12] = {
            PSC_F1, PSC_F2, PSC_F3, PSC_F4,  PSC_F5,  PSC_F6,
            PSC_F7, PSC_F8, PSC_F9, PSC_F10, PSC_F11, PSC_F12,
        };
        return fns[hid - 0x3A];
    }

    switch (hid) {
        case 0x28: return PSC_Return;
        case 0x29: return PSC_Escape;
        case 0x2A: return PSC_BackSpace;
        case 0x2B: return PSC_Tab;
        case 0x2C: return PSC_Space;
        case 0x2D: return PSC_Minus;
        case 0x2E: return PSC_Equals;
        case 0x2F: return PSC_LBr;
        case 0x30: return PSC_RBr;
        case 0x31: return PSC_BkSlash;
        case 0x33: return PSC_Semi;
        case 0x34: return PSC_Quote;
        case 0x35: return PSC_Tilde;
        case 0x36: return PSC_Comma;
        case 0x37: return PSC_Period;
        case 0x38: return PSC_Slash;
        case 0x39: return PSC_CapsLk;

        /* Navigation */
        case 0x49: return PSC_Insert;
        case 0x4A: return PSC_Home;
        case 0x4B: return PSC_PgUp;
        case 0x4C: return PSC_Delete;
        case 0x4D: return PSC_End;
        case 0x4E: return PSC_PgDn;

        /* Arrows */
        case 0x4F: return PSC_RightA;
        case 0x50: return PSC_LeftA;
        case 0x51: return PSC_DownA;
        case 0x52: return PSC_UpA;

        /* Keypad Enter */
        case 0x58: return PSC_Return;

        default: return 0;
    }
}

/* --- Gamepad mapping --------------------------------------------------
 *
 * hid_app.c fills usbhid_gamepad_state_t with its own bit layout (see
 * process_gamepad_report). We translate that to EMULib BTN_* constants
 * so the platform layer can OR the USB state into the NES/SNES pad's
 * accumulator directly.
 */
static unsigned int gamepad_to_btn_mask(const usbhid_gamepad_state_t *gp) {
    unsigned int m = 0;
    if (!gp || !gp->connected) return 0;

    /* D-pad (hid_app.c: bit 0=up, 1=down, 2=left, 3=right). */
    if (gp->dpad & 0x01) m |= BTN_UP;
    if (gp->dpad & 0x02) m |= BTN_DOWN;
    if (gp->dpad & 0x04) m |= BTN_LEFT;
    if (gp->dpad & 0x08) m |= BTN_RIGHT;

    /* Buttons (hid_app.c process_gamepad_report layout):
     *   0x0001=A 0x0002=B 0x0004=X 0x0008=Y
     *   0x0010=L 0x0020=R 0x0040=Start 0x0080=Select  */
    if (gp->buttons & 0x0001) m |= BTN_FIREA;
    if (gp->buttons & 0x0002) m |= BTN_FIREB;
    if (gp->buttons & 0x0004) m |= BTN_FIREX;
    if (gp->buttons & 0x0008) m |= BTN_FIREY;
    if (gp->buttons & 0x0010) m |= BTN_FIREL;
    if (gp->buttons & 0x0020) m |= BTN_FIRER;
    if (gp->buttons & 0x0040) m |= BTN_START;
    if (gp->buttons & 0x0080) m |= BTN_SELECT;
    return m;
}

/* --- Lifecycle -------------------------------------------------------- */

void usbhid_wrapper_init(void) {
    if (s_initialized) return;
    usbhid_init();
    s_mouse_x = s_mouse_y = 0;
    s_mouse_buttons = 0;
    s_initialized = 1;
}

void usbhid_wrapper_tick(void) {
    if (!s_initialized) return;

    /* Drive the TinyUSB host stack. */
    usbhid_task();

    /* Drain mouse deltas and accumulate into the MSX cumulative pos. */
    usbhid_mouse_state_t ms;
    usbhid_get_mouse_state(&ms);
    if (ms.dx || ms.dy || ms.buttons != s_mouse_buttons) {
        s_mouse_x = (uint8_t)(s_mouse_x + clamp_delta(ms.dx));
        /* hid_app.c already inverts Y so up-on-mouse == up-in-game. */
        s_mouse_y = (uint8_t)(s_mouse_y + clamp_delta(ms.dy));
        s_mouse_buttons = ms.buttons & 0x03;
    }
}

/* --- Public API ------------------------------------------------------- */

int usbhid_wrapper_keyboard_connected(void) {
    return s_initialized ? usbhid_keyboard_connected() : 0;
}

int usbhid_wrapper_mouse_connected(void) {
    return s_initialized ? usbhid_mouse_connected() : 0;
}

int usbhid_wrapper_gamepad_connected(void) {
    return s_initialized ? usbhid_gamepad_connected() : 0;
}

int usbhid_wrapper_get_key(int *pressed, unsigned char *key) {
    if (!s_initialized || !pressed || !key) return 0;

    uint8_t hid;
    int down;
    while (usbhid_get_key_action(&hid, &down)) {
        unsigned char psc = hid_to_psc(hid);
        if (psc) {
            *pressed = down;
            *key = psc;
            return 1;
        }
        /* Skip unmapped keycodes and keep draining. */
    }
    return 0;
}

unsigned int usbhid_wrapper_get_joystick(void) {
    if (!s_initialized) return 0;

    unsigned int mask = 0;
    usbhid_gamepad_state_t gp;

    usbhid_get_gamepad_state_idx(0, &gp);
    mask |= gamepad_to_btn_mask(&gp);

    usbhid_get_gamepad_state_idx(1, &gp);
    mask |= gamepad_to_btn_mask(&gp);

    return mask;
}

unsigned int usbhid_wrapper_get_joystick_idx(int idx) {
    if (!s_initialized || idx < 0 || idx > 1) return 0;
    if (!usbhid_gamepad_connected_idx(idx)) return 0;
    usbhid_gamepad_state_t gp;
    usbhid_get_gamepad_state_idx(idx, &gp);
    return gamepad_to_btn_mask(&gp);
}

unsigned int usbhid_wrapper_get_mouse(int port) {
    /* Only port 0 is wired to the USB mouse; port 1 stays idle unless
     * the user plugs a second mouse (not currently supported). */
    if (!s_initialized || port != 0 || !usbhid_mouse_connected()) return 0;

    unsigned int v = (unsigned int)s_mouse_x & 0xFF;
    v |= ((unsigned int)s_mouse_y & 0xFF) << 8;
    /* MSX.c extracts buttons as (MouState>>12)&0x30, which corresponds
     * to bits 16 (F1=left) and 17 (F2=right). */
    if (s_mouse_buttons & 0x01) v |= (1u << 16);
    if (s_mouse_buttons & 0x02) v |= (1u << 17);
    return v;
}
