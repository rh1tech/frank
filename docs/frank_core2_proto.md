# FRANK Core 2 Proto assembly and usage guide

<p align="center">
  <img src="./boards/3d/frank_core2_proto-iso.png" alt="FRANK Core 2 Proto 3D render" width="640">
</p>

FRANK Core 2 Proto is the bring-up board for the dual-RP2350 subsystem generation that later shipped as [Core 2U](./frank_core2u.md). It is a square 55 × 55 mm, four-layer board carrying an RP2350B master and an RP2350A slave, each with its own 16 MB flash, 8 MB PSRAM, 12 MHz oscillator and USB-C — and nothing else beyond video, sound and storage.

Read it as Core 2U with the right-hand 12 mm removed. Core 2U adds the MW7211A hub, two USB Type-A host ports, the TS3USB221 host multiplexer, the CD4069 tape input and a slide switch; everything else — the two dies, the ESP-PSRAM64H memory, the ASE oscillators, the TDA1387T and LM358 audio chain, the AMS1117-3.3 rail — is the same design. There is **no USB host, no WiFi, no real-time clock and no power switch** on this board: it powers up as soon as USB-C is connected.

- **PCB size:** 55.00 × 55.00 mm, 4 layers
- **Compute:** RP2350B QFN-80 (master) + RP2350A QFN-60 (slave), both on-board
- **Component count:** 133 placements
- **KiCad project:** [`hardware/frank_core2_proto/`](../hardware/frank_core2_proto)
- **Gerbers:** [`hardware/frank_core2_proto/gerbers/`](../hardware/frank_core2_proto/gerbers/)
- **BOM, schematics PDF, assembly drawing:** [`hardware/frank_core2_proto/docs/`](../hardware/frank_core2_proto/docs/)

> **This is a prototype board.** It is published because it is the clearest, smallest
> illustration of the dual-RP2350 topology — not because it is the one to build for
> everyday use. For a usable machine, build [Core 2U](./frank_core2u.md) instead.

> **Recently promoted from [frank-lab](https://github.com/rh1tech/frank-lab).** Review the
> schematic and BOM before ordering a PCB.

## What's on the board

| Subsystem | Component(s) | Purpose |
|-----------|--------------|---------|
| Compute (master) | RP2350B QFN-80 | Main emulation core |
| Compute (slave) | RP2350A QFN-60 | Audio and housekeeping |
| Flash | W25Q128JVPIQ × 2 | 16 MB per die |
| PSRAM | ESP-PSRAM64H × 2 (SO-8) | 8 MB per die |
| Clock source | ASE-12 MHz oscillators × 2 | One per die |
| Video | DC3RX19JA2 HDMI Type A + 2 × R_Pack04 270 R | HDMI out with series termination |
| Audio out | TDA1387T + LM358 + PJ-320D 3.5 mm jack | Discrete I²S DAC and buffer |
| Storage | 104031-0811 MicroSD slot | ROMs, WADs, disk images |
| USB | USB4215-03-A Type-C × 2 | Native USB, one per die |
| USB protection | USBLC6-2SC6 × 2 | ESD |
| Power | AMS1117-3.3, SS34, 1N5819WS × 2 | 5 V in, 3.3 V LDO rail |
| Indicators | WS2812B-2020 + green and blue 0603 LEDs | Addressable status plus two discretes |
| Buttons | 4 × KMR221G side tactile | Boot and reset, one pair per die |
| Mounting | 4 × Ø2.5 mm plated | Plated and stitched |

## Bill of materials (high-level)

The full, exact BOM is generated from the KiCad project and published as
[`hardware/frank_core2_proto/docs/<rev>/bom.html`](../hardware/frank_core2_proto/docs/).
Of the 133 placements, 53 are capacitors and 38 are resistors; the headline parts are the
RP2350B and RP2350A, two W25Q128JVPIQ flash chips, two ESP-PSRAM64H chips, the TDA1387T
DAC with its LM358 buffer and the AMS1117-3.3 regulator.

## Assembly notes

- Both QFN packages need hot air or a hot plate. Solder the RP2350B, RP2350A, flash and PSRAM
  first, then the regulator, then the connectors.
- The AMS1117-3.3 is an LDO, not a buck. It will run warm — do not skip its ground pour.
- The four KMR221G tactiles are side-actuated and sit on the board edge, so they are easy to
  fit crooked. Tack one pad, check alignment, then solder the rest.
- RN1 and RN2 are 4 × 0603 resistor arrays on the HDMI lines. They are easy to mistake for a
  single large passive — check the silkscreen.
- There is no power switch. Anything you connect is live the moment USB-C is plugged in.

## First boot

1. Hold BOOT on the master (S1), connect its USB-C, release, and copy a `.uf2` to the drive
   that appears (or use `picotool load`).
2. Flash the slave the same way through its own USB-C, using S3 for BOOT.
3. Connect an HDMI display.
4. Insert a MicroSD card with ROMs or disk images.

There is no USB host port, so keyboard input has to come from whatever the firmware supports
over the remaining interfaces. This is the main reason the board is a prototype rather than a
daily driver.

## Firmware compatibility

Core 2 Proto has 16 MB flash and 8 MB PSRAM per die, so PSRAM-requiring firmware runs, and
the audio path is the same discrete TDA1387T I²S DAC that Core 2U uses. What firmware must
not expect is USB host, WiFi or a real-time clock — none of the three is fitted. Video is
HDMI only.

## Silkscreen

| Top | Bottom |
|:---:|:---:|
| <img src="./boards/frank_core2_proto-top.svg" alt="FRANK Core 2 Proto top silkscreen" width="420"> | <img src="./boards/frank_core2_proto-bottom.svg" alt="FRANK Core 2 Proto bottom silkscreen" width="420"> |

## Troubleshooting

- **No video:** confirm the firmware targets HDMI, and check RN1/RN2 — a missing or badly
  placed resistor array kills the link with no other symptom.
- **Board runs hot:** the AMS1117 is a linear regulator dropping 5 V to 3.3 V. Warm is normal;
  too hot to touch means check for a short on the 3.3 V rail.
- **No audio:** audio is the discrete TDA1387T + LM358 chain — check both and the PJ-320D
  jack wiring.
- **Only one die enumerates:** each die has its own USB-C and its own BOOT/RESET pair. Confirm
  you are using the right connector for the die you are flashing.
- **Expecting USB keyboards, WiFi or an RTC:** none is fitted. Use [Core 2U](./frank_core2u.md)
  or [FRANK Next](./frank_next.md).
