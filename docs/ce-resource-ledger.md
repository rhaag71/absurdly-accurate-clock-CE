# AAC-CE Resource Ledger

## Purpose and measurement conventions

This is the living resource ledger for Absurdly Accurate Clock Consumer
Edition. It records separate production and development build footprints,
active and planned hardware use, and open measurement needs. Update it at
meaningful milestones and retain prior entries in the milestone history.

Labels used below:

- **Build-derived**: emitted by PlatformIO, the linker, or an ELF inspection.
- **Calculated**: arithmetic from a cited build report or source constant.
- **Measured**: observed on running hardware with a measurement method stated.
- **Estimated**: engineering estimate, not directly measured.
- **Planned**: approved or proposed future use, not an active allocation.
- **Unknown / unverified**: no reliable evidence is available yet.

PlatformIO's memory summary is the primary profile comparison. Its `RAM used`
value equals ELF `.data + .bss` here; the ELF also contains a 272-byte
`.ram_vector_table` section. Both are shown so that the vector table is not
silently omitted from static SRAM accounting. The linker script reports a
512 KiB `RAM` region and separate 4 KiB `SCRATCH_X` and 4 KiB `SCRATCH_Y`
regions. Do not treat the main-RAM remainder as measured heap or stack margin:
runtime high-water use, allocator state, and contiguous free space have not
been measured. The linker script reserves 2 KiB stack sections in each
scratch region; this is a configured reservation, not a stack high-water
measurement.

## Current milestone and measurement environment

Baseline date: **2026-10-09**. Milestone: **Stage 2C.1 development-console
working-tree state**. Both firmware profiles built successfully, and the host
suite passed. The development console has not completed physical Pico 2
verification. This is the first recorded resource baseline; no previous
resource measurements or historical deltas are claimed.

| Item | Build evidence |
| --- | --- |
| Board / MCU | `rpipico2`; PlatformIO identifies Pico 2, RP2350, 150 MHz, 512 KiB RAM, 4 MiB flash |
| Platform | `raspberrypi` platform package 1.20.0+sha.5d4561a (configured from the pinned `platform-raspberrypi` revision `5d4561a05e3b212660ac6fdd3fbfb328d1988aa1`) |
| Framework | Arduino-Pico / `framework-arduinopico` 1.60100.0+sha.fd65f6d4 |
| Compiler package | `toolchain-rp2040-earlephilhower` 5.160100.260719; bundled GNU Arm toolchain reports 16.1.0 |
| Other reported tools | `tool-picotool-rp2040-earlephilhower` and `tool-pioasm-rp2040-earlephilhower` 5.160100.260719 |
| Build commands | `pio run -e pico2`; `pio run -e pico2-dev` |
| ELF section inspection | `arm-none-eabi-size -A .pio/build/<env>/firmware.elf`; `arm-none-eabi-nm -S -C .pio/build/<env>/firmware.elf` |

PlatformIO labels the platform `Raspberry Pi RP2040`; the same build output
identifies board Pico 2 and hardware RP2350. `platformio.ini` selects
`board = rpipico2`, so the RP2350 target is unambiguous. No linker map file was
generated. Build outputs and tool versions are build evidence, not physical
hardware measurements.

## Production and development memory comparison

Flash percentages use PlatformIO's reported maximum sketch size of 4,190,208
bytes (4 MiB flash with the configured reserved area) as the denominator.
Static SRAM percentages below use the 524,288-byte linker `RAM` region and ELF
`.data + .bss + .ram_vector_table`. Remaining values are arithmetic only.

| Resource | `pico2` production | `pico2-dev` development | Development minus production |
| --- | ---: | ---: | ---: |
| Flash used, PlatformIO sketch report (build-derived) | 46,624 B | 49,640 B | +3,016 B |
| Flash used / max sketch size (calculated) | 1.113% | 1.185% | +0.072 percentage points |
| PlatformIO maximum sketch capacity | 4,190,208 B | 4,190,208 B | — |
| Remaining sketch capacity (calculated from PlatformIO report) | 4,143,584 B | 4,140,568 B | −3,016 B |
| PlatformIO `RAM used` (`.data + .bss`) | 11,260 B (reported 2.1%) | 11,844 B (reported 2.3%) | +584 B |
| ELF static SRAM including `.ram_vector_table` (build-derived) | 11,532 B | 12,116 B | +584 B |
| ELF static SRAM / linker RAM region (calculated) | 2.200% | 2.311% | +0.111 percentage points |
| Linker `RAM` region | 524,288 B | 524,288 B | — |
| Main-RAM address space after those static sections (calculated) | 512,756 B | 512,172 B | −584 B |

### Stage 2C.2 measurements and change from Stage 2C.1

The table above remains the preserved Stage 2C.1 baseline. Measurements below
were taken from the Stage 2C.2 builds on **2026-10-10** using the same commands,
PlatformIO installation, linker region, and PlatformIO sketch-size report.

| Profile / resource | Stage 2C.1 baseline | Stage 2C.2 build | Absolute change |
| --- | ---: | ---: | ---: |
| `pico2` flash, PlatformIO sketch report | 46,624 B | 46,624 B | 0 B |
| `pico2` static SRAM including vector table | 11,532 B | 11,532 B | 0 B |
| `pico2-dev` flash, PlatformIO sketch report | 49,640 B | 51,648 B | +2,008 B |
| `pico2-dev` static SRAM including vector table | 12,116 B | 12,132 B | +16 B |

Stage 2C.2 percentages, calculated from the same capacities: production flash
1.113% and static SRAM 2.200%; development flash 1.233% and static SRAM
2.314%. The development increase is 0.048 percentage points of maximum sketch
capacity and 0.003 percentage points of the linker `RAM` region. Calculated
remaining sketch capacity is 4,143,584 B for production and 4,138,560 B for
development. Main-RAM address space after static sections is 512,756 B and
512,156 B respectively; these remain linker arithmetic, not runtime margins.

The development PlatformIO RAM report is 11,860 B (`.data + .bss`, 2.3% rounded)
and its ELF static SRAM total including the unchanged 272-byte vector table is
12,132 B. ELF section changes from the Stage 2C.1 measurements are `.text`
+928 B, `.rodata` +800 B, `.data` +280 B, and `.bss` −264 B. The net static SRAM
increase is 16 B. No linker map file is available to attribute the flash delta
to individual functions or data objects. Production measurements are unchanged,
consistent with the production source filter excluding simulator and console
code.

No new fixed-capacity buffer was introduced. The development simulator now
stores independent receiver and delivered pulse snapshots plus the GPS/PPS/RMC
control state; the total build-derived static SRAM delta is +16 B, but the
individual simulator object allocation is not isolated in the linker report.
The console's existing buffers are unchanged. No GPIO, UART, interrupt, PIO,
DMA, or other peripheral assignment was added. No application PIO or DMA
allocation was found. Control parsing and fault selection add bounded
development-only work; RMC checksum generation still traverses the bounded
sentence, and late mode omits GGA for that cycle. CPU time and timing margin
were not measured.

Both profiles built successfully without compiler or linker warnings in the
captured output. Host tests, including simulated faults, passed. **Hardware
testing is pending:** Stage 2C.2 fault behavior has not been verified on a
physical Pico 2.

PlatformIO rounds its RAM percentages to one decimal place. Its RAM summary
does not include the 272-byte `.ram_vector_table`; the ELF section totals do.
The “remaining” figures describe link-time region arithmetic only. They are
not measured runtime heap, stack, or application margin. The ELF `.heap`
section spans the remainder of the main RAM region by linker-script design;
that section size must not be interpreted as heap already available to the
application at runtime.

PlatformIO reports `Sketch size` rather than the complete packaged binary
length. The generated `firmware.bin` files were 58,936 B (`pico2`) and 61,952 B
(`pico2-dev`), a 3,016 B difference. The binary includes image/partition and
end metadata outside the PlatformIO sketch-size figure; use the PlatformIO
summary consistently for this profile comparison. Report both figures if a
future milestone changes the build packaging or measurement method.

The profile difference is not a historical growth delta. Major source-level
differences are the production real GPS input versus development simulated
GPS/PPS input, plus the development-only USB console and its buffers. The
shared parsers, timebase, display abstraction, and PD-2200 backend are present
in both. No per-module flash or SRAM attribution is inferred from total build
differences.

Build warnings/resource concerns: both PlatformIO builds succeeded without
compiler or linker warnings in the captured output. The PlatformIO platform
name discrepancy is recorded above. Runtime stack/heap margin and peripheral
usage hidden inside framework code remain unmeasured or unverified.

## GPIO allocation

GPIO numbers are Pico-relative. Current assignments were checked against
[`include/pins.hpp`](../include/pins.hpp), hardware initialization, and the
selected backend implementation.

| GPIO | Function / owner | Production `pico2` | Development `pico2-dev` | Status / notes |
| --- | --- | --- | --- | --- |
| GP0 | GPS UART0 TX (`Serial1`) | Active | Unallocated | Real GPS initializer is excluded from development. |
| GP1 | GPS UART0 RX (`Serial1`) | Active | Unallocated | Real GPS initializer is excluded from development. |
| GP2 | GPS PPS input, rising-edge interrupt | Active | Unallocated | No physical PPS interrupt is registered for simulated input. |
| GP4 | PD-2200 UART1 TX (`Serial2`) through MAX3232 | Active | Active | Backend sets UART1 TX to GP4 in both profiles. |
| GP5 | PD-2200 UART1 RX | Reserved / not driven | Reserved / not driven | Pin map reserves GP5; backend calls `Serial2.setRX(-1)` for transmit-only use. |
| GP8 | Proposed WS2812 DATA | Planned | Planned | No driver, pin setup, or active allocation. |
| GP6, GP9–GP22 | No current CE assignment identified | Unassigned | Unassigned | GP8 is separately listed above. No peripheral use is inferred from RP2350 capability. |
| Board-defined `LED_BUILTIN` | Heartbeat output in `main.cpp` | Active | Active | Source uses the board macro; no numeric GPIO is assigned in project pin map, so the exact mapping is not asserted here. |

No physical GPS UART/PPS pins are initialized by the development profile.
Production and development both initialize the current PD-2200 UART output.
No additional wiring, connector orientation, or WS2812 pixel order is
established by this ledger.

## Peripheral, PIO, and DMA accounting

| Resource | Current status | Ownership / evidence |
| --- | --- | --- |
| UART0 | Active in production only | GPS UART at configured 9,600 baud, 8N1, GP0/GP1. Development excludes `gps_input_real.cpp` and `hardware.cpp`; simulated events use no physical GPS UART. |
| UART1 | Active in both profiles | PD-2200 transmit-only at 9,600 baud, 8N1, GP4 through MAX3232; RX is disabled and GP5 is reserved. |
| USB CDC / `Serial` | Active in both profiles | USB diagnostics at 115,200 baud. Development also uses the console. I/O is serviced in bounded per-loop work. USB core internal buffering is not separately inventoried. |
| GPS PPS interrupt | Active in production only | Rising-edge `attachInterrupt` on GP2 captures `micros()` and increments the real-source sequence. No PPS ISR is attached in development. |
| Watchdog | Active in both profiles | RP2350 watchdog enabled for 4,000 ms. The main loop feeds it after input, diagnostics, heartbeat, timebase, and display service complete. |
| Hardware timers / alarm pool | Framework activity present; application allocation unknown | Application calls `micros()`/`millis()` and does not explicitly claim a timer. ELF contains framework `default_alarm_pool_entries`; exact hardware timer and alarm ownership/use is not established by source inspection. |
| SPI controllers | Unallocated by application | No active application SPI initialization or transaction was found. Historical Bridge/SPI documentation is not an active allocation. |
| I2C controllers | Unallocated by application | No active application I2C initialization was found. |
| PWM | Unallocated by application | No application PWM setup was found. |
| PIO blocks / state machines | No application allocation found | No application PIO program load or state-machine claim was found. Arduino-Pico PIO support in the framework does not itself prove an application allocation. |
| PIO instruction memory | Unallocated by application | No application PIO program is loaded. Planned WS2812 use is not active. |
| DMA channels | No application allocation found; framework ownership not fully verified | No application DMA claim/allocation was found. Do not infer free-channel count until framework and runtime ownership are inspected. |

The framework contains peripheral support code, and framework-owned USB,
timer, interrupt, or DMA activity may exist outside application source. This
ledger records only verified application ownership and identified framework
evidence; it does not claim the RP2350 resources are otherwise free.

## Static buffers, stack, and heap

The following source capacities are directly inspectable. They are already
included in the linker totals above and must not be added to those totals a
second time.

| Storage | Capacity / evidence | Profile presence |
| --- | --- | --- |
| Production diagnostic queue | 1,024 bytes, `usb_queue` in `src/main.cpp`; ELF symbol confirms 0x400 bytes | Production only |
| Development console diagnostics queue | 1,024 bytes | Development only |
| Development console response queue | 512 bytes | Development only |
| Development console command line | 80 bytes (79 input characters plus terminator) | Development only |
| Development console object | 1,656 bytes total in ELF symbol (`nm`); includes the three arrays above plus state/counters/padding | Development only |
| Simulated NMEA stream storage | 192 bytes | Development only |
| Simulated fault state and receiver/delivered pulse snapshots | Added in Stage 2C.2; no new buffer. Whole-profile static SRAM change is build-derived at +16 B; individual object size is not isolated. | Development only |
| RMC parser line storage | 128 bytes | Both profiles |
| GGA parser line storage | 128 bytes | Both profiles |
| Display frame | Two 21-byte rows (20 display cells plus terminator per row), from `clock_display::Frame` | Both profiles |
| Display output command | 5 bytes maximum per `clock_display::Output` | Both profiles |
| GPS source, timebase, reception and display objects | Persistent globals in `src/main.cpp`; included in ELF totals. Aggregate per-object attribution was not established. | Profile-dependent source; shared timebase/display |

ELF inspection with `arm-none-eabi-nm -S -C` confirmed the production
1,024-byte queue and the development 1,656-byte console object. No linker map
file was produced, so individual object totals beyond directly named symbols
are not assigned from the whole-image delta.

Runtime heap use and stack high-water marks: **unknown / unmeasured** for both
profiles. The linker report does not establish runtime free heap, peak stack,
or the largest contiguous free block. Future measurement should use a
documented runtime method and representative worst-case console, parser,
display-backpressure, and interrupt workloads. Do not add memory
instrumentation without a separate implementation task.

Some functions use bounded automatic character arrays (including 80–128 byte
formatting buffers). Their maximum stack depth and interrupt interaction have
not been measured; no stack consumption is estimated here.

## Timing and watchdog constraints

These are configured limits and algorithmic safeguards from source, not
measured hardware timing or CPU-utilization results.

| Constraint | Configured / implemented value | Resource significance |
| --- | --- | --- |
| GPS PPS nominal cadence | 1 Hz receiver contract | Authoritative boundaries come from captured PPS, not RMC arrival. |
| PPS cadence qualification | Consecutive sequence and 900,000–1,100,000 µs period | Out-of-window or skipped edges prevent qualification. |
| PPS presence timeout | 1,500,000 µs | Timebase invalidates PPS presence after timeout. |
| RMC association window | Sentence start at least 20,000 µs after PPS; completion no later than 900,000 µs after PPS | Whole-second RMC label is associated with the preceding pulse. |
| GPS freshness / association timeout | 3,000,000 µs | Missing valid RMC or usable association unlocks the timebase. |
| Main-loop UART service gap | More than 20,000 µs triggers association discard and startup drain | Buffered serial bytes cannot safely be assigned fresh processing timestamps after a long stall. This is a configured safeguard, not a measured worst-case loop time. |
| GPS bytes processed per loop | At most 64 | Bounds input work so GPS traffic cannot starve later loop services. |
| Simulator schedule | 1,000,000 µs PPS period; RMC starts 200,000 µs after its pulse; events over 20,000 µs late are dropped | Development-only synthetic timing; not evidence of physical timing accuracy. |
| Simulated fault schedules | EARLY RMC starts at 5,000 µs after PPS (first byte timestamp 6,041 µs); LATE starts at 880,000 µs (CR completion 916,458 µs); LATE omits GGA | Deterministic development-only schedules, verified in native virtual-time tests. Not physical receiver measurements. |
| Development console input/output | Up to 32 input bytes and 64 output bytes per loop | Bounded USB service; no measured USB throughput or timing margin. |
| Display output | At most one PD-2200 command byte per service call when UART writable | Backend work is bounded; actual wire/backpressure behavior is hardware dependent. |
| Watchdog | 4,000 ms timeout; recurring feed at end of completed main-loop services | Hardware reset protection is implemented. Worst-case feed interval and physical reset behavior are not measured by these builds. |

No loop-frequency, interrupt-latency, UART-overrun, USB-throughput, CPU-load,
or timing-margin measurements were taken for this baseline. Existing PD-2200
startup delays are 100 ms in display initialization and 500 ms for separately
powered display settling; these occur during startup, not as recurring loop
delays.

## Display backend resource considerations

### Current backend: PD-2200 / Noritake-compatible VFD

- Active in both profiles; uses UART1 (`Serial2`) transmit on GP4 at 9,600 baud,
  8N1, through a MAX3232. RX is disabled; GP5 is reserved.
- Presentation uses a 20-column, two-row frame. Output retains one submitted
  frame and one in-flight command buffer of at most five bytes; it does not
  queue animation frames. Frames and command storage are included in linker
  totals.
- Each backend service call submits at most one byte when UART reports writable.
  UART acceptance is not physical display readback. UART latency, display
  response, and backpressure behavior require hardware observation.
- The current PD-2200 command protocol and experimentally observed device
  behavior are documented in the [engineering baseline](ce-engineering-baseline.md)
  and [PPS/timebase notes](pps-timebase.md). Do not generalize model-specific
  observations to other displays.

### Planned backend: one 8x32 WS2812B RGB panel

- **Planned, not allocated:** 256 RGB pixels; proposed data output GP8 and
  PIO-driven transmission. No PIO state machine, instruction memory, DMA
  channel, or framebuffer is currently allocated for WS2812.
- A tightly packed logical RGB framebuffer would require **768 bytes**
  (`256 pixels × 3 bytes/pixel`, calculated); metadata, mapping, alignment,
  staging, or double-buffering would add to that value. This is not current
  SRAM usage.
- Planned electrical elements are an SN74HCT541N level shifter powered from
  5 V, approximately 330-ohm series data resistor, 100 nF local bypass, and
  shared Pico/display ground. These details are not hardware qualified.
- Preserve backend independence and future heterogeneous multi-display
  composition. The timing engine must not wait on slow output. Resource budgets
  for PIO, DMA, refresh time, buffers, and concurrent backends must be measured
  or derived from an implemented design when that work is authorized.

## Power and physical measurement gaps

No resource figures in this section are firmware build measurements.

| Item | Status |
| --- | --- |
| WS2812 panel current and brightness/current curve | Unknown; measure on the selected panel across intended brightness settings. |
| Panel thermal behavior | Unknown; requires representative enclosed operating measurements. |
| 5 V supply capacity and margin | The 5 V / 2 A figure is a provisional planning target, not a verified requirement. Confirm panel, Pico, level-shifter, and transient loads before selecting limits. |
| HCT541 logic levels and data integrity | Planned circuit; actual Pico-to-panel waveform and timing have not been measured. |
| Matrix pixel mapping and physical orientation | Not established; do not assume serpentine order, connector orientation, or first-pixel location. |
| Diffuser, enclosure, and optical effects | Not measured; evaluate readability, thermal impact, and light leakage on the physical assembly. |
| GPS PPS/RMC hardware timing | Physical GPS receiver setup and whole-second association timing require receiver-specific observation. Simulator qualification does not establish these properties. |
| Development USB console on Pico 2 | Stage 2C.1 physical console verification is complete per current project status. Stage 2C.2 fault behavior remains physically unverified. |

## Resource concerns and action items

1. Keep future production and development build measurements separate and
   compare them with this first baseline using the same PlatformIO/toolchain
   measurement method where possible.
2. Measure runtime stack high-water and heap behavior on representative hardware
   before making claims about remaining runtime memory. Preserve the distinction
   between the linker RAM region, scratch banks, and runtime allocator/stack use.
3. Inspect framework/runtime peripheral ownership before assigning DMA,
   hardware timers, PIO, or other resources to future backends.
4. Before WS2812 implementation, establish the actual panel's power, thermal,
   electrical timing, mapping, buffer, PIO, and DMA requirements. Keep all such
   values marked planned until measured or build-derived.
5. Verify physical GPS PPS/RMC timing and complete Stage 2C.2 fault-injection
   checks before recording those behaviors as physically qualified. Stage
   2C.1 console verification is complete per current project status.

## Milestone history

| Date | Milestone | Production (`pico2`) | Development (`pico2-dev`) | Notes |
| --- | --- | --- | --- | --- |
| 2026-10-09 | First resource baseline; Stage 2C.1 development-console working-tree state | Flash 46,624 B; static SRAM including vector table 11,532 B | Flash 49,640 B; static SRAM including vector table 12,116 B | Both builds and host suite passed. Console remains unverified on physical Pico 2. No prior baseline or historical resource delta exists. |
| 2026-10-10 | Stage 2C.2 development-only simulated GPS/PPS fault injection | Flash 46,624 B (Δ 0); static SRAM 11,532 B (Δ 0) | Flash 51,648 B (Δ +2,008); static SRAM including vector table 12,132 B (Δ +16) | Both builds and host fault/regression suite passed. No GPIO/peripheral assignment changes. Physical Pico 2 fault-injection verification is **PENDING**. |
