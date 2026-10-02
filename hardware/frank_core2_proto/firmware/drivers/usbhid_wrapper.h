/*
 * frank-msx — fMSX for RP2350
 *
 * Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
 * https://github.com/rh1tech/frank-msx
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * USB HID Wrapper for fMSX on RP2350.
 *
 * Thin adapter between the TinyUSB host driver (drivers/usbhid) and the
 * platform layer. When USB_HID_ENABLED is defined the wrapper:
 *   - maps HID keyboard events to PS/2-style Duke3D scancodes so they
 *     flow through the same path as real PS/2 keys;
 *   - exposes USB gamepad state as a BTN_* bitmask (EMULib) for the
 *     joystick layer;
 *   - accumulates mouse deltas into an MSX-format cumulative position
 *     word for the Mouse() hook.
 *
 * When USB_HID_ENABLED is not defined the header only provides inline
 * stubs, so callers can include it unconditionally.
 */

#ifndef USBHID_WRAPPER_H
#define USBHID_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USB_HID_ENABLED

/* Lifecycle */
void usbhid_wrapper_init(void);
void usbhid_wrapper_tick(void);

/* Device presence */
int usbhid_wrapper_keyboard_connected(void);
int usbhid_wrapper_mouse_connected(void);
int usbhid_wrapper_gamepad_connected(void);

/* Drain one queued keyboard event. Returns 1 if `*pressed`/`*key` were
 * filled, 0 otherwise. `key` is a Duke3D-style PS/2 scancode (same
 * alphabet as drivers/ps2/ps2kbd_wrapper.c). */
int usbhid_wrapper_get_key(int *pressed, unsigned char *key);

/* Current gamepad state as EMULib BTN_* mask (merged across slots). */
unsigned int usbhid_wrapper_get_joystick(void);

/* Per-slot gamepad state (idx = 0 or 1). Returns 0 if that slot isn't
 * populated — the caller composes P1 / P2 as it sees fit. */
unsigned int usbhid_wrapper_get_joystick_idx(int idx);

/* MSX-format cumulative mouse state for port N (0 or 1):
 *   bits  0..7  = X position (cumulative, wraps mod 256)
 *   bits  8..15 = Y position
 *   bits 16..17 = buttons (F1=left, F2=right) in the 0x10030 layout
 *                 expected by MSX.c when it masks (MouState>>12)&0x0030.
 * Returns 0 if no USB mouse is connected.
 */
unsigned int usbhid_wrapper_get_mouse(int port);

#else /* !USB_HID_ENABLED */

static inline void         usbhid_wrapper_init(void)                     {}
static inline void         usbhid_wrapper_tick(void)                     {}
static inline int          usbhid_wrapper_keyboard_connected(void)       { return 0; }
static inline int          usbhid_wrapper_mouse_connected(void)          { return 0; }
static inline int          usbhid_wrapper_gamepad_connected(void)        { return 0; }
static inline int          usbhid_wrapper_get_key(int *p, unsigned char *k) { (void)p; (void)k; return 0; }
static inline unsigned int usbhid_wrapper_get_joystick(void)             { return 0; }
static inline unsigned int usbhid_wrapper_get_joystick_idx(int idx)      { (void)idx; return 0; }
static inline unsigned int usbhid_wrapper_get_mouse(int port)            { (void)port; return 0; }

#endif /* USB_HID_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* USBHID_WRAPPER_H */
