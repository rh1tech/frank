# XT8086 Beta assembly and usage guide

<p align="center">
  <img src="./boards/3d/xt8086_beta-iso.png" alt="XT8086 Beta 3D render" width="640">
</p>

XT8086 Beta puts a real Intel 8086 in a DIP-40 socket and builds an IBM PC XT class machine around it. The 8086 runs in minimum mode with SN74LS04 and SN74LS74 glue; an RP2350B acts as the chipset, providing memory, video, storage and I/O. It is a PC, not an emulation of one.

Video leaves the RP2350B as VGA-timed RGB and is converted to HDMI on-board by an MS9288A, so the board drives a modern display directly. Sound is a transistor-driven PC speaker with a 12 mm buzzer, plus a 3.5 mm line out and a jumper to route audio over HDMI instead. A DS3231MZ with a CR1220 cell keeps the clock, which a PC of this vintage rather needs.

This is a **separate version stream** from [XT8086 Alpha](../hardware/xt8086_alpha), not a later revision of it. The alpha is a PGA2350-based board with VGA and composite outputs; the beta replaces the module with a bare RP2350B, drops VGA and composite for HDMI, and adds a USB hub, stacked host port and the RTC.

- **PCB size:** 99.50 × 80.00 mm
- **Compute:** Intel 8086 (minimum mode, DIP-40 socket) + RP2350B QFN-80 as chipset
- **KiCad project:** [`hardware/xt8086_beta/`](../hardware/xt8086_beta)
- **Gerbers:** [`hardware/xt8086_beta/gerbers/`](../hardware/xt8086_beta/gerbers/)
- **BOM, schematics PDF, assembly drawing:** [`hardware/xt8086_beta/docs/`](../hardware/xt8086_beta/docs/)

> **Recently promoted from [frank-lab](https://github.com/rh1tech/frank-lab).** Review the
> schematic and BOM before ordering a PCB.

## What's on the board

| Subsystem | Component(s) | Purpose |
|-----------|--------------|---------|
| CPU | Intel 8086, DIP-40 socket | Minimum mode, socketed |
| Glue logic | SN74LS04, SN74LS74 (SOIC-14) | Clock and control conditioning |
| Chipset | RP2350B QFN-80 | Memory, video, storage and I/O |
| Flash | W25Q128JVPIQ | 16 MB SPI flash |
| PSRAM | ESP-PSRAM64H | 8 MB PSRAM |
| Clock source | ASE-12 MHz oscillator | RP2350B clock |
| Video | MS9288A + HDMI Type A | VGA-timed RGB converted to HDMI on-board |
| Video clock | 24.576 MHz + AMS1117-1.8 | MS9288A clock and 1.8 V rail |
| Sound | BC847 + 12 × 8.5 mm buzzer | PC speaker |
| Audio out | 3.5 mm jack, HDMI-audio jumper | Line level or over HDMI |
| Clock | DS3231MZ + CR1220 | Battery-backed real-time clock |
| USB host | Stacked USB Type-A (2 ports) | Behind MW7211A hub |
| USB routing | TS3USB221 | Host multiplexing |
| USB protection | CH217K × 2, USBLC6 × 3, 1.5 A fuse | Current limit and ESD |
| USB (native) | USB Type-C | RP2350B native USB |
| Storage | MicroSD slot | Disk images and ROMs |
| Power in | DC-005 barrel jack, USB-C, 3-pin header | Three sources |
| Power | MP1584EN buck, SY8089AAAC, AMS1117-1.8 | 5 V, 3.3 V and 1.8 V rails |
| Power path | TPS2116, AO3401A, Force On jumper | Source selection and ideal diode |
| Switches | SS12D00, SK12D07VG3NS, 3-pos DIP | Power, mode, configuration |
| Mounting | 4 × Ø2.7 mm | Plain, unplated |

## Bill of materials (high-level)

The full, exact BOM is generated from the KiCad project and published as
[`hardware/xt8086_beta/docs/<rev>/bom.html`](../hardware/xt8086_beta/docs/). The headline
parts are the Intel 8086 in its DIP-40 socket, the SN74LS04 and SN74LS74, the RP2350B, the
W25Q128 flash, the ESP-PSRAM64H, the MS9288A video converter, the DS3231MZ RTC and the
MP1584EN buck.

## Assembly notes

- Solder the RP2350B, flash, PSRAM and MS9288A first — they are the fine-pitch parts and need
  hot air or a hot plate.
- Fit the **DIP-40 socket**, not the 8086 itself. The CPU drops in once the rails are verified.
- Check the 1.8 V rail before fitting the MS9288A; the converter will not survive 3.3 V.
- The LS04 and LS74 are SOIC-14 here, not the through-hole DIPs used on the alpha.
- The barrel jack, stacked USB and buzzer stand proud — fit them last.

## First boot

1. Check the 5 V, 3.3 V and 1.8 V rails with the 8086 socket **empty**.
2. Hold BOOT, connect USB-C, release, and copy a `.uf2` to the drive that appears.
3. Power down, seat the 8086 observing pin 1, and power back up.
4. Connect an HDMI display.
5. Plug a USB keyboard into the stacked host port and insert a MicroSD card.
6. Fit the CR1220 cell so the clock survives power-off.

## Firmware compatibility

XT8086 Beta runs its own chipset firmware on the RP2350B — it is not a target for the FRANK
emulation cores, which assume the RP2350 is the CPU rather than the support chip. Video is
HDMI only (through the MS9288A), input is USB only, and there is no WiFi or tape input on
this board.

## Silkscreen

| Top | Bottom |
|:---:|:---:|
| <img src="./boards/xt8086_beta-top.svg" alt="XT8086 Beta top silkscreen" width="420"> | <img src="./boards/xt8086_beta-bottom.svg" alt="XT8086 Beta bottom silkscreen" width="420"> |

## Troubleshooting

- **Nothing on screen:** the MS9288A needs its 1.8 V rail and its 24.576 MHz clock. Check both
  before suspecting the RP2350B.
- **8086 does nothing:** confirm pin 1 orientation in the socket and that the LS04 and LS74 are
  fitted the right way round.
- **No audio:** check the HDMI-audio jumper. With it fitted, audio leaves over HDMI rather than
  the 3.5 mm jack.
- **Clock resets:** fit the CR1220 cell; the DS3231MZ keeps time only while it is backed up.
- **Board will not power from the barrel jack:** the TPS2116 selects between sources; verify
  centre-positive polarity and an intact fuse.
