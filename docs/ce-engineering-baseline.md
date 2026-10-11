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

The CE source has compile-time production/development profiles, GPS-input
selection, and display selection. Both profiles use the same authoritative
timebase. Production selects physical GPS/PPS; development selects simulated
GPS/PPS. Both default to the bounded USB CDC STDOUT display. The optional
PD-2200 driver can be selected explicitly. The selected consumer WS2812 8x32
Panel has an optional PIO driver; its raw topology, logical geometry proofs,
temporary 5x7 font, full-height 6x8 font, and continuous `HH:MM` mode are
physically established. The 6x8 candidate was judged too large for the desired
product and future effect space. The first compact candidate was physically
judged visually unattractive despite its useful spacing. The normal
development runtime now uses the conventional 4x6 compact clock font with
plain unslashed zero; it awaits physical evaluation. Its six glyph rows are
drawn at y=1..6, leaving row 7 unused. Effects should decorate around the
readable glyph mask rather than shape or replace the typography. A
development-only console command sets temporary logical RGB before global
brightness.

## Repository evidence: current implementation

| Item | Classification | Evidence / limits |
| --- | --- | --- |
| Pico 2 / RP2350 build target | **Implemented in inherited AAC firmware** | `platformio.ini` selects `rpipico2`; README documents the pinned PlatformIO integration. This establishes the configured target, not a specific assembled CE unit. |
| GPS UART0 and separate PPS input | **Implemented in inherited AAC firmware** | `pins.hpp`, `hardware.cpp`, and `main.cpp` configure UART0 (`Serial1`) on GP0/GP1 and rising-edge PPS on GP2. The timing code associates RMC UTC labels with PPS captures. |
| GPS/PPS qualification and UTC state | **Implemented in inherited AAC firmware** | `clock_state.*` tracks GPS validity, PPS presence/lock, and UTC validity. It requires two consecutive coherent RMC labels and a subsequent qualifying edge to commit. The detailed windows and failure behavior are in [pps-timebase.md](pps-timebase.md). |
| Inherited timebase loss behavior | **Implemented in inherited AAC firmware** | On loss of qualification UTC becomes invalid; source inspection shows no oscillator extrapolation or prolonged holdover. A briefly fresh PPS after the last RMC is not holdover. |
| Display framework and STDOUT | **Implemented specifically for CE** | `display_selection.hpp` defaults to STDOUT only when no driver is named; explicit selections compose only those drivers. STDOUT uses a bounded USB CDC line and is quiet during development console mode. |
| Optional PD-2200 driver | **Implemented in inherited AAC firmware; optional in CE** | Explicit VFD selection includes the presentation adapter and verified PD-2200 command driver. It uses UART1 TX on GP4 / physical pin 6 only; RX is disabled and GP5 remains available. MAX3232 labeling and DB9 wiring quirks are external hardware details. The physical display is not the selected consumer hardware. |
| Selected consumer display | **Implemented; compact font awaits physical evaluation** | WS2812 8x32 Panel, one 8-row by 32-column RGB matrix with 256 pixels. Selected builds own GP8 and one PIO0 state machine. Raw RGB transport, vertical serpentine topology, geometry proofs, both prior font candidates, and live presentation-state `HH:MM` clock mode were physically verified. The 6x8 candidate was judged too large; the compact variable-width candidate awaits physical evaluation. Global brightness and development logical-color control are implemented. |
| AAC-Bridge interface | **Implemented in inherited AAC firmware** | It was present before Stage 1 as SPI1 publisher/peripheral transport and TIME_SYNC. Stage 1 removes this CE support and its active source dependencies. |
| Physical timezone button | **Implemented in inherited AAC firmware** | It was present before Stage 1 on GP6 and selected UTC/four U.S. zones. Stage 1 removes the button subsystem for Dad's selected build; timezone/civil-time conversion remains available in presentation code. |
| Watchdog and USB diagnostics | **Implemented in inherited AAC firmware** | `main.cpp` enables the RP2350 watchdog, queues bounded USB diagnostics, and feeds the watchdog after recurring services. `watchdog_policy.hpp` sets a 4 s timeout and watchdog-boot heartbeat behavior. Software implementation is not proof of a completed physical watchdog test. |
| Compile-time production/development profiles and GPS source selection | **Implemented specifically for CE** | Production compiles real UART/PPS; development compiles simulated input, console, and fault injection. Both compile the same display-selection mechanism, defaulting to STDOUT. |
| Display framework | **Implemented specifically for CE** | Static compile-time composition passes one coherent snapshot to each selected driver. No runtime plugin registry is present. WS2812 is independently selectable and composes with STDOUT and/or VFD. |

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
| Compile-time display framework | **Implemented specifically for CE** | `display_selection.hpp` selects STDOUT by default, or exactly the explicitly named driver set. `display_framework::Framework` passes the same read-only snapshot to each selected driver. |
| WS2812B 8×32 RGB panel, 256 pixels per clock | **Selected design / implemented clock driver** | One panel is selected. The development runtime uses the existing presentation state and new conventional 4x6 compact clock font for `HH:MM`. Clock mode and prior fonts were physically evaluated; the 6x8 candidate was judged too large, and the first compact candidate visually unattractive. The active font awaits physical evaluation; its six rows use y=1..6, leaving row 7 unused. |
| RP2350 PIO drives WS2812B data | **Implemented; physical waveform unverified** | Selected builds use PIO0 SM0 and a four-instruction program fixed at instruction offset 0. Initialization fails closed if SM0 is already claimed. No DMA is used. |
| HCT541 level shifting and WS2812 development power | **Wired contract; physical behavior unverified** | GP8 / physical pin 11 drives the already wired SN74HCT541N and approximately 330 Ω series path to panel DIN. The panel is powered by a separate regulated 5 V supply during development, with shared ground. Waveform integrity, current, and thermal behavior remain unmeasured. |
| GP8 for WS2812B data | **Implemented assignment; physical behavior unverified** | Selected WS2812 builds initialize GP8 through PIO. Default builds leave it untouched. It connects through the external HCT541 buffer to panel DIN. |
| Raw-index framebuffer independent of physical LED ordering | **Implemented driver storage** | A fixed 256-entry RGB buffer (768 bytes) serializes raw indices in increasing order. Logical coordinates use the physically established top-left-start vertical serpentine column mapping documented below. |
| Development input for display testing | **Implemented specifically for CE** | `pico2-dev` supplies deterministic synthetic NMEA/PPS through the same parsers and timebase before display composition. The WS2812 clock consumes the resulting shared presentation snapshot. |
| Clock fonts, animations, overlays, calendar, special-day themes | **Compact conventional candidate implemented; physical approval pending** | The physically verified 5x7 bring-up font and full-height 6x8 candidate remain available as references, as do earlier compact candidates. The 6x8 candidate was judged too large and the first compact candidate visually unattractive. The active development clock uses a conventional 4x6 font with plain unslashed zero; glyphs render on y=1..6 and leave row 7 unused. Effects should decorate around the glyph mask without shaping or replacing the numerals. Physical font approval remains open. |
| Time formatting and centered layout | **Actual-width centering and unpadded 12-hour formatting implemented** | Format civil time first, measure the active font's actual rendered width, then center every draw on 32 columns using `x = (32 - rendered_width) / 2` with deterministic integer placement. 12-hour presentation omits the leading zero; 24-hour presentation retains two hour digits. The selected build is currently 24-hour. See [Consumer Edition Requirements, Section 5](Consumer-Edition-Requirements#5-12--24-hour-display). |
| Development clock color | **Implemented for WS2812 development** | Console `color` reports the current logical RGB; `color <r> <g> <b>` sets non-persistent 0..255 channels. Default is `(255, 96, 0)`. A color-generation change redraws/retransmits the current text before the unchanged global-brightness output stage. No palette, theme, gamma, or current-limit behavior is included. |
| Compile-time personalization and development source | **Selected design / approved requirement** | Typed compile-time profile settings and real/simulated input selection are implemented. Production personalization uses compile-time configuration. The development profile supports deterministic simulated GPS/PPS through the shared timing engine. Stage 2C.1 console controls and Stage 2C.2 receiver fault injection are implemented; acquisition/recovery demonstrations and calendar/theme scenarios remain future work. |
| GPS lock, unsynced, and holdover states | **Selected design / approved requirement** | After initial qualified GPS/PPS lock, local crystal-derived UTC may be estimated in HOLDOVER for at most 24 hours from the last qualified reference. Requalify through normal rules before LOCKED; expire to UNSYNCED at the limit. This is not implemented in inherited firmware, which invalidates UTC on reference loss. Accuracy is uncharacterized and is not GPS-verified. |
| Conservative brightness and current limiting | **Brightness implemented; limiter future** | WS2812 development brightness is a software appearance control, range 0..255, default 8. It is not a current limiter and is not physically tuned. No current budget or limiter is implemented or measured. The provisional system target is 5 V / 2 A and requires measured verification. |
| Black pixel grid and diffuser | **Installed and physically evaluated** | User reports distinct square pixels and substantially more useful apparent brightness than the bare panel. Development brightness around 2..16 is useful; 1 is extremely dim and shifts amber toward red, while 2 remains usable but very dim. Exact production setting, grid/diffuser dimensions, enclosure, ventilation, thermal behavior, and final optical calibration remain open. |
| Separate WS2812 status chain | **Approved architecture; driver not implemented** | Approximately 3–4 discrete LEDs form a separate electrical chain and logical device, never appended to the 256-pixel main panel. The future status driver will own their count, meanings/colors, data GPIO, and update cadence. |
| Status data path | **Planned hardware direction; GPIO unassigned** | Pico GPIO TBD → spare SN74HCT541N channel → separate approximately 330 Ω series resistor → approximately 3–4 WS2812 LEDs. No Pico GPIO is assigned or reserved before driver implementation. |
| Status PIO | **Preferred future option; unallocated** | The panel uses PIO0 SM0. A status driver may use PIO0 SM1 if available and may share the loaded four-instruction WS2812 program. SM1 is not currently allocated or reserved. |
| Timing-quality status indication | **Selected design / approved requirement** | A status indicator is intended to distinguish LOCKED, HOLDOVER, and UNSYNCED. Colors/effects remain open; no status LED code exists. It must not imply live PPS during HOLDOVER. |
| Animation style | **Recenter behavior approved; general style not finalized** | A subtle presentation-only recenter animation may run when a new measured layout has a different target origin; no movement is needed when the target origin is unchanged. It cannot delay or affect the new displayed time or authoritative UTC. Other fades, dissolves, decays, color transitions, and rolling effects remain candidates without a selected style. See [Consumer Edition Requirements, Section 8](Consumer-Edition-Requirements#8-matrix-status-pixels-and-visual-effects). |
| USB serial diagnostics | **Selected design / approved requirement** | Preserve a service/debug path for GPS/time quality, watchdog and fault diagnosis. Inherited USB diagnostics exist; CE message set/rate and field accessibility are not finalized. |
| Watchdog | **Selected design / approved requirement** | CE requires recovery from firmware stalls. Inherited RP2350 watchdog exists; the CE service boundary and physical recovery test must be confirmed on the target build. |

## Compile-time profiles and GPS/PPS input sources

**Implemented specifically for CE:** `platformio.ini` defines `pico2`
(production) and `pico2-dev` (development), both targeting the same Pico 2 /
RP2350 board, Arduino-Pico framework, and dependency set. Display selection is independent of profile selection. With no `AAC_DISPLAY_*`
definition, both profiles select STDOUT. Explicit VFD selection does not add
STDOUT; `AAC_DISPLAY_STDOUT` may be combined with one or more physical drivers.

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

## Display framework and driver boundaries

**Implemented specifically for CE:** `presentation::State` is a value snapshot
containing clock quality/time, pulse phase data, and selected civil zone. The
main loop derives it from clock state and passes the same const snapshot to a
static compile-time composition. The generic framework has no device protocol
or hardware behavior. STDOUT, the optional PD-2200 driver, and the optional
WS2812 PIO driver are implemented. The WS2812 development runtime converts the
snapshot through the existing civil-time function and renders the conventional
4x6 compact clock font. The original 5x7 bring-up font, 6x8 candidate, first
compact candidate, and soft compact candidate remain available as references.
The active font awaits physical evaluation. Glyphs occupy y=1..6; row 7 is
intentionally unused by basic time rendering. The formatter centers actual
rendered width on every draw and omits the leading zero in 12-hour mode.

The selected WS2812 driver keeps a 256-entry static RGB buffer with raw access
and a logical 32x8 helper. Physical observation established raw index 0 at
top-left, left-to-right column progression, and alternating vertical column
traversal (first column top-to-bottom). The raw chase, RGB transport, corners,
perimeter, row/column proofs, both prior font candidates, and live clock mode
are physically verified. The full-height 6x8 candidate was judged too large.
The active compact font uses six glyph rows at y=1..6; row 7 is unused by the
primary time renderer and remains available for future overlays. The former
fills, geometry scans,
fixed text hold, and raw chase are retired from normal runtime. Development
renders local civil `HH:MM` from
`presentation::State`, using the current 24-hour configuration and logical
amber RGB `(255, 96, 0)`. Global software brightness is an unsigned 0..255
output control, default 8; integer nearest scaling `(channel * brightness +
127) / 255` produces `(8, 3, 0)` at the default. Scaling occurs during GRB
serialization, leaves framebuffer colors intact, and does not change the
panel's 5 V supply. Development console commands `brightness` and
`brightness <0-255>` report/set it. `color` reports logical clock RGB;
`color <r> <g> <b>` sets temporary development values from 0..255 and a change
redraws/retransmits the current time. Color remains full-scale upstream of
global brightness and future output-transfer correction. Invalid UTC clears
the panel; otherwise a frame is sent when displayed `HH:MM` or logical color
changes. Transport remains
bounded to eight words per service and a nonblocking 1 ms latch interval.
Brightness is an appearance control, not a current limiter. Gamma/perceptual
correction, separate current-budget limiting, and ambient-light control remain
future work. Future effects and themes continue to render logical RGB through
the same output pipeline. The earlier observation of red coloration during
the blue fill remains recorded but was not investigated.

### Future WS2812 intensity and output-transfer architecture

Keep rendering policy independent from physical LED conversion. The intended
logical path is presentation/clock state → font and logical renderer →
transitions/effects/themes → day-of-month and other overlays → logical RGB
framebuffer → optional local per-pixel/effect intensity → global display
brightness → future perceptual/LED transfer correction → WS2812 GRB
serialization/PIO. The electrical current-budget limiter is a separate
protection function; its ordering relative to transfer correction is not
decided and must not be inferred from this conceptual path.

Local/effect intensity belongs to compositing and expresses relative
pixel-to-pixel intensity inside a frame. For example, a main digit, bright
trail, dim trail, and afterglow may use local values 255, 128, 64, and 16.
Global display brightness applies once to the finished composition and sets
overall apparent clock brightness. Neither value belongs in font glyphs or
transport behavior. Future fades, dissolves, glow, trails, sparkles, swirls,
scrolling, and special-day effects should produce logical colors and local
intensity, then share the same global output stages. Clock glyphs are masks
painted with logical RGB; black/off explicitly writes zero RGB and differs
from transparent/no-op. See [Consumer Edition Requirements, Section 8](Consumer-Edition-Requirements#8-matrix-status-pixels-and-visual-effects)
for the required compositor semantics and implementation priority.

When reducing a pixel's intended/perceived intensity, apply local intensity
and global brightness before gamma or LED-transfer correction. Do not gamma
correct a channel and then multiply the corrected code to create a fade. Keep
effect/color decisions in a consistent perceived-color space as far as
practical; treat transfer correction as a late physical-output conversion.
This correction maps desired/perceived channel intensity to the output code;
it is distinct from global brightness and from electrical current protection.

Mountain Lizard's [“Gamma correction with WS2812
LEDs”](https://mountainlizard.com/posts/gamma-ws2812/) is a useful reference.
It describes ordinary
perceptual gamma, applying brightness/intensity before correction, and a
WS2812-aware transfer curve based on reported nonlinear low-code behavior in
actual WS2812-family parts, roughly within the first few tens of output codes.
Such a curve can assign larger output codes at low perceived levels and
preserve more dark-color steps than a conventional 8-bit gamma curve. The
article also cautions that compatible clones may be more linear and may favor
conventional gamma. The exact LED silicon/version in this BTF panel is
unknown; do not assume either transfer behavior. Any curve must be understood
or generated from documented parameters/methodology, attributed, and
physically compared on this panel. Do not import a third-party LUT as
unexplained data. Keep conventional gamma available as a development
comparison if useful. No LUT or correction is implemented.

RGB channel balance/white balance is another independent physical calibration
option: maximum red, green, and blue codes need not have equal perceived
brightness. If needed, document and validate channel calibration separately
from global brightness and transfer correction. No channel calibration is
implemented.

The user's current optical-stack observation is that the black PETG-style
pixel grid and diffuser have been installed and tested and produce distinct
square pixels. Apparent brightness is substantially more useful than on the
bare panel. Global brightness around 2..16 is currently useful for
development; 1 is extremely dim and shifts the current amber toward red as
green quantizes to zero, while 2 remains usable but very dim. These are
observations, not production calibration; exact production brightness is
unselected.

When gamma work is authorized, use a development-only selectable output
transfer mode (off, conventional, and WS2812-adjusted as concepts; command
names are open). Compare identical clock colors through the installed black
grid and diffuser at brightness 1, 2, 4, 8, 12, and 16. Judge perceived
brightness spacing, amber hue, dark-color fidelity, banding, smoothness of
fades/gradients, local trails/afterglow, and early loss of low green/blue
channels. Include smooth fades or trails because static digits alone do not
show transition quality. Select production behavior only from physical
evidence. This is a future validation plan, not implemented test mode.

The formatting and measured-width centering contract is recorded in
[Consumer Edition Requirements, Section 5](Consumer-Edition-Requirements#5-12--24-hour-display);
optional recenter behavior is in [Section 8](Consumer-Edition-Requirements#8-matrix-status-pixels-and-visual-effects).
Future transitions consume old and new logical positions and remain entirely
in presentation; they cannot delay or change authoritative time. The compact
font is implemented for evaluation, while final font selection and effects
remain open. No effects framework is implemented.

The approximately 3–4 status LEDs are separate from the main 256-pixel chain.
The main panel driver owns its 32x8 geometry, framebuffer/rendering,
presentation, and panel themes/animations. A separate status driver will own
its small LED count, status meanings/colors, data GPIO, and update cadence; the
drivers do not depend on each other. They may share low-level WS2812 PIO
transport code. The spare HCT541 channel and separate series resistor are the
intended electrical path. GPIO selection and PIO0 SM1 use remain unallocated
until implementation; SM1 is only preferred if available. This separation
keeps the panel at exactly 256 pixels and prevents panel-chain faults or
updates from coupling status indication to panel geometry/rendering.

The architecture is intended to accommodate the selected WS2812 RGB matrix,
MAX7219 monochrome matrices, MAX7219 seven-segment modules, SPI TFT LCDs,
character VFDs (including the current PD-2200), and later technologies. These
are backend targets, not drivers supplied by Stage 1.

**Implemented framework capability / future routing work — multi-display
composition:** configurations
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

Static selection of multiple drivers is implemented. The current framework
services each selected driver in order once per loop and does not route
different fields to different backends. Drivers must keep each service bounded;
independent asynchronous cadence and output state belong to each driver. The
selected consumer hardware remains one WS2812 8x32 Panel; its low-level driver
and mapping for the observed topology are implemented while final clock
rendering is physically verified; brightness appearance tuning remains open.

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

- Dad's build uses one WS2812B 8×32 RGB matrix. Approximately 3–4 WS2812 status
  LEDs are a separate chain with a separate data GPIO, not appended to the
  matrix. They use a spare HCT541 channel and separate approximately 330 Ω
  resistor. No GPIO is assigned/reserved and no status driver is implemented.
- The main panel uses GP8 through its SN74HCT541 channel and approximately
  330 Ω data resistor. The separate status path's GPIO remains TBD.
  GP8 is assigned by the selected driver and remains untouched in default
  builds. The panel uses a separate regulated 5 V development supply and a
  shared ground. Signal integrity is unmeasured; physical pixel topology is
  established as top-left-start vertical serpentine columns.
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
