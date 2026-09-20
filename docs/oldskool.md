# Old Skool FRANK assembly and usage guide

<p align="center">
  <img src="./boards/3d/oldskool-iso.png" alt="Old Skool FRANK 3D render" width="640">
</p>

Old Skool FRANK is the board for people who want the ports their machines actually had. It is the only board in the lineup carrying **HDMI and a real VGA DE-15 at the same time**, alongside a hardware PS/2 mini-DIN and a top-mounted DB9 gamepad port. Compute is a socketed Pimoroni PGA2350 module, so the RP2350B can be pulled and replaced.

It is also the only **2-layer** board here. That keeps fabrication cheap and makes it far friendlier to hand-assembly and rework than the 4-layer boards — there are no buried planes to fight. Audio is deliberately analogue: a TDA1545A DAC buffered by an LM358 into a green 3.5 mm jack, with a red jack for tape input through a CD4069UBE. Through-hole 6 mm tactile buttons rather than SMD.

- **PCB size:** 100.00 × 79.00 mm
- **Compute:** Pimoroni PGA2350 module (RP2350B)
- **KiCad project:** [`hardware/oldskool/`](../hardware/oldskool)
- **Gerbers:** [`hardware/oldskool/gerbers/`](../hardware/oldskool/gerbers/)
- **BOM, schematics PDF, assembly drawing:** [`hardware/oldskool/docs/`](../hardware/oldskool/docs/)

> **Recently promoted from [frank-lab](https://github.com/rh1tech/frank-lab).** Review the
> schematic and BOM before ordering a PCB.

## What's on the board

| Subsystem | Component(s) | Purpose |
|-----------|--------------|---------|
| Compute | Pimoroni PGA2350 module | Socketed RP2350B with flash and PSRAM |
| Video | HDMI Type A **and** VGA DE-15 | Both outputs fitted |
| Keyboard / mouse | PS/2 mini-DIN-6 | Hardware PS/2 |
| Gamepad | DB9 male, top-mount | Atari / Sega style pad |
| USB host | Stacked USB Type-A (2 ports) | Behind MW7211A hub |
| USB routing | 74HC4052 | Host multiplexing |
| USB (upstream) | USB Type-C **and** USB Type-B | Two upstream options |
| Audio out | TDA1545A + LM358 + 3.5 mm jack (green) | Analogue DAC and buffer |
| Tape in | 3.5 mm jack (red) + CD4069UBE | Cassette / audio input |
| WiFi | ESP-01S module socket | Socketed WiFi |
| Storage | MicroSD slot | ROMs, WADs, disk images |
| Power | USB-C / USB-B, AMS1117-3.3 | 5 V in, single 3.3 V LDO rail |
| Switches | SK12D07VG3NS × 2 | Power and mode |
| Buttons | 6 mm through-hole tactile × 3 | Boot, reset, user |
| Mounting | 4 × Ø2.7 mm | Plain, unplated |

## Bill of materials (high-level)

The full, exact BOM is generated from the KiCad project and published as
[`hardware/oldskool/docs/<rev>/bom.html`](../hardware/oldskool/docs/). The headline parts are
the Pimoroni PGA2350 module, the MW7211A hub with its 74HC4052 multiplexer, the TDA1545A DAC
with its LM358 buffer, the CD4069UBE tape stage and the AMS1117-3.3 regulator.

## Assembly notes

- This is a **2-layer** board with a socketed compute module, so it is the most hand-solderable
  design in the repo. There is no QFN to hot-air.
- Fit the low-profile parts first, then the tall connectors: VGA, DB9, PS/2, stacked USB and
  the barrel of the audio jacks all stand proud and will get in the way.
- The DB9 is a **top-mount male** part — check the orientation against the silkscreen before
  soldering; it is not reversible.
- The PGA2350 socket determines the board's height. Seat the module only after everything
  else is soldered and the rails have been checked.

## First boot

1. Seat the PGA2350 module, observing pin 1.
2. Hold BOOT, connect USB-C (or USB-B), release, and copy a `.uf2` to the drive that appears.
3. Connect either an HDMI display or a VGA monitor.
4. Plug in a PS/2 keyboard, a USB keyboard, or both.
5. Insert a MicroSD card with ROMs or disk images.

## Firmware compatibility

Old Skool FRANK runs firmware that targets the FRANK I/O set with VGA, PS/2 and DB9 support —
the same combination the full-size boards use. PSRAM comes from the PGA2350 module, so
PSRAM-requiring firmware works as long as a PSRAM-equipped module is fitted. There is no
real-time clock on this board.

## Silkscreen

| Top | Bottom |
|:---:|:---:|
| <img src="./boards/oldskool-top.svg" alt="Old Skool FRANK top silkscreen" width="420"> | <img src="./boards/oldskool-bottom.svg" alt="Old Skool FRANK bottom silkscreen" width="420"> |

## Troubleshooting

- **No video on VGA:** confirm the firmware build targets VGA rather than HDMI; both outputs
  are fitted but firmware usually drives one.
- **Gamepad reads nothing:** the DB9 is top-mount male — a reversed part will look soldered
  and still be wrong.
- **No PS/2:** check the mini-DIN shield ground and that the firmware enables hardware PS/2.
- **Board runs hot:** the AMS1117 is a linear regulator. Warm is expected; hot means check the
  3.3 V rail for a short.
- **Module not detected:** reseat the PGA2350 and confirm pin 1 orientation.
