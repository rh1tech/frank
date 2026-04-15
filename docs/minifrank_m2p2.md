# MiniFRANK M2P2

## Overview

Credit-card-sized board. RP2350A soldered directly as QFN (no Pico module), 0603 components throughout. M2 GPIO layout. Has an ESP-01S socket for WiFi (runs [frank-netcard](https://github.com/rh1tech/frank-netcard) AT modem firmware, pairs with the [frank-manul](https://github.com/rh1tech/frank-manul) text browser). MW7211A USB hub handles keyboard/mouse.

**Latest hardware revision:** 1.04

## Features

- RP2350A on-board (QFN)
- ESP-01S WiFi socket (ESP8266, for frank-netcard)
- MW7211A USB 1.1 hub for keyboard/mouse
- HDMI, VGA, TFT video
- TDA1387 I2S DAC (optional) or PWM audio (default)
- PS/2 keyboard and mouse (Mini-DIN 6-pin)
- Two DB9 gamepad ports (Famicom/NES)
- MicroSD (TF) card slot (SPI)
- Tape input (3.5mm jack)
- NCP1117 3.3V regulator
- USB-C (power + programming)
- BOOT button

## JP1 switches

| Switch | What it does |
|--------|-------------|
| AUDIO | PWM (default) or TDA1387 DAC |
| USB | Direct programming vs. USB hub mode |
| MCLK / MDAT | Mouse clock/data |

`USB` must be OFF when flashing firmware.

## BOM

See `docs/1_04/bom.html`.

## PCB fabrication

Gerbers are in `gerbers/`:

| Revision | File |
|----------|------|
| 1.02 | `minifrank_m2p2_1_02.zip` |
| 1.03 | `minifrank_m2p2_1_03.zip` |
| 1.04 | `minifrank_m2p2_1_04.zip` |

Specs: 2 layers, 1.6mm, ENIG recommended (QFN), 1oz copper, 0.15mm/0.15mm trace/space.

## Assembly

See `docs/1_04/assembly.pdf`.

The RP2350A is a QFN-60 at 0.4mm pitch — stencil + reflow oven strongly recommended. Start with the smallest components; they get buried fast.

### SMD

1. Paste all pads
2. Place RP2350A
3. Crystal, flash, decoupling caps
4. NCP1117 3.3V regulator (required)
5. TDA1387 (optional), MW7211A (optional)
6. Passives
7. Reflow

### Through-hole

1. Connectors: PS/2 Mini-DIN, HDMI, D-SUB 15 VGA, 3.5mm audio/tape, USB-C, DB9 gamepads (×2), MicroSD
2. ESP-01S header (2x4, 2.54mm female)
3. DIP switches (JP1), BOOT button

### WiFi (optional)

1. Get an ESP-01S (ESP8266, 1MB flash)
2. Flash [frank-netcard](https://github.com/rh1tech/frank-netcard) via USB-to-serial adapter
3. Plug it into the socket
4. frank-manul stores WiFi credentials in flash

### Flashing firmware

1. JP1 `USB` switch → OFF
2. Hold BOOT, plug USB-C into PC, release
3. Board shows up as `RPI-RP2` drive
4. Copy .uf2, board reboots automatically

### First boot

1. Power on, check LED, connect display
2. SD card + PS/2 keyboard
3. Test WiFi with frank-manul if ESP-01S is installed

## Case

### Rev 1.02 (`case/1_02/`)

| File | Description |
|------|-------------|
| `bottom.stl` | Bottom |
| `cover.stl` | Cover |
| `top.stl` | Top |

### Rev 1.03 (`case/1_03/`)

| File | Description |
|------|-------------|
| `bottom.stl` | Bottom |
| `button.stl` | Button cap |
| `side_b_1.stl` / `side_b_2.stl` | Back panels |
| `side_s_1.stl` / `side_s_2.stl` | Side panels |
| `top.stl` | Top |

PLA or PETG, 0.2mm layers, 20% infill.

## Schematic

`docs/1_04/minifrank_m2p2.pdf`

## Firmware

M2 GPIO layout. Pick **M2** builds from [rh1tech/frank-*](https://github.com/rh1tech). Match firmware to video output. `USB` switch must be OFF to flash.
