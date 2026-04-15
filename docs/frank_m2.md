# FRANK M2

## Overview

FRANK M2 is the M2 GPIO layout variant, optimized for the RP2350A and its HSTX peripheral (better HDMI, faster RAM). Same feature set as the M1, different pinout. 0805 components, Pico 2 module via socket.

**Latest hardware revision:** 2.06

## Features

- Raspberry Pi Pico 2 (RP2350A) socket
- RP2040-Zero socket for USB keyboard/mouse → PS/2 conversion
- HDMI (HSTX-optimized), VGA, soft composite (RCA), external TFT
- TDA1387 stereo I2S DAC, PWM audio, audio amplifier (3.5mm jack)
- PS/2 keyboard and mouse (Mini-DIN 6-pin)
- Two DB9 gamepad ports (Famicom/NES)
- USB keyboard/mouse via RP2040-Zero running [ps2x2pico](https://github.com/No0ne/ps2x2pico) firmware
- MicroSD (TF) card slot (SPI)
- Tape input (3.5mm jack)
- USB-C or DC barrel jack (5-12V) power input

No dedicated PSRAM socket on M2. PSRAM goes on the Pico module: piggyback a QSPI PSRAM chip onto the flash of a clone Pico 2 (black boards with SOIC-8 flash), or use a Pimoroni Pico Plus 2 (PSRAM built in).

PS/2 and USB keyboard are mutually exclusive.

## BOM

See `docs/2_06/bom.html`.

## PCB fabrication

Gerbers are in `gerbers/`:

| Revision | File |
|----------|------|
| 2.06 | `frank_m2_2_06.zip` |

Specs: 2 layers, 1.6mm, HASL or ENIG, 1oz copper, 0.2mm/0.2mm trace/space.

## Assembly

See `docs/2_06/assembly.pdf`.

### SMD first

1. Stencil and paste
2. Place passives (smallest first)
3. Place ICs: LM2596S, TDA1387 (optional), TXS0104E, 74HCT74, CD4069UBE, 74HC4052
4. Reflow (~245°C peak)

### Through-hole

1. Pico 2 pin headers (2x20, 2.54mm)
2. RP2040-Zero header (optional)
3. Connectors: PS/2 Mini-DIN, HDMI, D-SUB 15-pin VGA, RCA, 3.5mm audio/tape, USB-A, USB-C, DC-005, DB9 gamepads (×2), MicroSD
4. DIP switches, buttons

Minimum: Pico header + one video output + PS/2 connector.

### RP2040-Zero (USB keyboard/mouse)

1. Flash [ps2x2pico](https://github.com/No0ne/ps2x2pico) ("Stacked" build)
2. Insert into socket
3. JP1: `ZERO`, `MOUSE CLK`, `MOUSE DATA` on

### First boot

1. Power on, check LED
2. Connect display
3. Hold BOOTSEL, plug USB, release, copy .uf2
4. SD card + keyboard

## Case

No official case design yet.

## Schematic

`docs/2_06/frank_m2.pdf`

## Firmware

M2 GPIO layout. Pick **M2** builds from [rh1tech/frank-*](https://github.com/rh1tech). Match the firmware to your video output.
