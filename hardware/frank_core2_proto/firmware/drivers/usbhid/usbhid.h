/*
 * USB HID host driver
 * Based on TinyUSB HID host example
 * SPDX-License-Identifier: MIT
 */

#ifndef USBHID_H
#define USBHID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// USB HID State
//--------------------------------------------------------------------

// USB keyboard state accessible from wrapper
typedef struct {
    uint8_t keycode[6];     // Currently pressed keys (HID keycodes)
    uint8_t modifier;       // Modifier keys (shift, ctrl, alt, etc.)
    int has_key;            // Non-zero if a key event is pending
} usbhid_keyboard_state_t;

// USB mouse state
typedef struct {
    int16_t dx;             // Accumulated X movement
    int16_t dy;             // Accumulated Y movement
    int8_t wheel;           // Wheel movement
    uint8_t buttons;        // Button state (bit 0=left, 1=right, 2=middle)
    int has_motion;         // Non-zero if motion/button change occurred
} usbhid_mouse_state_t;

// USB gamepad state
typedef struct {
    int8_t axis_x;          // Left stick X: -127 to 127
    int8_t axis_y;          // Left stick Y: -127 to 127
    uint8_t dpad;           // D-pad: bit 0=up, 1=down, 2=left, 3=right
    uint16_t buttons;       // Buttons: bit 0=A, 1=B, 2=X, 3=Y, 4=L, 5=R, 6=Start, 7=Select
    int connected;          // Non-zero if gamepad is connected
} usbhid_gamepad_state_t;

//--------------------------------------------------------------------
// API Functions
//--------------------------------------------------------------------

void usbhid_init(void);
void usbhid_task(void);

int usbhid_keyboard_connected(void);
int usbhid_mouse_connected(void);

void usbhid_get_keyboard_state(usbhid_keyboard_state_t *state);
void usbhid_get_mouse_state(usbhid_mouse_state_t *state);
int usbhid_get_key_action(uint8_t *keycode, int *down);

/**
 * Get keyboard state as bitmask (same format as PS/2 keyboard)
 * Uses same KBD_STATE_* constants from ps2kbd_wrapper.h
 */
uint16_t usbhid_get_kbd_state(void);

/** Non-zero while Ctrl+Alt+Del are all currently held on a USB keyboard.
 *  Del is not exposed in the KBD_STATE bitmask; this chord is only used
 *  to soft-reset a running ROM. */
int usbhid_ctrl_alt_del_pressed(void);

/** Pop the next queued raw ASCII character from USB keyboard input.
 *  Returns a-z / A-Z (shift-aware), 0-9, space, or '\b' for Backspace.
 *  Returns -1 when empty. Mirrors ps2kbd_get_raw_char(). */
int usbhid_get_raw_char(void);

/** Check if a USB gamepad is connected (any slot) */
int usbhid_gamepad_connected(void);

/** Get combined gamepad state (all connected gamepads merged) */
void usbhid_get_gamepad_state(usbhid_gamepad_state_t *state);

/** Check if USB gamepad at specific slot (0 or 1) is connected */
int usbhid_gamepad_connected_idx(int idx);

/** Get gamepad state for specific slot (0 or 1) */
void usbhid_get_gamepad_state_idx(int idx, usbhid_gamepad_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* USBHID_H */
