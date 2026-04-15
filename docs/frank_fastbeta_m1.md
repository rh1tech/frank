# FRANK FastBeta M1

## Overview

A stripped-down FRANK board built specifically for [FastBeta](https://fastbeta.pro/) — a ZX Spectrum emulator with near-authentic 48/128/+3e behavior, dual AY-3-8910 sound, SAA1099, BetaDisk, and DivIDE. M1 GPIO layout, Pico module socket, HDMI + VGA. No composite, no audio multiplexer.

**Latest hardware revision:** 1.04

## Features

- Raspberry Pi Pico / Pico 2 socket
- HDMI and VGA video
- I2S audio
- PS/2 keyboard and mouse (Mini-DIN 6-pin)
- Two DB9 gamepad ports (Famicom/NES)
- MicroSD (TF) card slot (SPI)
- 8MB PIO PSRAM (ESP-PSRAM64H, optional)
- Tape input (3.5mm jack)
- USB-C or DC barrel jack (5-12V)
- MP1584EN DC-DC converter (rev 1.04) / L7805ABD2T-TR (rev 1.0)

## BOM

See `docs/1_04/bom.html`.

## PCB fabrication

Gerbers are in `gerbers/`:

| Revision | File |
|----------|------|
| 1.00 | `fastbeta_m1_1_00.zip` |
| 1.04 | `fastbeta_m1_1_04.zip` |

Specs: 2 layers, 1.6mm, HASL or ENIG, 1oz copper.

## Assembly

See `docs/1_04/assembly.pdf`.

### SMD first

1. Paste, place passives
2. Place ICs: MP1584EN, level shifters
3. HDMI resistor network: 820R, 360R, 820R, 360R, 820R, 360R, 75R, 75R
4. Reflow (~245°C)

### Through-hole

1. Pico pin headers (2x20, 2.54mm)
2. Connectors: PS/2 Mini-DIN, HDMI, USB-C, DC-005, DB9 gamepads (×2), MicroSD, 3.5mm tape
3. DIP switches, buttons

### PSRAM (optional)

1. Solder ESP-PSRAM64H
2. Close JP5

### 3.3V regulator (optional)

Solder AMS1117 for better stability, or close JP4 to use Pico's internal regulator.

### First boot

1. Power on, HDMI display
2. Hold BOOTSEL, plug USB, release, copy FastBeta .uf2
3. SD card with ZX Spectrum disk images, PS/2 keyboard

## Case

STL files in `case/1_04/`:

| File | Description |
|------|-------------|
| `body_hdmi.stl` | Body (HDMI) |
| `body_vga.stl` | Body (VGA) |
| `bottom_hdmi.stl` | Bottom (HDMI) |
| `bottom_vga.stl` | Bottom (VGA) |
| `cover.stl` | Top cover |

PLA or PETG, 0.2mm layers, 20% infill.

## Schematic

`docs/1_04/fastbeta_m1.pdf`

## Firmware

Primarily for [FastBeta](https://fastbeta.pro/) (ZX Spectrum 48K/128K/+3e, 2×AY-3-8910, SAA1099, BetaDisk, DivIDE). Also runs other M1-layout firmware from [rh1tech/frank-*](https://github.com/rh1tech).

More info: [fastbeta.pro](https://fastbeta.pro/), [fastbeta.pro/rh1tech-fb](https://fastbeta.pro/rh1tech-fb)
