# FRANK Core 2 Proto — dual-RP2350 test firmware

Bring-up and link-characterisation firmware for the FRANK Core 2 Proto board:
an RP2350B master (U3, QFN-80) and an RP2350 slave (U6, QFN-60) joined by
two 8-bit source-synchronous parallel buses running at a measured
96.1 MiB/s aggregate, error-free.

The master brings up HDMI, prints a full diagnostic report on screen,
runs its own flash and PSRAM tests, then interrogates the slave over the
link and measures how fast the two halves can actually talk.

```
firmware/
├── boards/          SDK board headers (package + default pin selection)
├── common/          shared by both MCUs: board map, link, protocol, memory tests
├── drivers/         reused verbatim from frank-msx (HDMI, SD, audio, PSRAM, USB HID)
├── master/          RP2350B firmware + build/flash scripts
├── slave/           RP2350A firmware + build/flash scripts
├── probe/           minimal firmware for bisecting a silent bring-up
├── build_all.sh     build both halves with matching options
├── swd_flash.sh     flash over SWD with a Debug Probe (preferred)
└── flash_all.sh     guided two-step USB BOOTSEL flash
```

## Quick start

```bash
cd frank_core2_proto/firmware
./build_all.sh          # both halves, 252 MHz, USB CDC console

./swd_flash.sh master   # with a Debug Probe on J1 (preferred)
./swd_flash.sh slave    #                  ... or J3

./flash_all.sh          # or USB BOOTSEL: prompts through slave then master
```

**Prefer SWD if you have a Debug Probe.** USB-BOOTSEL flashing needs a
button press whenever the firmware wedges — exactly when you are
iterating fastest — and `picotool reboot -u` will not recover a target
that has faulted into lockup. SWD does not care what the target is
doing, and it gives you `pc` when something stops.

Reset order does not matter. The master reports `LINK DOWN` if the slave
is not serving when the sweep runs, then keeps probing and re-runs the
diagnostic on its own as soon as the slave answers.

If the slave is wedged, the master pulses FS to ask it to reboot. If the
slave is in lockup, use SWD on J3 — see the hardware note below.

## Build options

Both scripts read the same environment variables, and `build_all.sh`
passes them to both halves so the two firmwares stay matched:

| Variable | Default | Meaning |
|---|---|---|
| `USB_HID` | `0` | `0` = USB CDC serial console. `1` = USB HID host keyboard on the master, console moves to UART |
| `CPU_SPEED` | `252` | System clock in MHz. Sets the link's ceiling: one byte per 5 clocks |
| `PSRAM_SPEED` | `133` | PSRAM clock ceiling in MHz |
| `FLASH_SPEED` | `66` | Flash clock ceiling in MHz |
| `CLEAN` | `0` | `1` wipes the build directory first |

```bash
USB_HID=1 ./build_all.sh              # HID keyboard, UART console
CPU_SPEED=300 CLEAN=1 ./build_all.sh  # push the link to ~57 MiB/s per direction
```

**Build both halves at the same `CPU_SPEED`.** The receiving PIO state
machine has to complete its three-instruction loop inside the
transmitter's byte period, and each side derives that from its own
system clock. Mismatched clocks give you a link that works in one
direction and drops bytes in the other. `build_all.sh` exists to make
the matched build the path of least resistance.

## Consoles

| `USB_HID` | Master console | Slave console |
|---|---|---|
| `0` (default) | USB CDC on J8 | USB CDC on J9 |
| `1` | UART0 on J2 (GPIO0 TX / GPIO1 RX, 115200) | UART1 on J4 (GPIO24 TX / GPIO25 RX, 115200) |

The master mirrors everything it draws on screen to its console, so a
serial capture is a complete record of the run. Pressing any key on
either the USB HID keyboard or the serial console re-runs the whole
diagnostic.

## The link

Two independent buses, each 8 data lines plus a clock and a VALID
strobe, plus three single-wire control signals. Every pin below comes
from the KiCad netlist.

| Signal | Master | Slave | Notes |
|---|---|---|---|
| Bus A data D0..D7 | GPIO20..27 | GPIO1..8 | master → slave |
| Bus A clock | GPIO28 | GPIO9 | 33R series (R1) |
| Bus A valid | GPIO29 | GPIO10 | |
| Bus B data D0..D7 | GPIO30..37 | GPIO11..18 | slave → master |
| Bus B clock | GPIO38 | GPIO19 | 33R series (R2) |
| Bus B valid | GPIO39 | GPIO20 | |
| FS (frame sync) | GPIO40 out | GPIO21 in | reserved for future phase signalling |
| DB_MS (doorbell) | GPIO41 out | GPIO22 in | "master ready" |
| DB_SM (doorbell) | GPIO42 in | GPIO23 out | "slave ready / done" |

Both buses share one relative layout — clock at `data_base + 8`, valid at
`data_base + 9` — which is what lets a single pair of PIO programs serve
either direction on either chip.

### Wire protocol

Transmit is two PIO instructions, five system clocks per byte:

```
cycle:   0     1     2     3     4     0     1
DATA:  <--------- byte N -------------><--- byte N+1 ...
CLK:   ____________/‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\________/‾‾
                      ^ receiver samples here (t=3)
```

Two low, three high. Both numbers are forced by the receiver, and the
reasoning is worth keeping because the obvious 4-cycle version looks
correct and is not:

- The sample always lands at **rising edge + 1**. `wait 1 pin` completes
  on the edge; `in pins` runs the cycle after.
- The receive loop inspects the low phase during **one cycle** of each
  iteration, so a low phase shorter than 2 cycles is intermittently
  missed — which drops a byte and desynchronises the 32-bit word
  framing.

Together those rule out a 4-cycle period. 2 low + 2 high samples at t=3
with the next data change at t=4: three cycles of setup but **one cycle
of hold**, 3.97 ns at 252 MHz. Shrinking the low phase to 1 cycle to
centre the sample fixes hold and breaks edge detection instead.

Five cycles gives 3 setup and 2 hold. The worst case doubles, the low
phase keeps the two cycles the receiver needs, and the cost is top speed:
sys_clk/5 rather than sys_clk/4.

### Measured, not calculated

On the first assembled board at 252 MHz, both halves matched:

| Divider | Per direction | Duplex aggregate | Byte errors |
|---|---|---|---|
| 1.00x | 48.0 MiB/s | **96.1 MiB/s** | 0 |
| 1.25x | 38.4 MiB/s | 76.9 MiB/s | 0 |
| 1.50x | 32.0 MiB/s | 64.0 MiB/s | 0 |
| 2.00x | 24.0 MiB/s | 48.0 MiB/s | 0 |

Control round-trip: 139.7 us.

Getting there took one controlled experiment worth recording. With the
master on 5-cycle timing and the slave still on 4-cycle, the same wire
carried both:

| Direction | TX timing | 48 MiB/s | 60 MiB/s |
|---|---|---|---|
| M→S | 5-cycle | 0 errors | n/a |
| S→M | 4-cycle | 20819 bad bytes | 99551 bad bytes |

At the *same* 48 MiB/s one timing is clean and the other is not, which
rules out signal integrity and points squarely at the hold margin.
Fractional dividers make it worse again: only integer dividers are
jitter-free, and 1.25x makes the period alternate between N and N+1
cycles, eating into margins that are already only two cycles wide.

Going faster than sys_clk/5 needs a different receiver — two state
machines ping-ponging on alternate edges, say — not a faster clock.

### Handshake

`DB_MS` means "master is ready for the next step", `DB_SM` means "slave
is ready or done". Every phase raises both, transfers, then drops both.
Nothing depends on the two chips agreeing about absolute time, so the
slave can boot seconds after the master and still join cleanly.

## What the master reports

```
FRANK Core 2 Proto  -  dual RP2350 bring-up      v1.0
-----------------------------------------------------
              MASTER 2350B      SLAVE 2350A
Chip ID       E661...           E661...
Package/rev   QFN-80 B / rev 2  QFN-60 A / rev 2
Sys clock     252 MHz           252 MHz
Flash ID      EF4018 16 MB      EF4018 16 MB
Flash read    31.8 MiB/s        33.5 MiB/s
Flash CRC32   1A2B3C4D          9F8E7D6C
PSRAM         8 MB ok           8 MB ok
PSRAM write   12.9 MiB/s        13.4 MiB/s
PSRAM read    30.1 MiB/s        32.9 MiB/s
PSRAM errors  0                 0
-----------------------------------------------------
 SD:29.7 GB   USB:cdc   I2S:ok   HDMI:640x480@60
-----------------------------------------------------
 rate     M->S        S->M        duplex      err
 1.00x    48.0 MiB/s  48.0 MiB/s  96.1 MiB/s  0
 1.25x    38.4 MiB/s  38.4 MiB/s  76.9 MiB/s  0
 1.50x    32.0 MiB/s  32.0 MiB/s  64.0 MiB/s  0
 2.00x    24.0 MiB/s  24.0 MiB/s  48.0 MiB/s  0
 round-trip 139.71 us   peak duplex 96.1 MiB/s
 LINK OK - error free to 96.1 MiB/s aggregate
```

### Throughput and integrity are measured separately

- **Throughput** is one uninterrupted ring DMA — no per-block handshake,
  no CPU verification. The number is the wire and nothing else.
- **Integrity** is block-at-a-time with a handshake between each, every
  byte compared against the expected LFSR pattern.

Mixing them would either understate the speed (verification in the hot
path) or overstate the confidence (unverified bytes). The `err` column
comes from the integrity pass; a row is only marked OK when it is zero.
A structural failure shows as `no-run` rather than a small error count —
conflating "the exchange never completed" with "two bytes were wrong"
sends you hunting for signal integrity problems that are not there.

Each side generates the same pattern from the same seed, so neither has
to send reference data across the link it is trying to test.

## Peripheral tests

Run on **both** MCUs — the slave ships its results back over the link
and the master renders them beside its own.

- **Flash** — JEDEC ID and capacity, sequential XIP read throughput, and
  a CRC-32 over the first 64 KiB computed twice. A CRC mismatch between
  the two passes means marginal QSPI timing, which would make every
  other figure on the screen suspect.
- **PSRAM** — presence and size by address-aliasing probe, then a full
  write-and-verify sweep of all 8 MB with timing for both passes.

The probes read through the *uncached* XIP alias (`0x15000000`); through
the cached window a write followed by a read of the same address is
answered from the 8 KiB XIP cache, so a missing chip would look present
and an aliasing address would look like it isn't. Throughput uses the
cached window, because that is how real code reaches these devices.

## Audio

Three tones per run: **440 Hz left, 880 Hz right, 660 Hz both**, ~0.2 s
each, at about 27% of full scale.

Separate channels at distinct pitches are deliberate. A single mono tone
cannot tell a working stereo path from a stuck LRCK or a dead channel —
all three sound the same, which is to say they all sound like success.

`I2S:ok` on screen means only that the state machine consumed its
samples, i.e. SCLK and LRCK are clocking. It says nothing about the
TDA1387 converting, the analogue path through U7 to J6, or whether U8 is
even fitted — a board with no DAC at all still reports `I2S:ok`. There
is no loopback, so past GPIO 9/10/11 your ears (or a scope on J6) are
the only instrument.

That label was briefly worse than useless: `audio_i2s_program_init()`
configures the state machine but does not call `pio_gpio_init()` — the
frank-msx driver did that separately inside `i2s_init()`. Replacing that
driver with direct PIO calls dropped it, so the pads never left SIO
mode. The FIFO drained, the probe passed, and nothing reached the DAC.

## Heartbeats

Both LEDs are driven from a timer IRQ, not the main loop, so they keep
beating while the foreground is parked in a link handshake or a
multi-second PSRAM sweep. If an LED stops, that core is wedged.

**Master** — WS2812B (LD1, GPIO46), colour carries the state:

| Colour | Meaning |
|---|---|
| Blue breathe | Booting, peripherals not probed yet |
| Amber breathe | Self-test or link test running |
| Violet breathe | Idle, waiting for the peer |
| Green breathe | Everything passed |
| Red blink | Something failed — check the console |

**Slave** — blue LED (LD2, GPIO26), rate carries the state: 1 s slow beat
when waiting, 200 ms when serving, 80 ms frantic blink on self-test
failure.

## Hardware notes

### Hardware bug: GPIO43 does not reach the slave's RUN pin

The schematic draws master GPIO43 through R3 and on to the slave's `RUN`
sheet pin, and the wire and junction geometry are correct. The net still
does not exist.

**Root cause:** `rp2350a_slave.kicad_sch` declares hierarchical labels
for `GPIO1`..`GPIO29` but not for `RUN` — inside the slave sheet, `RUN`
is only a plain local label. A parent sheet pin binds to a *hierarchical*
label in the child, so the connection stops dead at the sheet boundary.
The netlist and the PCB agree:

```
/RP2350A/RUN                      pads: S4.1, S4.3, U6.26   (button only)
/RP2350{slash}43B{slash}RUNA{slash}SR   pads: U3.54, R3.1   (pull-up only)
```

So on this revision **the master cannot reset the slave in hardware.**
GPIO43 is an input with a 10K pull-up and the firmware never drives it.

*Fix for the next revision: add a hierarchical label `RUN` in the slave
sheet and re-route. One label.*

#### Bodge for this revision (optional)

Both ends of the missing net are accessible pads, so one wire restores
it — no QFN pin work:

```
R3 pin 1  ---->  S4 pin 1
(GPIO43 net)     (slave RUN net)
```

**R3 pin 1, not pin 2.** Pin 2 is +3V3; wiring that to RUN shorts 3V3 to
ground through S4 every time the reset button is pressed.

Then build the master with the wire declared:

```bash
cmake -S master -B master/build -DSLAVE_RESET_BODGE=ON
```

The pin is driven **open-drain and never driven high**: asserting reset
makes it a low output, releasing returns it to an input and lets R3's
10K pull-up do the work. Driving it high would fight S4 — which shorts
RUN to ground — and put the full pin drive current through the button.
Idle is therefore also the safe state, including at power-on before any
firmware runs.

Both build configurations are safe against operator error: the flag off
with the wire fitted just leaves the pin an input, and the flag on
without the wire just sends the pulse nowhere.

#### How the firmware works around it

Three mechanisms replace what the missing trace would have provided.
None of them needs the trace, so they stay useful afterwards.

**Startup ordering — never depended on it.** The doorbell handshake is
built so neither side needs the other to be at a known point in time:
`DB_MS` means "master ready", `DB_SM` means "slave ready", every phase
raises both and drops both. The slave can boot seconds after the master
and still join cleanly.

**The master keeps looking.** While the link is down, the idle loop
probes for the slave every five seconds with a 200 ms doorbell timeout,
and re-runs the whole diagnostic unprompted the moment it answers. A
slave that boots late, gets reflashed, or reboots on its own is picked
up without a keypress.

**The slave can reset itself, and the master can ask it to.** FS
(master GPIO40 → slave GPIO21) is wired, tested and otherwise unused, so
it doubles as a reset request: the master holds it high for 250 ms and
the slave reboots via its watchdog. The slave samples FS from a *timer
interrupt*, not the main loop, so this still works when the slave's
foreground is stuck — which is exactly when it is worth having. The
slave also arms an 8 s watchdog it kicks in the serve loop, so it
recovers from a hang whether or not the master notices.

#### Recovery escalates

The reconnect probe escalates rather than reaching for the biggest
hammer first, because the mechanisms recover different failures and none
subsumes the others:

| Failed probes | Action | Recovers |
|---|---|---|
| 1 | none — just probe again | a slave still booting, or absent |
| 2, 6, 10… | FS reset request | a slave whose foreground is stuck |
| 4, 8, 12… | reset pulse (bodge builds only) | a slave in lockup, interrupts off |

The slave's watchdog runs underneath all of it. The software path is
kept even in bodge builds: FS is the cleaner reboot when the slave is
still executing, and it is the only one that works on an unmodified
board.

### RP2350B GPIO window

The master is the B package and drives link pins up to GPIO39 plus the
WS2812 on GPIO46. Two things follow, and both are easy to get wrong:

1. The stock `pico2` board definition declares `PICO_RP2350A 1`, which
   caps `NUM_BANK0_GPIOS` at 30. `boards/frank_core2_proto_master.h` sets it
   to 0.
2. An RP2350B PIO instance can only address 32 consecutive GPIOs, either
   0–31 or 16–47, selected by `pio_set_gpio_base()`. The link spans
   GPIO20 to GPIO38, so it needs the upper window. Without it the SDK
   either rejects the configuration or — with parameter assertions off —
   quietly aliases GPIO32–38 onto GPIO0–6, giving a link that looks
   wired but reads garbage.

`link_init()` selects the window and then hard-asserts on the return
value of `pio_sm_init()`, so a misconfiguration fails loudly at boot
instead of producing plausible-looking wrong numbers.

### Resource allocation (master)

| Resource | Owner |
|---|---|
| PIO0 | Link TX + RX state machines |
| PIO1 (one SM) | I2S audio (TDA1387) |
| PIO2 | WS2812B heartbeat |
| DMA 0, 1 | HSTX video scanout |
| DMA_IRQ_0 | HSTX video scanout (exclusive handler) |
| DMA (claimed) | Link TX + RX |
| Core 1 | HSTX scanout, launched by `graphics_init()` |

### Why the audio test does not use the frank-msx I2S driver

`audio.c`'s `i2s_init()` installs an **exclusive** `DMA_IRQ_0` handler
for its double-buffered playback path. The HSTX video driver
(`pico_hdmi/video_output.c`) has already taken `DMA_IRQ_0`. The second
`irq_set_exclusive_handler()` hard-asserts, `panic()` executes a
breakpoint with no debugger attached, and the core escalates straight
into **lockup** — report half-drawn, USB dead, no message, and no
software route back into BOOTSEL.

That combination is specific to this firmware: frank-msx pairs the audio
driver with the *PIO* HDMI path, which uses `DMA_IRQ_1`, so the two
never collide there.

A tone test needs neither DMA nor an interrupt, so `main.c` drives the
I2S state machine directly from `audio_i2s.pio` and pushes samples into
the FIFO against a deadline. If you later want streaming audio here,
move the driver to `DMA_IRQ_1` rather than reintroducing the collision.

### Boot ordering

`mem_test_flash_identify()` takes the QMI out of XIP to issue a raw
`0x9F`. It runs before `graphics_init()` launches core 1, because core 1
fetching from flash during that window would hang. The PSRAM probe runs
in the same quiet period. Everything after that is plain loads and
stores through the XIP windows and is safe with both cores running.

## Debugging

Every stage prints to the console before it runs (`[boot] ...`), so a
stall is located by reading the last line rather than by bisecting the
binary. Both firmwares also repeat a banner for two seconds at startup:
USB CDC enumeration plus getting a terminal open reliably takes longer
than the diagnostic takes to run, and a report you missed is a report
you do not have.

Recovery, in increasing order of severity:

| Symptom | Action |
|---|---|
| Slave not answering | Nothing — the master retries every 5 s and pulses FS |
| Slave firmware hung | Its own 8 s watchdog reboots it |
| Slave core in lockup | `./swd_flash.sh slave --reset-only` |
| Master wedged | `./swd_flash.sh master --reset-only` |

With a probe attached, `pc` tells you the rest:

```bash
openocd -f interface/cmsis-dap.cfg -c "adapter speed 5000" \
        -f target/rp2350.cfg -c "init" -c "halt" -c "reg pc" -c "exit"
arm-none-eabi-addr2line -f -e master/build/frank-core2-master.elf <pc>
```

`pc == 0xeffffffe` means the core is in **lockup** — a fault escalated,
which is what `panic()` looks like from the outside once its breakpoint
executes with no debugger attached. Anything in flash resolves to a real
source line with `addr2line`.

## `probe/` — bring-up bisect target

A ~20-line firmware that does nothing but set the clock, bring up USB
stdio, and print a line per second. It exists to answer one question
when the master firmware comes up silent: *is this the diagnostic's
fault, or the board's?*

```bash
cd probe && cmake -S . -B build -DPICO_PLATFORM=rp2350 && cmake --build build -j8
picotool load -f build/frank-core2-probe.uf2 && picotool reboot -f
```

If the probe prints, the platform is sound and the fault is in the
diagnostic. If the probe is also silent, the problem is the clock, the
flash timing, or the board header — look there before reading a line of
diagnostic code.

## Reused from frank-msx

`drivers/` is copied verbatim from `frank-msx` so the two projects can
stay in sync:

- `pico_hdmi/` + `HDMI_hstx.c` — HSTX HDMI at 640x480, pins 12–19
- `sdcard/` + `fatfs/` — microSD on SPI0
- `audio.c` + `audio_i2s.pio` — I2S to the TDA1387
- `psram_init.c` — QMI setup for the ESP-PSRAM64H
- `usbhid/` — TinyUSB HID host

`common/board_config.h` is a shim that feeds the FRANK Core 2 Proto pin map to
those drivers under the name they expect, so they can be re-synced from
frank-msx without patching.

The master's pinout for HDMI (12–19), microSD (4–7), I2S (9/10/11) and
PSRAM CS (47) matches Murmulator 2.0 exactly, which is why the drivers
drop in unmodified.
