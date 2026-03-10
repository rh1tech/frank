# Protea

Protea is an open-source, feature-rich serial terminal and Wi-Fi modem PCB, designed in KiCad and released under the GNU General Public License (version 3 or later). Inspired by David Hansel's](https://github.com/dhansel) [VersaTerm](https://github.com/dhansel/VersaTerm) and [Richard Bettridge's](https://richardbettridge.com/) [RS232 Serial Wi-Fi Modem](https://github.com/TheOldNet/vintage-computer-wifi-modem), Protea combines modern connectivity with classic terminal functionality.

## Key Features

* **Plug-and-Play Operation**: simply connect a standard USB keyboard and an HDMI display—no additional host system or software required. Protea powers on instantly, requiring no boot-up sequence or shutdown procedure.
* **Universal Serial I/O**. Supports both RS232 and TTL (3.3 V or 5 V) serial levels. Choose from fixed presets or any custom baud rate between 50 and 921 600 baud. Full hardware (RTS/CTS) and software (XOn/XOff) flow control ensure reliable data transfer.
* **Comprehensive VT100 Emulation**. Full support for VT100 attributes—including bold, underline, blink, inverse, double width, and double height—and 16 ANSI colors. Advanced control-sequence compatibility passes VTTest for VT52/VT100/VT102 conformance.
* **PETSCII Mode**. Emulates the PETSCII character set and control codes used by the Commodore 64, complete with an authentic PETSCII bitmap font.
* **RP2040-Powered**. Built around the Raspberry Pi RP2040 microcontroller, Protea delivers instant-on performance and robust processing for terminal emulation.
* **Built-In Wi-Fi Modem**. Integrates an ESP8266EX module with on-board antenna, providing transparent serial-to-Wi-Fi bridging for telnet, web-based interfaces, or custom network protocols. Enables remote terminal sessions over local networks or the Internet.
* **Highly Configurable**. Customizable color palettes, and savable configuration profiles mean you can tailor Protea to any workflow. A built-in menu system allows on-the-fly adjustments without recompiling firmware.

Protea brings together the convenience of modern hardware with the nostalgia and versatility of vintage terminal interfaces—ideal for hobbyists, retro-computing enthusiasts, and embedded systems developers.