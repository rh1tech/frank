# FRANK

FRANK is a hardware emulation platform built around the Raspberry Pi RP2040/RP2350. It started as a fork of the [Murmulator](https://murmulator.ru/) project by [Alex Ekb](https://t.me/Alex_Eburg) and grew from there.

The name comes from "Frankenstein" — it's stitched together from parts of different projects.

## Supported Platforms

Over 30 emulated systems and native ports:

### Console Emulators

| Firmware | Platform | Repository | PSRAM Required |
|----------|----------|------------|----------------|
| frank-nes | NES / Famicom (Dendy) | [frank-nes](https://github.com/rh1tech/frank-nes) | Yes (8MB) |
| frank-snes | SNES / Super Famicom | [frank-snes](https://github.com/rh1tech/frank-snes) | Yes (8MB) |
| frank-genesis | Sega Genesis / Mega Drive | [frank-genesis](https://github.com/rh1tech/frank-genesis) | Yes (8MB) |
| frank-c64 | Commodore 64 | [frank-c64](https://github.com/rh1tech/frank-c64) | Yes (8MB) |
| frank-apple | Apple IIe | [frank-apple](https://github.com/rh1tech/frank-apple) | Optional |

### PC Emulation

| Firmware | Platform | Repository | PSRAM Required |
|----------|----------|------------|----------------|
| frank-386 | IBM PC i386 (DOS, Windows 3.x/95, Linux) | [frank-386](https://github.com/rh1tech/frank-386) | Yes (8MB) |

### Game Engine Ports

| Firmware | Game | Repository | PSRAM Required |
|----------|------|------------|----------------|
| frank-idtech1 | DOOM, Heretic, Hexen, Strife (all-in-one) | [frank-idtech1](https://github.com/rh1tech/frank-idtech1) | Yes (8MB) |
| frank-doom | DOOM (standalone) | [frank-doom](https://github.com/rh1tech/frank-doom) | Yes (8MB) |
| frank-heretic | Heretic | [frank-heretic](https://github.com/rh1tech/frank-heretic) | Yes (8MB) |
| frank-wolf3d | Wolfenstein 3D | [frank-wolf3d](https://github.com/rh1tech/frank-wolf3d) | Yes (8MB) |
| frank-duke3d | Duke Nukem 3D | [frank-duke3d](https://github.com/rh1tech/frank-duke3d) | Yes (8MB) |
| frank-prince | Prince of Persia | [frank-prince](https://github.com/rh1tech/frank-prince) | Yes (8MB) |
| frank-digger | Digger Remastered | [frank-digger](https://github.com/rh1tech/frank-digger) | No |

### OS and utilities

| Firmware | Description | Repository | PSRAM Required |
|----------|-------------|------------|----------------|
| frank-os | Full desktop OS (Windows 95-style GUI, apps, built-in emulators) | [frank-os](https://github.com/rh1tech/frank-os) | Yes (8 MB) |
| frank-kickstart | UF2 firmware launcher/bootloader with SD card browser | [frank-kickstart](https://github.com/rh1tech/frank-kickstart) | Optional |
| frank-manul | Text web browser (HTTP/HTTPS via ESP-01 WiFi) | [frank-manul](https://github.com/rh1tech/frank-manul) | Yes (8 MB) |
| frank-netcard | AT modem firmware for ESP-01 WiFi module | [frank-netcard](https://github.com/rh1tech/frank-netcard) | No |

### Murmulator firmware

The original [Murmulator](https://murmulator.ru/) project also provides:

**ZX Spectrum emulators:**
- ZX Spectrum 48K / 128K / +3e (multiple emulator cores: tecnocat, pico-spec, frut-bat, ZX Speccy P, murmulator)
- ZX Elf (pico-alf)

**Cross-platform emulators:**
- MS-DOS IBM PC XT 8088
- BK-0010 / BK-0011M (Soviet home computer)
- Radio 86RK (Soviet home computer)
- Macintosh
- Atari 800
- NES / Famicom / Dendy
- Sega Master System / Game Gear
- NEC PC Engine / TurboGrafx-16
- Nintendo GameBoy / GameBoy Color
- Watara Supervision
- Atari Lynx
- Bandai Wonderswan / Wonderswan Color
- NeoGeo Pocket Color
- Gamate
- Game & Watch / Elektronika

**Tools and applications:**
- MurmulatorOS
- Picomite MMBasic
- VersaTerm (terminal for retro computers)
- Bootloader (flash firmware from SD card)

## Hardware

### Product families

- **FRANK** — 0805 components, Pico module socket, RP2040-Zero for USB-to-PS/2. Easy to hand-solder.
- **MiniFRANK** — Credit-card-sized. 0603 components, QFN MCU soldered on-board, no Pico module.
- **TurboFRANK** — USB hub, RS-232, TurboSound (2×AY-3-8910), speaker amp. M1P1 uses a Pico socket; M1P2 has both RP2040 and RP2350A on-board with a USB multiplexer.

### Naming: MxPy

- **M1 / M2** — GPIO layout. M1 is the first-gen Murmulator pinout. M2 is second-gen, optimized for RP2350A HSTX and faster RAM.
- **P1 / P2** — RP chip. P1 = RP2040, P2 = RP2350A.

So TurboFRANK M1P1 = M1 layout + RP2040. MiniFRANK M2P2 = M2 layout + RP2350A.

When downloading firmware, pick the build matching your GPIO layout (M1 or M2).

### Boards

All KiCad project files are in `hardware/`.

#### FRANK (regular)

| Board | Layout | Compute Module | Latest Rev | Description | Docs |
|-------|--------|----------------|------------|-------------|------|
| [FRANK M2](./hardware/frank_m2) | M2 | Pico 2 (socket) | 2.06 | M2 pinout optimized for RP2350A HSTX, QSPI PSRAM via Pico module | [Guide](./docs/frank_m2.md) |
| [FRANK FastBeta M1](./hardware/frank_fastbeta_m1) | M1 | Pico / Pico 2 (socket) | 1.04 | Streamlined board for FastBeta ZX Spectrum firmware, HDMI/VGA, PIO PSRAM | [Guide](./docs/frank_fastbeta_m1.md) |

#### MiniFRANK (compact)

| Board | Layout | Compute Module | Latest Rev | Description | Docs |
|-------|--------|----------------|------------|-------------|------|
| [MiniFRANK M2P2](./hardware/minifrank_m2p2) | M2 | RP2350A (on-board QFN) | 1.04 | Credit-card-sized, 0603 components, ESP-01S WiFi, USB hub (MW7211A) | [Guide](./docs/minifrank_m2p2.md) |

#### TurboFRANK (feature-rich)

| Board | Layout | Compute Module | Latest Rev | Description | Docs |
|-------|--------|----------------|------------|-------------|------|
| [TurboFRANK M1P1](./hardware/turbofrank_m1p1) | M1 | Pico / Pico 2 (socket) | 1.07 | USB hub, RS-232, TurboSound (2xAY-3-8910), speaker amplifier | [Guide](./docs/turbofrank_m1p1.md) |
| [TurboFRANK M1P2](./hardware/turbofrank_m1p2) | M1 | RP2040 + RP2350A (on-board) | 1.01 | Dual-MCU with USB multiplexer and hardware power switch | [Guide](./docs/turbofrank_m1p2.md) |

### Accessories

In `accessories/`:

| Accessory | Latest Rev | Description | Docs |
|-----------|------------|-------------|------|
| [HDMI2VGA](./accessories/hdmi2vga) | 2.04 | HDMI to VGA / soft composite passive adapter | [Assembly Guide](./docs/hdmi2vga.md) |
| [USB2PS2](./accessories/usb2ps2) | 1.03 | USB to PS/2 adapter built on RP2040 | [Assembly Guide](./docs/usb2ps2.md) |

### Other compatible boards

Most firmware also runs on:

- Murmulator (M1 and M2)
- Olimex PICO-PC (some firmware)
- Waveshare RP2350-PiZero (some firmware)

### PSRAM

Most emulators need 8MB PSRAM. Connection method depends on layout:

- **M1 boards** — PIO. Solder an ESP-PSRAM64H onto the board's PSRAM pads.
- **M2 boards** — QSPI, piggybacked onto the Pico 2's flash chip. Works with clone Pico 2 boards (usually black, SOIC-8 flash). Or just use a Pimoroni Pico Plus 2 (has PSRAM built in).

## Getting started

1. Order or fab the PCB (gerbers are in each board's `gerbers/` directory)
2. Assemble per the [board-specific guide](./docs/)
3. Flash firmware: hold BOOTSEL, plug USB, release, copy `.uf2` to the drive (or `picotool load`)
4. SD card with ROMs / WADs / disk images
5. Display (HDMI or VGA) + keyboard (PS/2 or USB) + gamepad

For quick switching between firmware, flash [frank-kickstart](https://github.com/rh1tech/frank-kickstart) first. It gives you a graphical launcher that reflashes from SD without BOOTSEL.

## Repo layout

```
hardware/      Board KiCad projects
accessories/   Adapter boards (HDMI2VGA, USB2PS2)
docs/          Assembly guides
software/      Prebuilt firmware
archive/       Old board revisions
```

Experimental and work-in-progress boards are in a separate repo: [frank-lab](https://github.com/rh1tech/frank-lab).

## Links

- [rh1.tech/projects/frank](https://rh1.tech/projects/frank) — project website
- [github.com/rh1tech](https://github.com/rh1tech?q=frank) — firmware repos

## Author

Mikhail Matveev — software engineer and hardware developer based in Thessaloniki, Greece. Background in software architecture, QA, and open-source development. Builds retro computing hardware and writes firmware for fun. More projects at [rh1.tech](https://rh1.tech).

## License

&copy; 2026 Mikhail Matveev, <xtreme@rh1.tech>

GPL v3. See [LICENSE](./LICENSE).
