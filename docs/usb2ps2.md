# USB2PS2

## Overview

Active adapter that converts USB keyboards and mice to PS/2. Built on the RP2040. Draws power from the PS/2 port's +5V line, so no separate power supply needed. Plug a USB keyboard/mouse into one end, PS/2 cable into the FRANK board on the other.

**Latest hardware revision:** 1.03

## Features

- RP2040 (on-board or via Raspberry Pi Zero)
- USB Type-A host input
- PS/2 Mini-DIN 6-pin output
- Level shifter (3.3V ↔ 5V)
- UART
- Self-powered from PS/2 +5V

## BOM

See `docs/1_03/bom.html`.

## PCB fabrication

Gerbers are in `gerbers/`:

| Revision | File |
|----------|------|
| 1.00 | `USB2PS2Z_HW_1_00.zip` |
| 1.01 | `USB2PS2Z_HW_1_01.zip` |
| 1.02 | `USB2PS2Z_HW_1_02.zip` |
| 1.03 | `USB2PS2Z_HW_1_03.zip` |

Specs: 2 layers, 1.6mm, HASL or ENIG, 1oz copper.

## Assembly

See `docs/1_03/assembly.pdf`.

### SMD

1. Paste, place passives
2. Level shifter, ESD protection (CMZJ2)
3. Reflow

### Through-hole

1. Raspberry Pi Zero pin headers (if using Zero as MCU)
2. USB Type-A host connector, PS/2 Mini-DIN 6-pin socket
3. Buttons

### Firmware

1. Hold BOOTSEL, plug USB, release
2. Copy .uf2 firmware to the drive
3. Adapter translates USB HID → PS/2 automatically

### Testing

1. USB keyboard into the adapter
2. PS/2 output into FRANK board
3. Power on, type something

Avoid wireless keyboards and multi-function devices — they tend to cause issues.

## Case

STL files in `case/REV_1_0/`:

| File | Description |
|------|-------------|
| `bottom.stl` | Bottom |
| `top.stl` | Top |

PLA or PETG, 0.2mm layers, 20% infill.

## Schematic

`docs/1_03/usb2ps2.pdf`

Board photos: `docs/front.png`, `docs/back.png`
