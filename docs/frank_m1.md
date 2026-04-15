# FRANK M1

## Overview

FRANK M1 is the original board in the FRANK (regular) family. M1 GPIO layout, Pico / Pico 2 module via socket, 0805 components. An optional RP2040-Zero converts USB keyboards and mice to PS/2.

**Latest hardware revision:** 2.06

## Features

- Raspberry Pi Pico / Pico 2 socket (RP2040 or RP2350)
- RP2040-Zero socket for USB keyboard/mouse → PS/2 conversion
- HDMI, VGA, soft composite (RCA), and external TFT video outputs
- TDA1387 stereo I2S DAC, PWM audio, audio amplifier (3.5mm jack)
- Audio multiplexer (I2S / PWM / TDA select)
- PS/2 keyboard and mouse (Mini-DIN 6-pin)
- Two DB9 gamepad ports (Famicom/NES)
- USB keyboard/mouse via RP2040-Zero running [ps2x2pico](https://github.com/No0ne/ps2x2pico) firmware
- MicroSD (TF) card slot (SPI)
- 8MB PIO PSRAM (ESP-PSRAM64H, optional)
- Tape input (3.5mm jack)
- RS-232 serial (MAX232)
- USB-C or DC barrel jack (5-12V) power input
- Optional NCP1117 3.3V regulator

PS/2 and USB keyboard ports are mutually exclusive — pick one.

## BOM

See `docs/2_06/bom.html` (interactive, open in a browser).

## PCB fabrication

Gerbers are in `gerbers/`:

| Revision | File |
|----------|------|
| 2.05 | `frank_m1_2_05.zip` |
| 2.06 | `frank_m1_2_06.zip` |

Specs: 2 layers, 1.6mm, HASL or ENIG, 1oz copper, 0.2mm/0.2mm trace/space.

## Assembly

See `docs/2_06/assembly.pdf` for the placement diagram.

### SMD first

1. Stencil and paste all SMD pads
2. Place passives (0402, 0603 first — they're hardest to reach later)
3. Place ICs: LM2596S, TDA1387 (optional), TXS0104E, 74HCT74, CD4069UBE, 74HC4052, MAX232
4. Reflow (~245°C peak for lead-free)

### Through-hole

Work from shortest to tallest:

1. Pico pin headers (2x20, 2.54mm)
2. RP2040-Zero header (if you want USB keyboard/mouse)
3. Connectors: PS/2 Mini-DIN, HDMI, D-SUB 15-pin VGA, RCA, 3.5mm audio out, 3.5mm tape in, USB-A, USB-C, DC-005 barrel jack, DB9 gamepads (×2), MicroSD slot
4. DIP switches (JP1), tactile buttons

Minimum to get it running: Pico header, one video output, PS/2 connector.

### PSRAM (optional)

Most emulators need more than the Pico's 264KB RAM:

1. Solder ESP-PSRAM64H to the PSRAM pads (fine-pitch — flux + hot air)
2. Close JP5 solder jumper

### Modules

**Pico / Pico 2:** solder pin headers if bare, insert with USB port facing out.

**RP2040-Zero (USB keyboard/mouse):**
1. Flash [ps2x2pico](https://github.com/No0ne/ps2x2pico) ("Stacked" build)
2. Insert into socket
3. JP1 switches: `ZERO` on, `MOUSE CLK` on, `MOUSE DATA` on

### JP1 switches

| Switch | What it does |
|--------|-------------|
| ZERO | Powers the RP2040-Zero |
| MOUSE CLK | Mouse clock line (needed for USB mouse) |
| MOUSE DATA | Mouse data line (needed for USB mouse) |
| PSRAM | Enables PSRAM |
| TAPE IN | Enables tape input |

### Power

- USB-C (5V, ≥1A) under the Pico module — recommended
- DC barrel jack (5-12V, center positive)
- Optional: solder NCP1117 3.3V regulator. Without it, close JP4 to use the Pico's internal regulator (fine for simple setups, not recommended under heavy load)

### First boot

1. Power on via USB-C
2. Check power LED
3. Connect display
4. Hold BOOTSEL on the Pico, plug USB into a PC, release, drag .uf2 onto the drive
5. Insert SD card, connect keyboard

## Case

No official case design yet.

## Schematic

`docs/2_06/frank_m1.pdf`

## Firmware

M1 GPIO layout. Pick **M1** builds from [rh1tech/frank-*](https://github.com/rh1tech). Each build targets a specific video output, so match it to your display.
