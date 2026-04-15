# TurboFRANK M1P2

## Overview

Dual-MCU board: both RP2040 and RP2350A soldered on-board, no Pico module. A TS3USB221 USB multiplexer switches between the two chips, and there's a hardware power switch. M1 GPIO layout, TurboSound support.

**Latest hardware revision:** 1.01

## Features

- RP2040 + RP2350A on-board (both soldered)
- TS3USB221 USB multiplexer (switch between MCUs)
- Hardware power switch
- HDMI, VGA video
- TDA1387 I2S DAC + PWM audio + audio amplifier (3.5mm jack)
- Audio multiplexer (I2S / PWM / TDA)
- Two DB9 gamepad ports (Famicom/NES)
- USB host
- MicroSD (TF) card slot (SPI)
- Tape input (3.5mm jack)
- TurboSound
- Display connector
- USB-C or DC barrel jack power

## BOM

See `docs/1_01/bom.html`.

## PCB fabrication

Gerbers are in `gerbers/`:

| Revision | File |
|----------|------|
| 1.00 | `turbofrank_m1p2_1_00.zip` |
| 1.01 | `turbofrank_m1p2_1_01.zip` |

Specs: 2 layers, 1.6mm, ENIG recommended (QFN), 1oz copper, 0.15mm/0.15mm trace/space.

## Assembly

See `docs/1_01/assembly.pdf`.

Both MCUs are QFN packages — stencil + reflow oven strongly recommended.

### SMD (the hard part)

1. Paste all pads
2. Place RP2040 (QFN-56, 0.4mm pitch) and RP2350A (QFN-60, 0.4mm pitch)
3. Crystals, flash, decoupling caps
4. Remaining ICs: voltage regulator, TS3USB221, TDA1387, level shifters, logic, audio mux
5. Passives
6. Reflow

### Through-hole

1. Connectors: HDMI, D-SUB 15 VGA, 3.5mm audio/tape, USB-A, USB-C, DC barrel jack, DB9 gamepads (×2), MicroSD
2. Power switch
3. DIP switches, buttons

### First boot

1. Flip the power switch on
2. Check LED, connect HDMI
3. Hold BOOTSEL, USB to PC, release, copy .uf2
4. Use the MCU switch to select which chip to program
5. SD card + peripherals

## Case

STL files in `case/1_01/`:

| File | Description |
|------|-------------|
| `bottom.stl` | Bottom plate |
| `button.stl` | Button cap |
| `cover.stl` | Top cover |
| `middle.stl` | Middle frame |

PLA or PETG, 0.2mm layers, 20% infill.

## Schematic

`docs/1_01/turbofrank_m1p2.pdf`

## Firmware

M1 GPIO layout. Use the USB multiplexer to pick which MCU gets the firmware. **M1** builds from [rh1tech/frank-*](https://github.com/rh1tech). Match firmware to video output.
