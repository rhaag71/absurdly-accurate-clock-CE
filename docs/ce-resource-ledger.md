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

### WS2812 bring-up profile (2026-10-11)

The initial dedicated `pico2-dev-ws2812` profile was built with the same
pinned PlatformIO platform and Arduino-Pico framework. That earlier renderer
used 47,532 B flash and 12,312 B `.data + .bss`; its driver object was 792 B.
The selected profile replaces the default STDOUT display, so whole-profile
deltas are not the framebuffer size alone.

Flash use is 1,012 B above default development. This includes the PIO driver,
bring-up sequencer, and SDK PIO calls; no per-function flash attribution is
claimed. Fresh default builds report `pico2` 44,888 B flash / 11,312 B
`.data + .bss`, and `pico2-dev` 46,520 B / 11,632 B. Static SRAM totals
including the 272-byte vector table are 11,584 B and 11,904 B. The WS2812
buffer and PIO instruction image are absent from both defaults.

The following logical mapping/effects profile is retained as a historical
measurement; that renderer was later removed after target-runtime problems.
The restored raw bring-up profile is retained as the preceding milestone; the
latest corner-proof build measurements are recorded in the milestone table.

### Restored raw bring-up build (2026-10-11)

Fresh `pico2-dev-ws2812` build after removing logical mapping, effects, and
startup bisect code: 47,396 B flash and 12,312 B `.data + .bss`; adding the
272-byte vector table gives 12,584 B static SRAM. This is 1,780 B less flash
and 8 B less `.data + .bss` than the removed full-demo build. It is 136 B less
flash than the historical first bring-up measurement (47,532 B); `.data +
.bss` is unchanged from that historical build. The linked display object is
792 B, including the 768-byte framebuffer and driver state; the PIO instruction
image adds 8 B of SRAM. At that milestone the restored image awaited physical
reverification; the user subsequently confirmed that raw-fill/chase image,
four-corner proof, perimeter trace, and row/column scans on hardware. The
temporary 5x7 font remains available as a known-good reference. The full-height
6x8 candidate was subsequently evaluated and judged too large. The first
compact variable-width font preserved useful spacing but was physically
judged visually unattractive, and the subsequent soft compact candidate was
not accepted. The approved variable-width 5x7 clock font is implemented;
exact glyph appearance awaits physical evaluation. It draws at y=0..6 and
leaves row 7 unused, while preserving that row for future overlays.

The selected driver owns GP8, PIO0 SM0, and four instruction words at fixed
offset 0. It checks SM0 before claiming it; a conflict leaves GP8 low and
stops initialization. It uses no DMA and adds no interrupt. At 150 MHz,
divider 15 gives a 10 MHz PIO clock; ten cycles encode each 800 kHz bit.
T0H/T0L are 3/7 cycles; T1H/T1L are 6/4 cycles. The service loop queues at
most eight 24-bit pixel words per application pass and uses a nonblocking 1 ms
interval after the final FIFO write. These are source/build-derived values;
the exact waveform has not been instrumentally measured. User reports the
panel visibly responds to the raw-index chase. Its topology is physically
established as top-left-start vertical serpentine columns; the logical mapping
and four-corner proof were subsequently verified on hardware.

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
shared parsers, timebase, framework, and default STDOUT driver are present in
both. No per-module flash or SRAM attribution is inferred from total build
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
| GP4 | Optional PD-2200 UART1 TX (`Serial2`) through MAX3232 | Unallocated | Unallocated | Claimed only when `AAC_DISPLAY_VFD` is selected. |
| GP5 | Unassigned / available | Unallocated | Unallocated | VFD has no RX GPIO; `Serial2.setRX(-1)` disables UART1 RX when VFD is selected. |
| GP8 | WS2812 DATA | Unallocated | Active only in `pico2-dev-ws2812` | Set low as GPIO before PIO mux; external SN74HCT541N and approximately 330-ohm resistor connect to panel DIN. |
| Separate WS2812 status DATA | GPIO TBD | Unallocated | Unallocated | Planned separate chain of approximately 3–4 status LEDs, through another SN74HCT541N channel and its own approximately 330-ohm resistor. No pin is assigned or reserved before driver implementation. |
| GP6, GP9–GP22 | No current CE assignment identified | Unassigned | Unassigned | GP8 is separately listed above. No peripheral use is inferred from RP2350 capability. |
| Board-defined `LED_BUILTIN` | Heartbeat output in `main.cpp` | Active | Active | Source uses the board macro; no numeric GPIO is assigned in project pin map, so the exact mapping is not asserted here. |

No physical GPS UART/PPS pins are initialized by the development profile. Default production and development builds select STDOUT and initialize no VFD pins or UART1.
No additional wiring, connector orientation, or WS2812 pixel order is
established by this ledger.

## Peripheral, PIO, and DMA accounting

| Resource | Current status | Ownership / evidence |
| --- | --- | --- |
| UART0 | Active in production only | GPS UART at configured 9,600 baud, 8N1, GP0/GP1. Development excludes `gps_input_real.cpp` and `hardware.cpp`; simulated events use no physical GPS UART. |
| UART1 | Unallocated in default profiles; conditional optional VFD resource | When `AAC_DISPLAY_VFD` is selected, PD-2200 uses transmit-only 9,600 baud, 8N1 on GP4 through MAX3232; RX disabled. Arduino-Pico exposes a global `Serial2` object in the default ELF, but application code does not call or initialize it. |
| USB CDC / `Serial` | Active in both profiles | USB diagnostics at 115,200 baud. Development also uses the console. I/O is serviced in bounded per-loop work. USB core internal buffering is not separately inventoried. |
| GPS PPS interrupt | Active in production only | Rising-edge `attachInterrupt` on GP2 captures `micros()` and increments the real-source sequence. No PPS ISR is attached in development. |
| Watchdog | Active in both profiles | RP2350 watchdog enabled for 4,000 ms. The main loop feeds it after input, diagnostics, heartbeat, timebase, and display service complete. |
| Hardware timers / alarm pool | Framework activity present; application allocation unknown | Application calls `micros()`/`millis()` and does not explicitly claim a timer. ELF contains framework `default_alarm_pool_entries`; exact hardware timer and alarm ownership/use is not established by source inspection. |
| SPI controllers | Unallocated by application | No active application SPI initialization or transaction was found. Historical Bridge/SPI documentation is not an active allocation. |
| I2C controllers | Unallocated by application | No active application I2C initialization was found. |
| PWM | Unallocated by application | No application PWM setup was found. |
| PIO blocks / state machines | No application allocation in defaults | `pico2-dev-ws2812` selects PIO0 SM0 explicitly (`pio_sm_is_claimed` then `pio_sm_claim`). If SM0 is already claimed, initialization leaves GP8 low and returns without starting the panel. PIO0 SM1 is not allocated or reserved; it is only a preferred future option for the separate status driver if available. |
| PIO instruction memory | Unallocated by default; four words in selected profile | The WS2812 program is fixed at PIO0 instruction offset 0 and uses four of 32 PIO0 instruction words. No application PIO program competes in this profile. |
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
| STDOUT pending output | 112-byte fixed line buffer, replaceable by current state; each service submits at most one whole line when CDC has room | Both default profiles |
| WS2812 RGB framebuffer | 256 × 3-byte RGB values = 768 bytes; current `display` composition symbol is 796 bytes including driver state; PIO instruction image adds 8 bytes | `pico2-dev-ws2812` only; absent from default images |
| VFD frame and output command | Two 21-byte rows and 5-byte maximum command, from `clock_display::Frame`/`Output` | VFD-selected builds only; absent from defaults |
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
| Display output | STDOUT submits at most one bounded line per service; VFD emits at most one payload byte per service after bounded startup command batches; WS2812 queues no more than eight words per service and uses a 1 ms nonblocking latch interval | Software service work is bounded; USB/UART/PIO electrical behavior is hardware dependent. |
| Watchdog | 4,000 ms timeout; recurring feed at end of completed main-loop services | Hardware reset protection is implemented. Worst-case feed interval and physical reset behavior are not measured by these builds. |

No loop-frequency, interrupt-latency, UART-overrun, USB-throughput, CPU-load,
or timing-margin measurements were taken for this baseline. Optional VFD power-on settling and reset/configure phases are now advanced
using elapsed-time checks and bounded UART writes; no startup delay blocks the
main loop. The optional WS2812 driver uses the bounded PIO service described
above. Raw panel response and its top-left-start vertical serpentine column
topology were physically verified; waveform measurement and corrected corner
frame behavior remain unverified.

## Display backend resource considerations

### Default backend: STDOUT

- Selected automatically in both standard profiles; uses USB CDC output and no
  display GPIO or UART peripheral. It keeps one replaceable 112-byte line and
  submits at most one line (up to 112 bytes) per service when the USB sink
  reports sufficient room. It replaces stale pending content rather than
  queueing frames. Development
  Console Mode suppresses STDOUT output; pending console responses/diagnostics
  take priority.

### Optional backend: PD-2200 / Noritake-compatible VFD

- Not selected in default profiles. When explicitly selected, it uses UART1
  (`Serial2`) TX on GP4 (physical pin 6) at 9,600 baud, 8N1, through a MAX3232.
  RX is disabled and GP5 remains unassigned. MAX3232 labeling and DB9 wiring
  quirks are external hardware details and do not change the Pico GPIO contract.
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

### Optional backend: one 8x32 WS2812 RGB panel

- The selected driver owns GP8 (Pico 2 physical pin 11), PIO0 SM0, four PIO
  instruction words, and a 768-byte fixed RGB buffer. Its program is fixed at
  offset 0; initialization checks that SM0 is available before claiming it.
- No DMA channel or interrupt is used. Pixels serialize as WS2812 GRB, most
  significant bit first. Raw physical index access remains available. The
  logical 32x8 mapping matches the physically established topology: raw index
  zero at top-left, columns progress left-to-right, and traversal alternates
  vertically, starting top-to-bottom in the first column.
- At the configured 150 MHz system clock, PIO divider 15 yields 10 MHz and ten
  cycles per bit (800 kHz). T0H/T0L are 3/7 cycles; T1H/T1L are 6/4 cycles.
  The driver waits nonblockingly for 1 ms after its final FIFO write before
  sending another frame, including FIFO drain and a conservative reset-low
  interval.
- Development starts dark, shows fixed component-8 red, green, and blue fills,
  returns dark, presents red/green/blue/yellow logical corners one at a time
  (1.8 s each), returns dark, traces all 76 logical perimeter pixels clockwise
  once, returns dark, scans rows 0..7 for 500 ms each and columns 0..31 for
  150 ms each, returns dark, displays fixed `12:34` for 4.5 seconds, returns
  dark, then chases one dim white pixel through raw indices 0..255. This was
  the earlier bring-up runtime; it is retired from normal clock-mode runtime.
  The raw chase, panel topology, corners, perimeter, rows, columns, and 5x7
  glyph rendering were physically verified. Waveform measurements remain
  unavailable.
- Current development behavior renders civil `HH:MM` from the shared
  `presentation::State`, using its UTC validity and zone. The physical face
  always uses unpadded 12-hour time, with midnight/noon as 12. The approved
  5x7 clock glyphs use declared widths 3..5 for digits and 1 for the colon,
  and center by measured string width. Glyphs draw at y=0..6; row 7 is unused
  by the primary time renderer and remains
  available for future overlays. The original bring-up 5x7, 6x8, first compact,
  and soft compact candidates remain available. The 6x8 was judged too large;
  earlier compact fonts were not approved. Exact approved-glyph physical
  evaluation remains pending. Default logical amber `(255, 96, 0)` at
  brightness 8/255 yields effective components `(8, 3, 0)`. Invalid UTC clears
  the last clock frame.
  Global software brightness is applied during output serialization, with
  default 8/255 and rounded integer scaling `(channel * brightness + 127) /
  255`. Development console `brightness` reports it and `brightness <0-255>`
  changes it. The development-only `color` and `color <r> <g> <b>` commands
  report/set temporary logical RGB (each channel 0..255); a color-generation
  change redraws and retransmits the same `HH:MM` even when the minute is
  unchanged. Logical color stays full scale and upstream of brightness and any
  future transfer correction. Brightness-generation tracking also retransmits
  the current frame after a change. Brightness and color remain independent.
  This is an
  appearance control, not current limiting; panel 5 V supply handling is
  untouched. Gamma/perceptual correction, a separate current-budget limiter,
  and ambient-light control remain future work. Current limiting may later
  scale outgoing frames against a measured/configured budget; its ordering
  relative to transfer correction is undecided. The user physically verified
  clock mode and reports useful brightness through the grid/diffuser, but no
  final calibration or electrical current measurement is established.
- The external path uses the wired SN74HCT541N and approximately 330-ohm series
  resistor to panel DIN. Development uses a separate regulated 5 V panel supply
  and shared ground. Current, signal quality, and thermal behavior are unknown.

### Planned separate WS2812 status chain

- Approximately 3–4 discrete WS2812 LEDs are a separate logical/electrical
  device. They are not appended to the 256-pixel panel chain. This preserves
  fixed panel geometry and lets panel rendering/transmission and status updates
  remain independently owned and serviceable.
- The future status driver owns its LED count, meanings/colors, data GPIO, and
  update cadence. The main panel driver owns its 32x8 geometry, framebuffer,
  clock presentation, and applicable themes/animations. The drivers should not
  depend on each other; they may share a low-level WS2812 PIO transport.
- Intended data path: unassigned Pico GPIO → spare channel on the existing
  SN74HCT541N → separate approximately 330-ohm series resistor → status LEDs.
  No GPIO has been selected or reserved.
- PIO0 SM1 is preferred for status only if available when implemented. It is
  not an allocation or reservation. Reusing the panel's loaded four-instruction
  WS2812 program is preferred where practical. No status framebuffer, PIO
  state machine, instruction memory, DMA, or interrupt is currently allocated.

## Power and physical measurement gaps

No resource figures in this section are firmware build measurements.

| Item | Status |
| --- | --- |
| WS2812 panel current and electrical response | Unknown; software brightness is an appearance control, not a current limit. The user reports development appearance observations through the installed grid/diffuser at brightness 1..16; measure actual current and validate output transfer on this panel before setting electrical limits or a gamma curve. |
| Panel thermal behavior | Unknown; requires representative enclosed operating measurements. |
| 5 V supply capacity and margin | The 5 V / 2 A figure is a provisional planning target, not a verified requirement. Confirm panel, Pico, level-shifter, and transient loads before selecting limits. |
| HCT541 logic levels and data integrity | Wired SN74HCT541N plus approximately 330-ohm series resistor; actual Pico-to-panel waveform and timing have not been measured. Panel uses separate regulated 5 V during development and shares ground. |
| Matrix pixel mapping and physical orientation | Established by raw chase, panel silkscreen, four-corner proof, perimeter trace, row/column scans, and live `HH:MM` clock: raw zero is top-left; columns progress left-to-right with alternating vertical direction, first column top-to-bottom. |
| Black pixel grid and diffuser | User reports installed/tested, producing distinct square pixels and substantially more useful apparent brightness than the bare panel. Brightness 1 is extremely dim and shifts amber red; 2 remains usable but very dim; 2..16 are useful development values. Exact production brightness and optical calibration remain open; enclosure, thermal impact, and light leakage still need evaluation. |
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
4. Verify the live clock mode, confirm the PIO waveform and electrical
   levels, and establish power/thermal behavior before final WS2812 rendering.
   Current and signal-integrity limits remain unmeasured.
5. Verify physical GPS PPS/RMC timing and complete Stage 2C.2 fault-injection
   checks before recording those behaviors as physically qualified. Stage
   2C.1 console verification is complete per current project status.

## Milestone history

| Date | Milestone | Production (`pico2`) | Development (`pico2-dev`) | Notes |
| --- | --- | --- | --- | --- |
| 2026-10-09 | First resource baseline; Stage 2C.1 development-console working-tree state | Flash 46,624 B; static SRAM including vector table 11,532 B | Flash 49,640 B; static SRAM including vector table 12,116 B | Both builds and host suite passed. Console remains unverified on physical Pico 2. No prior baseline or historical resource delta exists. |
| 2026-10-10 | Stage 2C.2 development-only simulated GPS/PPS fault injection | Flash 46,624 B (Δ 0); static SRAM 11,532 B (Δ 0) | Flash 51,648 B (Δ +2,008); static SRAM including vector table 12,132 B (Δ +16) | Both builds and host fault/regression suite passed. No GPIO/peripheral assignment changes. Physical Pico 2 fault-injection verification is **PENDING**. |
| 2026-10-10 | Compile-time display framework and default STDOUT | Flash 44,888 B (Δ −1,736); `.data + .bss` 11,312 B; static SRAM including vector table 11,584 B (Δ +52) | Flash 46,520 B (Δ −5,128); `.data + .bss` 11,632 B; static SRAM including vector table 11,904 B (Δ −228) | Build-derived from default STDOUT profiles. Removed default VFD-linked code and VFD buffers; exact per-driver deltas are not isolated. One-off `pico2` VFD-only build: 46,664 B flash / 11,268 B `.data + .bss` / 11,540 B including vector table. One-off STDOUT+VFD build: 46,672 B / 11,400 B / 11,672 B including vector table. Host regression suite and both default builds passed; no physical verification. |
| 2026-10-11 | WS2812 PIO bring-up driver and raw-index development renderer | `pico2`: 44,888 B flash / 11,312 B `.data + .bss`; `pico2-dev`: 46,520 B / 11,632 B; default values match display-framework milestone | `pico2-dev-ws2812`: 47,532 B flash; 12,312 B `.data + .bss`; 12,584 B including vector table | Selected build owns GP8; PIO0 SM0, instruction offset 0 and four instruction words; no DMA or interrupt; 768-byte RGB framebuffer and 8-byte PIO instruction image. Dev bring-up is bounded to eight FIFO words per service and a 1 ms post-write latch interval. Raw-index chase was subsequently physically verified; exact mapping and instrumented waveform/electrical measurements remain unavailable. |
| 2026-10-11 | Logical 32x8 mapping and full demo | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B / 11,632 B (unchanged) | `pico2-dev-ws2812`: 49,176 B flash (Δ +1,644); 12,320 B `.data + .bss` (Δ +8); 12,592 B including vector table (Δ +8) | WS2812 `display` object is 800 B (768-byte framebuffer + 32-byte state); PIO image remains 8 B. GP8, PIO0 SM0/four instruction words unchanged; no DMA, interrupts, or extra GPIO. Defaults have no WS2812 framebuffer or PIO allocation (confirmed by default ELF symbols). The user reports raw-chase response; logical mapping remains provisional pending corner/perimeter observation. |
| 2026-10-11 | Restored first physically working raw bring-up behavior | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B / 11,632 B (unchanged) | `pico2-dev-ws2812`: 47,396 B flash; 12,312 B `.data + .bss`; 12,584 B including vector table | Build-derived restored profile; 1,780 B less flash and 8 B less `.data + .bss` than the removed full-demo profile. 792-byte display object contains the fixed 768-byte RGB buffer and state; PIO instruction image is 8 B. Default ELF symbol inspection finds no WS2812 driver/PIO symbols. GP8, PIO0 SM0, four instruction words, no DMA/interrupt remain the selected resource contract. The user subsequently physically verified that simple image. |
| 2026-10-11 | Provisional logical map and four-corner proof | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B / 11,632 B (unchanged) | `pico2-dev-ws2812`: 47,532 B flash (Δ +136); 12,312 B `.data + .bss` (Δ 0); 12,584 B including vector table (Δ 0) | Build-derived. Added the bounds-checked logical 32x8 helper and four individually held corner frames; framebuffer and driver object size remain unchanged. Transport/resource ownership unchanged. Default ELFs contain no WS2812/PIO symbols. Corner mapping is not physically verified. |
| 2026-10-11 | Correct logical topology to observed vertical serpentine columns | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B / 11,632 B (unchanged) | `pico2-dev-ws2812`: 47,524 B flash (Δ -8); 12,312 B `.data + .bss` (Δ 0); 12,584 B including vector table (Δ 0) | Build-derived. Corrected only the logical-to-raw mapping helper and its focused tests/docs. The 768-byte framebuffer, GP8, PIO0 SM0/four instruction words, DMA/interrupt use, and raw chase are unchanged. Default ELFs remain free of linked WS2812/PIO resources. Physical topology is established from the observed raw chase; corrected corner frames await re-verification. |
| 2026-10-11 | Logical perimeter trace | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B / 11,632 B (unchanged) | `pico2-dev-ws2812`: 47,828 B flash (Δ +304); 12,312 B `.data + .bss` (Δ 0); 12,584 B including vector table (Δ 0) | Build-derived. Added a bounds-checked 76-step perimeter coordinate helper and one-lap demo state using the existing pixel-index storage. Verified mapping, transport, GP8/PIO0 SM0 ownership, framebuffer, and raw chase are unchanged. Default ELFs contain no linked WS2812/PIO resources. Perimeter awaited physical verification at this measurement milestone; user subsequently confirmed it. |
| 2026-10-11 | Logical row and column scans | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B / 11,632 B (unchanged) | `pico2-dev-ws2812`: 48,076 B flash (Δ +248); 12,312 B `.data + .bss` (Δ 0); 12,584 B including vector table (Δ 0) | Build-derived. Added bounded 32-pixel row frames held 500 ms and 8-pixel column frames held 150 ms, reusing the existing framebuffer and pixel index. Mapping, PIO/transport, ownership, and raw chase are unchanged. Default ELFs have no linked WS2812/PIO symbols. Scans await physical verification. |
| 2026-10-11 | Temporary 5x7 numeric glyph proof | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B / 11,632 B (unchanged) | `pico2-dev-ws2812`: 48,408 B flash (Δ +332); 12,312 B `.data + .bss` (Δ 0); 12,584 B including vector table (Δ 0) | Build-derived. Added a static digits/colon glyph table and fixed `12:34` rendered through logical coordinates after geometry proofs. Framebuffer size and static SRAM are unchanged. Mapping, transport, PIO ownership, and raw chase are unchanged; default ELFs remain without WS2812/PIO symbols. Font/text behavior awaits physical verification. |
| 2026-10-11 | Retire bring-up runtime; display presentation-state clock | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B / 11,632 B (unchanged) | `pico2-dev-ws2812`: 48,152 B flash (Δ −256); 12,308 B `.data + .bss` (Δ −4); 12,580 B including vector table (Δ −4) | Build-derived. Removed the demo scheduler and replaced fixed text with an `HH:MM` renderer consuming the shared snapshot and existing civil-time conversion. The panel object remains framebuffer-owning; no second framebuffer was added. GP8, PIO0 SM0/four instruction words, eight-word service bound, 1 ms latch, no DMA/interrupt remain unchanged. Default build sizes are unchanged; ELF symbol scans show no WS2812/font/PIO references in defaults. User-reported prior geometry/font physical verification is preserved; new clock-mode runtime awaits physical verification. |
| 2026-10-11 | Global WS2812 brightness and development console command | `pico2`: 44,888 B flash / 11,312 B RAM (unchanged); `pico2-dev`: 46,520 B flash / 11,632 B RAM (unchanged) | `pico2-dev-ws2812`: 48,560 B flash (Δ +408); 12,320 B RAM (Δ +12); 12,592 B including vector table (Δ +12) | Build-derived using PlatformIO summaries and ELF sections/symbols. The display composition is 796 B, logical framebuffer remains 768 B, PIO image remains 8 B. Brightness state adds a 1-byte setting and 4-byte generation counter, plus driver tracking state. GP8, PIO0 SM0/four instruction words, no DMA/interrupt, eight-word service bound and 1 ms latch remain unchanged. Default ELFs contain no linked WS2812/font/brightness/PIO symbols. No hardware measurement was performed. |
| 2026-10-11 | First full-height 6x8 clock-font candidate | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B flash / 11,632 B `.data + .bss` (unchanged) | `pico2-dev-ws2812`: 48,572 B flash (Δ +12); 12,320 B `.data + .bss` (Δ 0); 12,592 B including vector table (Δ 0) | Build-derived. Font table and logical glyph renderer add 12 B to selected-profile flash; no static SRAM delta and no second framebuffer. Default ELF symbol scans show no WS2812 driver/PIO or either numeric font linked. GP8, PIO0 SM0/four instruction words, no DMA/interrupt, transport and brightness pipeline remain unchanged. 5x7 remains physically verified reference; 6x8 awaits physical verification. |
| 2026-10-11 | Compact variable-width font and development logical clock color | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B flash / 11,632 B `.data + .bss` (unchanged) | `pico2-dev-ws2812`: 49,296 B flash (Δ +724); 12,336 B `.data + .bss` (Δ +16); 12,608 B including vector table (Δ +16) | Build-derived. Added compact font/table/layout code and development RGB parsing/control. The selected `display` object is 800 B, framebuffer stays 768 B, PIO image stays 8 B; static SRAM includes color state and refresh tracking. GP8/PIO0 SM0/no DMA/no interrupt, transport and brightness output stage are unchanged. Default ELFs contain no WS2812, PIO, font, or clock-color symbols. Prior fonts preserved; compact font awaits physical evaluation. |
| 2026-10-11 | Soft compact clock-font candidate and unpadded 12-hour rendering | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B flash / 11,632 B `.data + .bss` (unchanged) | `pico2-dev-ws2812`: 49,352 B flash (Δ +56); 12,336 B `.data + .bss` (Δ 0); 12,608 B including vector table (Δ 0) | Build-derived from PlatformIO summaries; static SRAM includes the unchanged 272-byte vector table in total. The new glyph table and renderer are selected only by WS2812 rendering. Default ELF symbol scans found no clock renderer, numeric clock font, WS2812 driver, or color-control symbols. Existing framebuffer, GP8/PIO0 SM0, four PIO instruction words, transport, color, and brightness path are unchanged. Earlier fonts remain available. New candidate awaits physical evaluation. |
| 2026-10-11 | Conventional 4x6 compact WS2812 clock font | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (unchanged); `pico2-dev`: 46,520 B flash / 11,632 B `.data + .bss` (unchanged) | `pico2-dev-ws2812`: 49,336 B flash (Δ −16); 12,336 B `.data + .bss` (Δ 0); 12,608 B including vector table (Δ 0) | Build-derived from PlatformIO summaries. Replaced the active soft 5x7 candidate with six-row glyphs (2–4 columns wide) drawn at y=1..6; row 7 remains unused. No second framebuffer or SRAM increase. Prior font headers/tests remain available. Default profile totals are unchanged; font is awaiting user physical evaluation. |
| 2026-10-12 | Approved variable-width 5x7 glyphs and fixed 12-hour physical face | `pico2`: 44,888 B flash / 11,312 B `.data + .bss` (Δ 0); static SRAM including vector table 11,584 B (Δ 0) | `pico2-dev`: 46,520 B flash / 11,632 B `.data + .bss` (Δ 0); static SRAM including vector table 11,904 B (Δ 0) | Build-derived from requested profiles. `pico2-dev-ws2812`: 49,336 B flash / 12,336 B `.data + .bss` / 12,608 B including vector table (Δ 0 from previous WS2812 build). No buffer, GPIO, PIO, DMA, interrupt, or SRAM allocation changed; the glyph table replacement and 12-hour formatter are within the same linked profile footprint. All requested host tests passed. Exact glyph appearance on the panel remains unverified. |
