# TurboFRANK M1P1

## Overview

The kitchen-sink board. M1 layout, 0805 components, Pico socket — same easy soldering as regular FRANK, but with a USB hub (MW7211A), RS-232 (MAX232), TurboSound (two AY-3-8910), and a PAM8403D speaker amp bolted on. USB keyboard/mouse works through either an RP2040-Zero module or the on-board hub.

**Latest hardware revision:** 1.07

## Features

- Raspberry Pi Pico / Pico 2 socket
- RP2040-Zero socket (USB keyboard/mouse → PS/2, option 1)
- MW7211A USB hub with two USB-A ports (USB keyboard/mouse, option 2)
- HDMI, VGA, soft composite
- TDA1387 I2S DAC + PWM audio + TurboSound (2×AY-3-8910)
- PAM8403D speaker amplifier (JP10/JP11 connectors)
- 3.5mm headphone jack, beeper output
- PS/2 keyboard and mouse (Mini-DIN 6-pin)
- Two DB9 gamepad ports (Famicom/NES)
- RS-232 serial (DB9, MAX232)
- MicroSD (TF) card slot (SPI)
- 8MB PIO PSRAM (ESP-PSRAM64H, optional)
- Tape input (3.5mm jack)
- USB-C or DC barrel jack (5-12V)
- Optional NCP1117 3.3V regulator

PS/2 and USB keyboard are mutually exclusive.

## BOM

See `docs/1_07/bom.html`.

## PCB fabrication

Gerbers are in `gerbers/`. Use rev 1.07 for new builds — older revisions have known issues (see revision history below).

| Revision | File |
|----------|------|
| 1.00–1.07 | `turbofrank_m1p1_hw_rev_1_0x.zip` |

Specs: 2 layers, 1.6mm, HASL or ENIG, 1oz copper, 0.2mm/0.2mm trace/space.

## Assembly

See `docs/1_07/assembly.pdf`.

### SMD first

1. Paste, place passives
2. Place ICs: LM2596S, TDA1387, TXS0104E, 74HCT74, CD4069UBE, MAX232, MW7211A (optional), PAM8403D (optional)
3. Reflow (~245°C)

### Through-hole

1. Pico pin headers
2. RP2040-Zero header (if using option 1 for USB input)
3. Connectors: PS/2 Mini-DIN, HDMI, D-SUB 15 VGA, RCA, 3.5mm audio/tape, USB-A hub ports (×2), USB-C, DC-005, DB9 gamepads (×2), RS-232 DB9, MicroSD, speaker connectors (JP10/JP11)
4. DIP switches (JP1), buttons

Minimum: Pico header + one video output + PS/2 connector.

### PSRAM (optional)

1. Solder ESP-PSRAM64H
2. Most emulators need it

### JP1 switches

| Switch | What it does |
|--------|-------------|
| PWM L/R | Software audio |
| TDA L/R | DAC audio |
| TRS L/R | TurboSound audio |
| BEEP | Beeper (PWM only) |
| SHND | Amplifier shutdown (mute without power-off) |
| MS DATA / MS CLK | Mouse (both needed) |
| GPAD1 / GPAD2 | Gamepads |
| TAPE IN | Tape loading |
| PSRAM | PSRAM enable |
| ZERO VBUS | Power RP2040-Zero |
| HUB VBUS | Power USB hub |
| RS232 TX/RX/CTS/RTS | Serial port |

### USB keyboard/mouse

**Option 1 — RP2040-Zero:**
Flash [ps2x2pico](https://github.com/No0ne/ps2x2pico) ("Stacked"), insert, JP1: `ZERO VBUS` + `MS CLK` + `MS DATA` on. Leave MW7211A (U9) unsoldered, `HUB VBUS` off.

**Option 2 — on-board hub (MW7211A):**
Solder U9, JP1: `HUB VBUS` + `MS CLK` + `MS DATA` on. Leave RP2040-Zero socket empty. Connect J21 (1.5mm JST, max 15cm cable).

### Solder jumpers

| Jumper | What it does |
|--------|-------------|
| JP2 | Close when PSRAM is soldered |
| JP3 | Close to use Pico's internal 3.3V regulator (skip if NCP1117 is populated) |
| JP4 | Mono audio mode |

### First boot

1. Power on, check LED
2. Display, hold BOOTSEL, plug USB, release, copy .uf2
3. Test hub ports, RS-232 if populated
4. SD card + keyboard

## Case

STL files in `case/1_06/`:

| File | Description |
|------|-------------|
| `bottom.stl` | Bottom plate |
| `top.stl` | Top cover |

PLA or PETG, 0.2mm layers, 20% infill.

## Schematic

`docs/1_07/turbofrank_m1p1.pdf`

Board photos: `docs/front.png`, `docs/back.png`

## Firmware

M1 GPIO layout. Pick **M1** builds from [rh1tech/frank-*](https://github.com/rh1tech).

## Revision history

| Rev | Notes |
|-----|-------|
| 1.07 | PAM8403D speaker amp, MW7211A USB hub, corrected TurboSound resistors |
| 1.03 | Stable, needs manual TurboSound resistor mods |
| 1.01 | Tape loading bug — skip |
| 1.00 | Jumper track errors — skip |
