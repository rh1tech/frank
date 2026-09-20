# Dino Z80 assembly and usage guide

<p align="center">
  <img src="./boards/3d/dino_z80-iso.png" alt="Dino Z80 3D render" width="640">
</p>

Dino Z80 is not an emulator. There is a real Zilog Z84C0020PEC in a DIP-40 socket, running actual Z80 machine code. The RP2350B beside it is not pretending to be a CPU — it is the chipset: memory, video, storage and I/O for a processor that shipped in 1976.

Video leaves the RP2350B as VGA-timed RGB and is converted to HDMI on-board by an MS9288A, so a modern display works without an adapter. Sound is the period-correct pairing of a PC speaker driven through a transistor and a 12 mm buzzer, plus a 3.5 mm line out and a tape input for loading from cassette. Power comes in through a 2.0 mm barrel jack, USB-C, or a 3-pin header, with an MP1584EN buck and a TPS2116 picking between them.

Dino shares its outline, mounting pattern and power/video architecture with [XT8086 Beta](./xt8086_beta.md) — the two boards are the same platform with a different CPU socketed into it.

- **PCB size:** 99.50 × 80.00 mm
- **Compute:** Zilog Z84C0020PEC (Z80, DIP-40 socket) + RP2350B QFN-80 as chipset
- **KiCad project:** [`hardware/dino_z80/`](../hardware/dino_z80)
- **Gerbers:** [`hardware/dino_z80/gerbers/`](../hardware/dino_z80/gerbers/)
- **BOM, schematics PDF, assembly drawing:** [`hardware/dino_z80/docs/`](../hardware/dino_z80/docs/)

> **Recently promoted from [frank-lab](https://github.com/rh1tech/frank-lab).** Review the
> schematic and BOM before ordering a PCB.

## What's on the board

| Subsystem | Component(s) | Purpose |
|-----------|--------------|---------|
| CPU | Zilog Z84C0020PEC, DIP-40 socket | Real Z80, 20 MHz rated, socketed |
| Bus buffer | 74HCT125 | Level-safe buffering onto the Z80 bus |
| Chipset | RP2350B QFN-80 | Memory, video, storage and I/O |
| Flash | W25Q128JVPIQ | 16 MB SPI flash |
| PSRAM | ESP-PSRAM64H | 8 MB PSRAM |
| Video | MS9288A + HDMI Type A | VGA-timed RGB converted to HDMI on-board |
| Video clock | 24.576 MHz + AMS1117-1.8 | MS9288A clock and 1.8 V rail |
| Sound | BC847 + 12 × 8.5 mm buzzer | PC speaker |
| Audio out | 3.5 mm jack | Line level |
| Tape in | 3.5 mm jack + BC850 pair | Cassette / audio input |
| USB host | Stacked USB Type-A (2 ports) | Behind MW7211A hub |
| USB routing | TS3USB221 | Host multiplexing |
| USB protection | CH217K × 2, USBLC6 × 3, 1.5 A fuse | Current limit and ESD |
| USB (native) | USB Type-C | RP2350B native USB |
| Storage | MicroSD slot | Disk images and ROMs |
| Power in | DC-005 barrel jack, USB-C, 3-pin header | Three sources |
| Power | MP1584EN buck, SY8089AAAC, AMS1117-1.8 | 5 V, 3.3 V and 1.8 V rails |
| Power path | TPS2116 | Automatic source selection |
| Switches | SS12D00, SK12D07VG3NS, 3-pos DIP | Power, mode, configuration |
| Mounting | 4 × Ø2.7 mm | Plain, unplated |

## Bill of materials (high-level)

The full, exact BOM is generated from the KiCad project and published as
[`hardware/dino_z80/docs/<rev>/bom.html`](../hardware/dino_z80/docs/). The headline parts are
the Z84C0020PEC in its DIP-40 socket, the RP2350B, the W25Q128 flash, the ESP-PSRAM64H, the
MS9288A video converter, the MW7211A hub and the MP1584EN buck.

## Assembly notes

- Solder the RP2350B, flash, PSRAM and MS9288A first — they are the fine-pitch parts and need
  hot air or a hot plate.
- Fit the **DIP-40 socket**, not the Z80 itself. The CPU drops in afterwards and should be
  seated only once the rails have been checked.
- Check the 1.8 V rail before fitting the MS9288A. It is the one rail that is easy to get
  wrong and the converter will not survive 3.3 V.
- The barrel jack, stacked USB and buzzer stand proud — fit them last.

## First boot

1. Check the 5 V, 3.3 V and 1.8 V rails with the Z80 socket **empty**.
2. Hold BOOT, connect USB-C, release, and copy a `.uf2` to the drive that appears.
3. Power down, seat the Z80 observing pin 1, and power back up.
4. Connect an HDMI display.
5. Plug a USB keyboard into the stacked host port and insert a MicroSD card.

## Firmware compatibility

Dino Z80 runs its own chipset firmware on the RP2350B — it is not a target for the FRANK
emulation cores, which assume the RP2350 is the CPU rather than the support chip. Video is
HDMI only (through the MS9288A), input is USB only, and there is no WiFi or real-time clock
fitted.

## Silkscreen

| Top | Bottom |
|:---:|:---:|
| <img src="./boards/dino_z80-top.svg" alt="Dino Z80 top silkscreen" width="420"> | <img src="./boards/dino_z80-bottom.svg" alt="Dino Z80 bottom silkscreen" width="420"> |

## Troubleshooting

- **Nothing on screen:** the MS9288A needs its 1.8 V rail and its 24.576 MHz clock. Check both
  before suspecting the RP2350B.
- **Z80 does nothing:** confirm pin 1 orientation in the socket, and that the 74HCT125 buffer
  is fitted and oriented correctly.
- **No USB input:** the stacked port sits behind the MW7211A hub — check the hub, the CH217K
  limiters and the 1.5 A fuse.
- **Board will not power from the barrel jack:** the TPS2116 selects between sources; verify
  the jack polarity is centre-positive and the fuse is intact.
