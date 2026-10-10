# AAC-CE Engineering Baseline

**Purpose:** establish the engineering baseline for the Consumer Edition from the
repository as it exists, while recording the design context supplied for CE.
This is documentation only. It does not assert that the proposed CE hardware or
features have been implemented or physically verified.

## Status meanings

Every item below is explicitly classified as one of:

- **Implemented in inherited AAC firmware** — present in the current inherited
  firmware source; code presence does not establish physical operation.
- **Implemented specifically for CE** — CE-specific code exists in this tree.
- **Selected design / approved requirement** — stated as the intended CE design
  in the task context, but not necessarily implemented.
- **Proposed but not finalized** — a candidate or direction without a final
  decision.
- **Unverified / requires hardware testing** — evidence is insufficient to claim
  physical behavior; this may accompany a selected design or inherited code.

The CE source has compile-time production and development profiles and separate
compile-time GPS-input and display-backend selection. Both profiles use the
inherited AAC timebase and current PD-2200 backend. Production selects physical
GPS UART/PPS; development selects the deterministic simulated receiver. The
WS2812 backend and additional display backends are not implemented.

## Repository evidence: current implementation

| Item | Classification | Evidence / limits |
| --- | --- | --- |
| Pico 2 / RP2350 build target | **Implemented in inherited AAC firmware** | `platformio.ini` selects `rpipico2`; README documents the pinned PlatformIO integration. This establishes the configured target, not a specific assembled CE unit. |
| GPS UART0 and separate PPS input | **Implemented in inherited AAC firmware** | `pins.hpp`, `hardware.cpp`, and `main.cpp` configure UART0 (`Serial1`) on GP0/GP1 and rising-edge PPS on GP2. The timing code associates RMC UTC labels with PPS captures. |
| GPS/PPS qualification and UTC state | **Implemented in inherited AAC firmware** | `clock_state.*` tracks GPS validity, PPS presence/lock, and UTC validity. It requires two consecutive coherent RMC labels and a subsequent qualifying edge to commit. The detailed windows and failure behavior are in [pps-timebase.md](pps-timebase.md). |
| Inherited timebase loss behavior | **Implemented in inherited AAC firmware** | On loss of qualification UTC becomes invalid; source inspection shows no oscillator extrapolation or prolonged holdover. A briefly fresh PPS after the last RMC is not holdover. |
| Current display backend | **Implemented in inherited AAC firmware** | `pd2200.*` and `clock_display.*` implement a Posiflex PD-2200 serial VFD through UART1/GP4. This is not the proposed CE LED panel. README documents bench-verified VFD wiring/characters for the reported test setup; it does not verify CE display hardware. |
| AAC-Bridge interface | **Implemented in inherited AAC firmware** | It was present before Stage 1 as SPI1 publisher/peripheral transport and TIME_SYNC. Stage 1 removes this CE support and its active source dependencies. |
| Physical timezone button | **Implemented in inherited AAC firmware** | It was present before Stage 1 on GP6 and selected UTC/four U.S. zones. Stage 1 removes the button subsystem for Dad's selected build; timezone/civil-time conversion remains available in presentation code. |
| Watchdog and USB diagnostics | **Implemented in inherited AAC firmware** | `main.cpp` enables the RP2350 watchdog, queues bounded USB diagnostics, and feeds the watchdog after recurring services. `watchdog_policy.hpp` sets a 4 s timeout and watchdog-boot heartbeat behavior. Software implementation is not proof of a completed physical watchdog test. |
| Compile-time production/development profiles and GPS source selection | **Implemented specifically for CE** | `pico2` and `pico2-dev` use the same RP2350 target, firmware architecture, dependencies, and selected PD-2200 backend. Production compiles the real UART/PPS source; development compiles the simulated source and USB console. Stage 2C.2 receiver fault injection is development-only; physical Pico 2 verification remains pending. |
| CE display, graphics, personalization, and enclosure direction | **Selected design / approved requirement** | The display boundary and PD-2200 backend are implemented in Stage 1. Compile-time profile selection and typed configuration defaults are implemented in Stage 2A. No LED framebuffer/backend, font/animation/theme system, or prototype light-well/enclosure implementation is included. |

## CE engineering decisions and requirements

These are design context for the baseline, not claims about current code. The
Consumer Edition requirements document was updated during Stage 1 to record the
approved direction and resolve decisions that superseded its earlier language.
The current source still determines what is implemented.

| Item | Classification | Baseline |
| --- | --- | --- |
| Pico 2 / RP2350 for Dad's clock | **Selected design / approved requirement** | Selected CE hardware target. The existing repository target agrees. |
| Retain original AAC GPS/PPS timing architecture | **Selected design / approved requirement** | Retain GPS UTC label + separate PPS qualification/association + authoritative UTC, with presentation downstream. Current inherited implementation is the starting point. |
| NEO-7 GPS on UART0 plus separate PPS | **Selected design / approved requirement** | GP0/GP1 UART0 and GP2 PPS match current inherited assignments. The exact receiver configuration and PPS-to-RMC absolute relationship remain subject to receiver-specific confirmation. |
| Standalone clock; no Wi-Fi or AAC-Bridge | **Selected design / approved requirement** | Dad's selected build has no Wi-Fi, AAC-Bridge, or physical UI button. Stage 1 removes active Bridge and button support from CE. |
| Hardware-independent compile-time display backend | **Implemented specifically for CE** | A coherent presentation snapshot is passed to a statically selected backend. The shared contract does not require pixels, RGB, or animation. The inherited PD-2200 backend is selected in the current build. |
| WS2812B 8×32 RGB panel, 256 pixels per clock | **Selected design / approved requirement** | One panel is selected for Dad's build. No panel driver or physical panel is present. |
| RP2350 PIO drives WS2812B data | **Selected design / approved requirement** | Intended output mechanism. No PIO LED implementation is present. |
| 74HCT541 5 V level shifter and approximately 330 Ω series resistor | **Selected design / approved requirement** | Electrical interface direction. Values, placement, supply/ground details, signal integrity, and actual panel compatibility require schematic review and bench testing before hardware claims. |
| GP8 proposed for WS2812B data | **Proposed but not finalized** | Not implemented and not physically tested. Stage 1 removes its inherited SPI assignment. This does not constitute WS2812 support or validate the pin choice. |
| Logical framebuffer independent of physical LED ordering | **Selected design / approved requirement** | Application drawing coordinates should not encode panel-specific serpentine/order mapping. Physical mapping belongs in a replaceable backend/configuration layer. No such framebuffer exists yet. |
| Development input for display testing | **Implemented specifically for CE** | `pico2-dev` supplies deterministic synthetic NMEA/PPS through the same parsers and timebase before connecting real GPS. Display animation and theme demonstrations remain future Stage 2C work. |
| Fonts, animations, overlays, calendar, special-day themes | **Selected design / approved requirement** | Intended presentation capabilities. Exact content, schedule, rendering resource limits, and precedence are not specified by the repository and remain to be designed. |
| Compile-time personalization and development source | **Selected design / approved requirement** | Typed compile-time profile settings and real/simulated input selection are implemented. Production personalization uses compile-time configuration. The development profile supports deterministic simulated GPS/PPS through the shared timing engine. Stage 2C.1 console controls and Stage 2C.2 receiver fault injection are implemented; acquisition/recovery demonstrations and calendar/theme scenarios remain future work. |
| GPS lock, unsynced, and holdover states | **Selected design / approved requirement** | After initial qualified GPS/PPS lock, local crystal-derived UTC may be estimated in HOLDOVER for at most 24 hours from the last qualified reference. Requalify through normal rules before LOCKED; expire to UNSYNCED at the limit. This is not implemented in inherited firmware, which invalidates UTC on reference loss. Accuracy is uncharacterized and is not GPS-verified. |
| Conservative brightness and current limiting | **Selected design / approved requirement** | Display must constrain brightness/current for a household appliance. No CE brightness control, power budget, or current limiter is implemented or measured. The provisional system target is 5 V / 2 A and requires measured verification. |
| Prototype light-well grid, diffuser, enclosure | **Proposed but not finalized** | Mechanical/optical development direction. Pitch, depth, divider geometry, diffuser, materials, ventilation, and enclosure dimensions are not specified or physically evaluated in this repository. |
| Optional three WS2812 status pixels | **Proposed but not finalized** | Up to three optional pixels may be chained after the matrix. Their use and meanings are not fixed. |
| Timing-quality status pixel | **Selected design / approved requirement** | One of the optional three chained pixels is designated to distinguish LOCKED, HOLDOVER, and UNSYNCED. Colors/effects are open; no LED code exists and no additional Pico GPIO is needed. Effects must not imply live PPS during HOLDOVER. |
| Animation style | **Proposed but not finalized** | Fades, dissolves, decays, color transitions, and rolling effects are candidates for early testing; no style is selected. |
| USB serial diagnostics | **Selected design / approved requirement** | Preserve a service/debug path for GPS/time quality, watchdog and fault diagnosis. Inherited USB diagnostics exist; CE message set/rate and field accessibility are not finalized. |
| Watchdog | **Selected design / approved requirement** | CE requires recovery from firmware stalls. Inherited RP2350 watchdog exists; the CE service boundary and physical recovery test must be confirmed on the target build. |

## Compile-time profiles and GPS/PPS input sources

**Implemented specifically for CE:** `platformio.ini` defines `pico2`
(production) and `pico2-dev` (development), both targeting the same Pico 2 /
RP2350 board, Arduino-Pico framework, and dependency set. The selected display
backend is a separate build definition from the profile; both current profiles
select the inherited PD-2200 implementation.

`include/ce_config.hpp` centralizes strongly typed compile-time settings for
the selected profile and backend, GPS baud, civil timezone and DST rule,
12/24-hour preference, calendar date lists, appearance/theme defaults, and
future brightness/current-limit options. It provides
separate production and development settings, selected by the build profile.
Current behavior remains UTC and 24-hour presentation. Birthday and holiday
lists are empty until user dates are supplied. Brightness/current limits and
production simulated-input settings are absent; no user data or synthetic
source is included in production.

The development profile compiles without a GPS module attached and uses a
deterministic simulated source. It starts after display initialization, with
its first nominal PPS one second later. The first RMC label is **2026-01-01
00:00:00 UTC** on that first pulse. PPS repeats nominally every 1,000,000 µs;
the RMC sentence starts 200,000 µs after its associated PPS and is delivered at
the configured 9600-baud, 8N1 byte cadence. A valid GGA sentence follows after
the RMC wire time plus a 50 ms gap. The seed, cadence, RMC phase, and maximum
event lateness are development compile-time settings. No host clock is read.
The first label does not bypass qualification: initial cadence and consecutive
labels must pass the normal engine before valid UTC is published.

**Implemented specifically for CE:** `gps_input_source.hpp` selects one source
at compile time. Production initializes GPS UART0 and registers the physical
PPS interrupt; it does not include the simulated source implementation or its
synthetic time settings. Development does not initialize GPS UART0 or register
the physical PPS interrupt. It emits `GPS_SOURCE=SIMULATED`; production emits
`GPS_SOURCE=REAL`. This source identity is separate from the existing timing
quality transitions and flags.

Both inputs reach the same RMC/GGA parsers and `Timebase::receive`/
`Timebase::poll` path. Real RMC reception timestamps are captured as UART bytes
are read; simulated `$` and completion timestamps follow the scheduled serial
byte times. The physical source preserves the startup UART drain safeguard.
The simulator begins after display startup, so its events have known timing
and do not pass through the physical startup discard. A simulated acquisition
is qualification against synthetic input, not a physically verified GPS fix.

Simulation uses `micros()`-derived monotonic scheduling. If a PPS or serial
event is over 20 ms late, the simulator drops pending data, marks an input
discontinuity, and does not replay an edge with a backdated timestamp. The
shared qualification engine remains responsible for accepting or rejecting
subsequent association and cadence. Stage 2C.1 adds a development-only USB
monitor/console described below. Stage 2C.2 fault controls originate in the
simulated receiver and are detailed in the [Consumer Edition requirements](Consumer-Edition-Requirements).
Acquisition/recovery demonstrations and calendar/theme scenarios remain future
work.

## Development USB monitor and console

**Implemented specifically for CE:** `pico2-dev` starts in Monitor Mode, where
asynchronous USB diagnostic records stream. Press Enter to switch to Console
Mode. The console prints a line boundary and `aac>` prompt, echoes input, and
supports backspace/delete, CR, LF, and CRLF. It has a fixed 79-character
command limit and rejects overlong lines. Commands are `help`, `status`,
`time YYYY-MM-DD HH:MM:SS`, `reset`, and `exit`. The `time` value is UTC and
supports years 2000–2099, matching the RMC parser's two-digit-year mapping.
`reset` uses the configured development seed (2026-01-01 00:00:00 UTC); both
reset and `time` restart simulated PPS/RMC/GGA input and require ordinary
timebase reacquisition.

In Console Mode, diagnostics continue to be generated and retained in a
bounded 1024-byte queue, but are not written asynchronously. Command responses
have a separate bounded 512-byte queue and take output priority. Entering or
leaving the console discards queued diagnostic bytes rather than mixing them
with command entry or replaying a backlog. Dropped diagnostic and response
bytes are reported by `status`. Input and output service are nonblocking and
bounded per main-loop iteration. The console source is excluded from the
production `pico2` build; production retains its existing bounded diagnostic
queue and has no development commands. Host tests cover console behavior and
simulator reseeding. Stage 2C.1 physical Pico 2 console verification is complete
per current project status; Stage 2C.2 fault verification remains pending.

Stage 2C.2 adds receiver-originated GPS-message, PPS-output, RMC-checksum, and
RMC-timing fault controls to `pico2-dev`; they do not directly modify the
timebase. Supported commands and exact scheduled offsets are recorded in the
[Consumer Edition requirements](Consumer-Edition-Requirements). Host fault
tests pass; physical Pico 2 verification remains pending.

## Display backend and PD-2200 boundaries

**Implemented specifically for CE:** `presentation::State` is a value snapshot
containing clock quality/time, pulse phase data, and selected civil zone. The
main loop derives it from the clock and passes it to a statically selected
backend. Backend selection is a build-time macro in the current PlatformIO
environment; there is no runtime plugin registry. A future backend will provide
its own selected implementation and keep pixel ordering, character mapping,
rendering/effects, and physical transport behind that boundary. The shared
contract does not demand pixels, RGB, or animation. Only the inherited PD-2200
backend is implemented and selectable now.

The architecture is intended to accommodate the selected WS2812 RGB matrix,
MAX7219 monochrome matrices, MAX7219 seven-segment modules, SPI TFT LCDs,
character VFDs (including the current PD-2200), and later technologies. These
are backend targets, not drivers supplied by Stage 1.

**Approved future requirement — multi-display composition:** configurations
may assign different parts of one presentation to multiple heterogeneous
physical displays. For example, a robotic arm could indicate hours, an LCD
minutes, an LED matrix seconds, a VFD the date or other information, and
independent indicators clock quality. These are examples only; they are not
hardware selected for Dad's build. The authoritative GPS/PPS timebase remains
independent of all displays. Multiple backends may consume the same coherent,
read-only presentation state and receive responsibilities such as hours,
minutes, seconds, date, or status. Their update rates, refresh policies,
response times, and capabilities may differ. Slow or electromechanical
mechanisms must not block timekeeping or unrelated display work; a future
backend may operate asynchronously when needed. The presentation model must
not require every backend to support a framebuffer, pixels, RGB, characters,
or instantaneous updates. Each backend owns its physical behavior and
transport, and composition must not create another time authority.

This is a future architectural requirement, not Stage 1 functionality. Stage 1
supports one compile-time-selected backend through `display_backend::Selected`;
multiple concurrent backends, routing, scheduling, and asynchronous operations
are not implemented. Dad's selected display remains one WS2812B 8×32 RGB panel.

To add a backend later, implement a concrete type with `begin(const
presentation::State&)` and `service(const presentation::State&)`;
keep its mapping/rendering/effect/transport details in that implementation, add
one compile-time selector branch and build definition, and preserve the
presentation snapshot as read-only input. Do not add requirements to the shared
contract unless all supported backend classes can use them meaningfully.

For the inherited PD-2200 path, keep these layers distinct:

1. **Noritake serial command protocol:** command bytes such as `ESC H` direct
   writes and `0x0E` clears characters. In the verified PD-2200 behavior,
   `0x0E` preserves the current cursor/write position; `0x0C` homes separately.
2. **PD-2200 display implementation:** 20-column/two-row layout, printable
   character policy, startup sequence, and the inherited hour-pair workaround
   are device-specific. The physical `00→08`/`10→18` symptom's root cause and
   workaround effectiveness remain unproven. Do not infer identical behavior
   from other Noritake-capable displays or later manuals.
3. **Physical UART/RS-232 transport:** the PD-2200 backend initializes UART1
   on GP4 at 9600 8N1; common `hardware.cpp` initializes GPS UART0 and PPS
   only. External MAX3232 wiring and display power/configuration are separate
   from command encoding. README records the verified test wiring.

Suitable protocol encoding could be extracted for another verified-compatible
VFD later, but Stage 1 does not attempt a speculative universal Noritake driver.
First define the shared command subset, device capability differences, and
transport boundary from verified target documentation/tests.

## Remaining selected-build facts and limits

- Dad's build uses one WS2812B 8×32 RGB matrix. Up to three extra WS2812 status
  pixels may be chained after it; one is designated for timing quality. Exact
  colors/effects remain open. These pixels share the serial chain and need no
  extra Pico GPIO. No status-pixel driver is implemented.
- The 74HCT541 / approximately 330 Ω data path remains a selected electrical
  direction. GP8 is proposed for data only; it is not assigned in firmware or
  physically tested. Its former SPI assignment was removed with Bridge support.
- The provisional system power target is 5 V / 2 A. Actual panel current,
  brightness limiting, temperature, level shifting, and power margin need
  measured verification. Do not treat the target as measured consumption.
- Compile-time personalization is the production direction. `pico2-dev`
  provides simulated GPS/PPS input through the shared timing qualification
  engine and the development-only USB monitor/console. Stage 2C.2 fault
  injection is implemented in the simulated receiver; animation/theme
  demonstrations and calendar scenarios remain future work.
- No physical UI button is in the selected Dad build. The current firmware
  defaults its selected display zone to UTC; the civil-time conversion library
  still supports all inherited U.S. zones.
- Holdover policy is approved at 24 hours from the last qualified reference,
  using the RP2350 local crystal-derived timing estimate. Reacquisition must
  repeat GPS/PPS qualification; expiration enters UNSYNCED. A reset or power
  interruption does not preserve holdover. Accuracy is oscillator/condition
  dependent and unmeasured. The existing AAC engine does not implement this.

## Conflicts with the older CE requirements

The [Consumer-Edition-Requirements](Consumer-Edition-Requirements) document was
updated during Stage 1. The table records earlier requirements superseded by
approved decisions and technical questions that remain open:

| Topic | Older document | Current baseline | Resolution / open point |
| --- | --- | --- | --- |
| MCU | — | RP2350/Pico 2 is the sole current target. | **Resolved:** no alternate target is supported or planned. |
| Settings | Timezone and 12/24-hour mode must persist; brightness/dimming/status may later persist. | Production personalization is compile-time; dev builds will be dedicated. | **Superseded for production personalization:** no profile infrastructure is introduced in Stage 1. User-adjustable settings are not selected. |
| Displays | Main display is simple room-readable time; secondary 2.25-inch ST7789 TFT is a candidate. | One WS2812B 8×32 RGB panel selected for Dad; VFD remains optional. | **Superseded:** TFT is not selected for Dad's build. Exact composition/status layout remains open. |
| Holdover | Earlier text required local timing after GPS/PPS loss and visible distinction. | RP2350 crystal-derived estimate for at most 24 hours from the last qualified reference; requalify before LOCKED, expire to UNSYNCED. | **Partly resolved:** this policy is documented but not implemented. Accuracy/uncertainty under temperature and supply conditions remains unmeasured; resets do not preserve the estimate. |
| Network and controls | Network is optional; requirements do not prohibit a physical button. | No Wi-Fi, AAC-Bridge, or physical UI button for Dad's build. | **Resolved:** Stage 1 removes active Bridge and button support from CE. |
| Display capabilities | Older doc prioritizes simple time and secondary status. | Development provides simulated GPS/PPS input; interactive demos, animation/theme controls, and calendar scenarios remain future work. | **Superseded in part:** scope and priority of production effects/status remain to be defined. |

## Engineering boundaries and verification

- **Implemented in inherited AAC firmware:** UTC remains canonical; GPS RMC is a
  label associated with a separate PPS edge; display-zone conversion is
  presentation-only. These software contracts are documented in the existing
  timebase and architecture documents.
- **Selected design / approved requirement:** CE presentation and hardware must
  not become a second time authority. A logical framebuffer should be insulated
  from physical LED ordering.
- **Selected design / approved requirement:** only time initially qualified
  against GPS/PPS can enter HOLDOVER. Estimated local time is never represented
  as GPS/PPS-verified; the status pixel must keep those states distinguishable.
- **Unverified / requires hardware testing:** no CE panel, PIO waveform,
  level-shifter path, resistor placement, current/thermal behavior, optical
  readability, diffuser, light-well, or enclosure has been tested or verified by
  this repository inspection.
- **Unverified / requires hardware testing:** inherited timing documentation
  records bench acquisition behavior for its receiver setup, but the exact
  NEO-7 configuration, PPS polarity/phase, and whole-second RMC association must
  be confirmed for the CE receiver before claiming timing correctness.
- **Unverified / requires hardware testing:** watchdog source and documented
  procedure establish implementation and a proposed bench check, not proof that
  a CE assembly recovers correctly.

The timing engine, GPS/PPS windows, and holdover behavior are not changed in
Stage 1. Holdover remains a requirement only. The dedicated inherited network
protocol documentation may describe historical AAC behavior; it is not an
active CE interface after this refactor.
