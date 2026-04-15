# HDMI2VGA

## Overview

Passive adapter that converts HDMI to VGA and/or soft composite. Plugs directly into a FRANK board's HDMI port — no external power needed. Use it to connect CRT monitors or older VGA displays.

**Latest hardware revision:** 2.04

## Features

- HDMI male input (plugs into FRANK board)
- D-SUB 15-pin VGA output
- Soft composite video output
- Passive (powered from HDMI)

## BOM

See `docs/2_04/bom.html`.

## PCB fabrication

Gerbers are in `gerbers/`:

| Revision | File |
|----------|------|
| 1.00 | `hdmi2vga_rev_1_00.zip` |
| 2.00–2.04 | `hdmi2vga_rev_2_0x.zip` |

Specs: 2 layers, 1.6mm, HASL or ENIG, 1oz copper.

## Assembly

See `docs/2_04/assembly.pdf`.

### SMD

1. Paste, place resistors (DAC ladder network) and capacitors
2. Reflow

### Through-hole

1. HDMI male connector
2. D-SUB 15-pin VGA connector
3. Composite connector (if applicable)
4. Buttons (if present)

Pay attention to HDMI and VGA connector alignment — sloppy joints cause signal integrity issues.

### Testing

1. Plug into FRANK board's HDMI port
2. Connect VGA monitor
3. Power on FRANK
4. Check for clean video output

## Case

### Rev 2.02 (`case/2_02/`) and 2.03 (`case/2_03/`)

| File | Description |
|------|-------------|
| `bottom.stl` | Bottom |
| `top.stl` | Top |

PLA or PETG, 0.2mm layers, 20% infill.

## Schematic

`docs/2_04/hdmi2vga.pdf`
