/*
 * TinyUSB Configuration for USB Host HID (Keyboard/Mouse)
 * Uses native USB port for Host mode
 * 
 * This file is ONLY included when USB_HID_ENABLED is defined in CMake.
 * The usbhid include directory is not added to the build otherwise.
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// Defined by compiler flags for flexible configuration
#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

// RHPort number used for host
#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT 0
#endif

// RHPort max speed
#define BOARD_TUH_MAX_SPEED OPT_MODE_FULL_SPEED

// Enable Host mode (disables Device mode including CDC stdio!)
#define CFG_TUH_ENABLED 1
#define CFG_TUD_ENABLED 0

// Default is max speed that hardware controller supports
#define CFG_TUH_MAX_SPEED BOARD_TUH_MAX_SPEED

//--------------------------------------------------------------------
// PIO-USB Configuration (for secondary USB port on GPIO pins)
// Enable this to use a GPIO-based USB Host while keeping native USB for CDC
//--------------------------------------------------------------------

// Set to 1 to use PIO-USB for Host (requires pio-usb library)
// Set to 0 to use native USB port for Host (disables USB CDC stdio)
#ifndef CFG_TUH_RPI_PIO_USB
#define CFG_TUH_RPI_PIO_USB 0
#endif

#if CFG_TUH_RPI_PIO_USB
// PIO-USB GPIO pins (define these in board_config.h if using PIO-USB)
#ifndef USB_HOST_PIO_DP_PIN
#define USB_HOST_PIO_DP_PIN 20  // D+ pin, D- will be DP+1
#endif
#endif

//--------------------------------------------------------------------
// HOST CONFIGURATION
//--------------------------------------------------------------------

// Size of buffer for receiving and sending control requests
#define CFG_TUH_ENUMERATION_BUFSIZE 256

// Max number of devices (hub counts as 1, then each device behind hub)
// Need at least: 1 hub + 2 HID devices = 3, use 5 for safety
#define CFG_TUH_DEVICE_MAX 5

// Number of hub devices
#define CFG_TUH_HUB 1

// Max number of HID interfaces (each device can have multiple interfaces)
// Keyboard = 1 HID, some mice = 2 HID (boot + extra features)
#define CFG_TUH_HID 8

// XInput application driver. Registered via usbh_app_driver_get_cb() so
// Xbox 360 / Xbox One / XboxOG controllers (which do NOT enumerate as
// HID) are picked up alongside regular HID gamepads. Source:
// xinput_host.c/.h, by Ryan Wendland / usb64 (MIT).
#define CFG_TUH_XINPUT 1

// CDC host (disable - we don't need to connect to CDC devices)
#define CFG_TUH_CDC 0

// Vendor (disable)
#define CFG_TUH_VENDOR 0

// MSC host (disable - we use SD card)
#define CFG_TUH_MSC 0

//--------------------------------------------------------------------
// HID BUFFER SIZE
//--------------------------------------------------------------------

// Must be large enough to hold any HID report
#define CFG_TUH_HID_EPIN_BUFSIZE 64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H */
