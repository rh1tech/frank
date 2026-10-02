#
# frank-msx — fMSX for RP2350
#
# Copyright (c) 2026 Mikhail Matveev <xtreme@rh1.tech>
# https://github.com/rh1tech/frank-msx
# SPDX-License-Identifier: GPL-3.0-or-later
#

# USB HID Driver for DOOM on RP2350
# Provides keyboard and mouse input via USB Host
#
# SPDX-License-Identifier: MIT

add_library(usbhid INTERFACE)

# Check if USB HID is enabled
if(USB_HID_ENABLED)
    message(STATUS "USB HID Host enabled")
    message(STATUS "  NOTE: USB CDC stdio will be DISABLED when USB HID is enabled!")
    
    # Add the actual source files when enabled
    target_sources(usbhid INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/hid_app.c
        ${CMAKE_CURRENT_LIST_DIR}/xinput_host.c
        ${CMAKE_CURRENT_LIST_DIR}/usbhid_wrapper.c
    )

    # Expose both this dir (tusb_config.h + usbhid.h) and the parent dir
    # (usbhid_wrapper.h — also the stubbed header when USB_HID is off).
    target_include_directories(usbhid INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
    )

    target_compile_definitions(usbhid INTERFACE
        USB_HID_ENABLED=1
    )
    
    # Link TinyUSB Host
    target_link_libraries(usbhid INTERFACE
        tinyusb_host
        tinyusb_board
    )
    
    # If using PIO-USB for secondary USB port
    if(USB_HID_PIO_USB)
        message(STATUS "USB HID using PIO-USB on GPIO${USB_HID_PIO_DP_PIN}")
        target_compile_definitions(usbhid INTERFACE
            CFG_TUH_RPI_PIO_USB=1
            USB_HOST_PIO_DP_PIN=${USB_HID_PIO_DP_PIN}
        )
        # Would need to link pio_usb library here if using PIO-USB
        # target_link_libraries(usbhid INTERFACE pio_usb)
    endif()
else()
    message(STATUS "USB HID Host disabled (use -DUSB_HID_ENABLED=1 to enable)")
    message(STATUS "  USB CDC stdio remains available for debug output")
    # Stub functions are provided inline in drivers/usbhid_wrapper.h
    # Do NOT include the usbhid directory to avoid tusb_config conflicts
endif()
