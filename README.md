# Absurdly Accurate Clock

Minimal C++ firmware for Raspberry Pi Pico 2 (RP2350), using PlatformIO and
Earle Philhower's Arduino-Pico core. Open this directory in PlatformIO.

## Build

```sh
pio run -e pico2
pio run -e pico2 -t upload
pio device monitor -b 115200
```

The first build needs network access for the platform, core, and tools.
For the first USB upload, hold BOOTSEL while connecting the Pico 2. Alternatively,
copy `.pio/build/pico2/firmware.uf2` to its BOOTSEL drive. Uploading is separate
from building; no hardware is programmed by `pio run` alone.

The platform is pinned to `5d4561a05e3b212660ac6fdd3fbfb328d1988aa1` in
[maxgerhardt/platform-raspberrypi](https://github.com/maxgerhardt/platform-raspberrypi).
Its [rpipico2 board manifest](https://github.com/maxgerhardt/platform-raspberrypi/blob/5d4561a05e3b212660ac6fdd3fbfb328d1988aa1/boards/rpipico2.json)
selects RP2350, Cortex-M33, 150 MHz, 4 MiB flash and 512 KiB RAM.
This follows the [Arduino-Pico PlatformIO instructions](https://arduino-pico.readthedocs.io/en/latest/platformio.html).
Do not substitute the original Pico's `pico` board configuration. The explicit
integration URL avoids relying on ambiguous registry platform support.

## Display and timezone

The CE build currently defaults to UTC and has no physical UI button. Timezone
conversion remains presentation-only and supports the inherited U.S. zones; see
[timezone notes](docs/display-timezone.md). Dad's selected consumer hardware is
the **WS2812 8x32 Panel**: one 8-row by 32-column panel with 256 RGB pixels. Its
optional PIO driver uses GP8 when selected. The raw 0..255 chase was physically
verified and established the panel topology: raw index 0 is top-left, with
columns progressing left-to-right and alternating top-to-bottom / bottom-to-top
traversal. The compile-time logical map now follows that vertical serpentine
column order. The four-corner proof, clockwise perimeter trace, row/column
scans, and temporary 5x7 glyph rendering are physically verified. The
development WS2812 profile renders simulated-time `HH:MM` through the normal
timing and presentation path in dim amber; clock mode has been physically
verified. Its software global brightness is 0..255, defaults to 8, and scales
serialized pixels only. The development console accepts `brightness` and
`brightness <0-255>`. This appearance control is not a current limiter and
does not change the panel's 5 V supply. Optical-stack tuning remains pending;
the former panel walkthrough is retired from normal runtime. The physically
evaluated 6x8 candidate was judged too large, and the first compact candidate
was judged visually unattractive. The active development font is a new
conventional, compact 4x6 clock face with plain unslashed zero; it awaits
physical evaluation. Its six rows are drawn at y=1..6, leaving row 7 unused.
The renderer centers each formatted string by actual glyph width and
suppresses the leading zero in 12-hour mode. Typography stays visually quiet
so future effects can decorate around the readable glyph mask. The full layout
contract is recorded in the
[Consumer Edition requirements](docs/Consumer-Edition-Requirements). See the
[CE engineering baseline](docs/ce-engineering-baseline.md) for status and
resource ownership.

## Layout and architecture

- `src/main.cpp`: cooperative GPS/PPS, compile-time display composition, USB diagnostics, and watchdog servicing.
- `src/hardware.cpp` and `include/hardware.hpp`: UART and PPS initialization.
- `include/presentation_state.hpp`: device-independent clock snapshot supplied to display drivers.
- `include/display_framework.hpp` and `include/display_selection.hpp`: static composition and compile-time selection.
- `include/stdout_display.hpp` and `src/stdout_display.cpp`: bounded USB CDC clock output driver.
- `src/clock_vfd.cpp` and `include/clock_vfd.hpp`: optional PD-2200 presentation adapter.
- `src/pd2200.cpp` and `include/pd2200.hpp`: optional PD-2200 command encoder and device behavior.
- `include/pins.hpp`: single source of truth for the GPIO contract.
- `include/clock_state.hpp`: authoritative UTC timebase, initially invalid and unlocked.
- `src/display_time.cpp`: presentation-only civil time and contemporary U.S. DST.
- `lib/`: future reusable C++ components; `test/`: host regression tests.
- `Paper-Documents/`: existing PDFs and paper/project documentation. Preserve
  this directory and its contents; it is not generated output or firmware data.

GPS RMC labels are associated with PPS edges by the UTC timebase. Canonical time
stays UTC; local-time conversion belongs at the display boundary. See
[timebase contract](docs/pps-timebase.md) for association and validity behavior.

If no display macro is selected, `display_selection.hpp` defaults to STDOUT.
Explicit physical drivers do not implicitly include STDOUT. Set any desired
combination of `AAC_DISPLAY_STDOUT`, `AAC_DISPLAY_VFD`, and
`AAC_DISPLAY_WS2812`; `AAC_DISPLAY_VFD` preserves the optional PD-2200
implementation. Selection is
compile-time only. Drivers receive the same `presentation::State`, own their
rendering and hardware, and must make bounded nonblocking progress. The
authoritative UTC engine has no display-driver dependency.

Build and upload the optional development clock with
`pio run -e pico2-dev-ws2812 -t upload`. The build uses GP8 and PIO0 SM0. Its
normal runtime displays civil `HH:MM` derived from the shared presentation
snapshot and existing timezone/DST conversion. The current compile-time hour
format is 24-hour. The compact variable-width digits/colon use logical RGB
`(255, 96, 0)` and the driver's software global brightness, range 0..255,
default 8. Rounded integer scaling yields `(8, 3, 0)` at the default, and
brightness changes retransmit the current frame. In the development console,
`brightness` reports the setting and `brightness <0-255>` changes it. Brightness
scales pixel data only; it is not a current limiter and does not change the
panel's 5 V supply. Clock mode is physically verified; optical-stack tuning
remains pending. The display clears while UTC is invalid and otherwise
transmits when displayed time or logical color changes. Geometry proofs and
the old raw chase remain in host regression coverage, not the normal runtime.
Host tests do not verify electrical panel timing. The future intensity,
transfer-correction, and panel A/B design is documented in the [engineering
baseline](docs/ce-engineering-baseline.md), including the Mountain Lizard
reference; no gamma curve is implemented.

When explicitly selected, the PD-2200 operates in **Noritake serial command mode**, configured for 9600
baud, 8N1. For the verified PD-2200, command `0x0E` clears displayed characters
without resetting the current write/cursor position; `0x0C` homes separately.
This PD-2200 behavior must not be generalized to other Noritake-compatible
displays. Protocol encoding is distinct from the PD-2200-specific layout and
workarounds, and from physical UART1/RS-232 transport through the MAX3232.

Default `pico2` and `pico2-dev` builds use STDOUT and do not initialize UART1,
GP4, or GP5 for the VFD. The optional VFD driver initializes UART1 TX on GP4
(physical pin 6) only when selected; RX is disabled and GP5 remains available.
The optional WS2812 driver owns GP8 (Pico 2 physical pin 11) only when selected.
The data path uses the SN74HCT541N level shifter and approximately 330-ohm
series resistor; development powers the panel from a separate regulated 5 V
supply with shared ground. The driver uses PIO and no DMA. Its logical mapping
matches the physically observed top-left-start vertical serpentine columns;
the four-corner proof, perimeter trace, and row/column scans are physically
verified. The live `HH:MM` clock mode is physically verified. The final
verified. The 6x8 candidate was judged too large and the first compact font
was judged visually unattractive despite its useful spacing. The new
conventional 4x6 compact clock font awaits physical evaluation; final approval
remains open. Effects should decorate around the glyphs, not stylize or replace
them. Time is formatted before its actual rendered width is measured and
centered on every draw; optional subtle
recenter animation is relevant only when the target origin changes. See the
[Consumer Edition display requirements](docs/Consumer-Edition-Requirements#5-12--24-hour-display)
for 12/24-hour formatting and the [presentation behavior](docs/Consumer-Edition-Requirements#8-matrix-status-pixels-and-visual-effects)
for future effects. Effects remain presentation policy and never own or alter
authoritative time. Brightness scaling is not a current limiter.

The approximately 3–4 WS2812 status LEDs are a separate logical and electrical
device, not pixels 256 onward in the panel chain. Their future data path uses
an as-yet-unassigned Pico GPIO, a spare SN74HCT541 channel, and its own
approximately 330-ohm series resistor. A separate status driver will own their
count, meanings/colors, GPIO, and update cadence; it may share a low-level PIO
transport with the panel. PIO0 SM1 is preferred only if available when that
driver is implemented. No status GPIO or PIO state machine is currently
assigned or reserved.

## Unattended recovery

The RP2350 hardware watchdog recovers a wedged firmware main loop after **4,000 ms**
without a feed. It is enabled at the start of setup (covering startup stalls too)
and fed only after all recurring main-loop services complete, never by an ISR or
timer. Normal loop work is bounded; VFD power-on settling is serviced
asynchronously when that optional driver is selected. GPS/PPS loss, invalid UTC, display faults or
backpressure do not intentionally cause resets.

Recovery follows normal startup: GPS/UTC validity and PPS lock must be acquired
again by the existing rules. Nothing preserves time quality
across reset. The SDK's RP2350-aware `watchdog_caused_reboot()` is sampled before
enabling the watchdog. A watchdog boot queues `RESET: watchdog` on USB serial
alongside existing diagnostics (subject to the existing bounded queue/host
availability). It also queues `RESET=WATCHDOG` after every fifth ordinary diagnostic
message for the remainder of that boot, so the cause remains observable after USB
reconnects. The marker does not count itself; cadence follows diagnostic activity
(potentially several minutes when quiet), with the same queue/drop policy. Normal
boots emit no periodic reset marker. The heartbeat toggles every 250 ms for that entire session instead
of the normal 500 ms: 2 Hz versus 1 Hz full blink cycles. Reacquisition does not
clear the faster cadence; a normal power cycle/reset restores normal cadence.
This is a liveness/reset diagnostic, independent of all time and network quality.

### Physical watchdog bench test

Use an SWD debugger with the production firmware; no firmware test hook is needed.
After normal startup, halt the application core at `loop()` and leave it halted
for more than four seconds. Watchdog debug pause is disabled. Configure the
debugger not to catch/hold reset or automatically re-halt the restarted target,
then detach without issuing another reset so startup can run. Confirm a hardware
reset about four seconds after the last feed, reconnect USB serial promptly to
observe `RESET: watchdog`, and check the persistent 250 ms LED toggle interval.
With GPS/PPS withheld, confirm invalid time; restore them
and confirm normal reacquisition while the faster heartbeat persists. Finally
power-cycle and check the normal 500 ms interval. Separately run with missing
GPS/PPS and disconnected display for longer than four seconds to
check that degraded operation alone does not reset. Host tests cannot establish
the physical reset behavior or exact timeout; record those on the bench.

## Verified VFD wiring

Bench testing verified Pico 2 GP4 (physical pin 6) UART TX at 9600 baud on an
oscilloscope using continuous `0x55` (ASCII `U`). The VFD then successfully
displayed those characters through the MAX3232 with the wiring below.

The tested MAX3232 module has misleading signal-direction labels: connect
**Pico GP4 TX to the module header labeled TXD, not RXD**. GP4 remains the
project's VFD UART TX pin; the module labeling does not change the GPIO contract.

| Source | Destination |
| --- | --- |
| Pico GP4 / physical pin 6 | MAX3232 TTL header labeled TXD |
| Pico GND | MAX3232 GND |
| MAX3232 DB9 pin 2 (RS-232 transmit output) | Posiflex DB9 pin 3 (receive input) |
| MAX3232 DB9 pin 5 | Posiflex DB9 pin 5 (signal ground) |

The **DB9 pin 2 to pin 3 crossover is required** for this tested module/display
combination. The PD-2200 is powered separately and configured for Noritake mode,
9600 baud, 8N1. These labeling and wiring findings apply to the tested module.
The temporary continuous-`U` diagnostic firmware has been removed; normal
firmware sends the clock/status fields described above.

## GPIO contract

All numbers below are GPIO numbers, not physical header positions. TX/RX and
input/output directions are relative to the Pico.

| GPIO | Assignment |
| --- | --- |
| GP0 | GPS UART0 TX (`Serial1`) |
| GP1 | GPS UART0 RX (`Serial1`) |
| GP2 | GPS PPS input |
| GP4 | PD-2200 UART1 TX (`Serial2`) via MAX3232, only when VFD is selected |
| GP5 | Unassigned and available |
| GP6–GP7 | Unassigned |
| GP8 | WS2812 data output, allocated only when that driver is selected |
| GP9–GP22 | Unassigned |

GP3 is also unassigned. UART0 GPS and UART1 display currently use **9600 baud,
8N1**, explicit bring-up assumptions in
`src/main.cpp`; confirm them against the GPS configuration and VFD switches.
USB `Serial` is separate from both hardware UARTs. No Bridge, Wi-Fi, or
physical UI button is used in the selected build.

## License

This project is licensed under the [MIT License](LICENSE).
