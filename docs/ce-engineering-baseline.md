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
development runtime now uses the approved clock-specific variable-width 5x7
glyph set, physically evaluated on the panel. Its seven glyph rows are drawn
at y=0..6; the glyph layer leaves row 7 unpainted. Dad's build uses row 7
x=0..30 for the local calendar marker and leaves (31,7) available. Effects
should decorate around the
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
| Selected consumer display | **Implemented; approved 5x7 clock glyphs physically verified** | WS2812 8x32 Panel, one 8-row by 32-column RGB matrix with 256 pixels. Selected builds own GP8 and one PIO0 state machine. Raw RGB transport, vertical serpentine topology, geometry proofs, variable-width rendering, 12-hour clock formatting, and glyph appearance were physically verified. Global brightness and development logical-color control are implemented. |
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
| NEO-7 GPS on UART0 plus separate PPS | **Selected design; receiver bench signals verified, Pico integration pending** | GP0/GP1 UART0 and GP2 PPS match current inherited assignments. Before Pico integration, the module/antenna produced PPS measured on a Rigol oscilloscope and UART TX activity whose Rigol-decoded data appeared valid. This is receiver bench evidence only; sentence mix, fix reporting, RMC/PPS timing, timing-window compatibility, and installed antenna performance still require AAC-CE integration verification. |
| Standalone clock; no Wi-Fi or AAC-Bridge | **Selected design / approved requirement** | Dad's selected build has no Wi-Fi, AAC-Bridge, or physical UI button. Stage 1 removes active Bridge and button support from CE. |
| Compile-time display framework | **Implemented specifically for CE** | `display_selection.hpp` selects STDOUT by default, or exactly the explicitly named driver set. `display_framework::Framework` passes the same read-only snapshot to each selected driver. |
| WS2812B 8×32 RGB panel, 256 pixels per clock | **Selected design / implemented clock driver** | One panel is selected. The development runtime uses the existing presentation state and approved variable-width 5x7 glyphs for `HH:MM`, at y=0..6; glyph drawing does not paint row 7. Dad's build uses row 7 x=0..30 for the local day marker, keeps (31,7) available, reserves (31,4) for input health and (31,3) for holdover. The 6x8 candidate was judged too large. Glyph appearance and physical formatting/rendering behavior have been verified. |
| RP2350 PIO drives WS2812B data | **Implemented; DMA transport physically approved on current panel** | Selected builds use PIO0 SM0 and a four-instruction program fixed at instruction offset 0. Initialization fails closed if SM0 or an unused DMA channel is unavailable. A frozen 256-word stream is sent with PIO-DREQ-paced DMA; no DMA interrupt is used. Electrical timing margins and waveform metrology remain unmeasured. |
| HCT541 level shifting and WS2812 development power | **Wired contract; physical behavior unverified** | GP8 / physical pin 11 drives the already wired SN74HCT541N and approximately 330 Ω series path to panel DIN. The panel is powered by a separate regulated 5 V supply during development, with shared ground. Waveform integrity, current, and thermal behavior remain unmeasured. |
| GP8 for WS2812B data | **Implemented assignment; physical behavior unverified** | Selected WS2812 builds initialize GP8 through PIO. Default builds leave it untouched. It connects through the external HCT541 buffer to panel DIN. |
| Raw-index framebuffer independent of physical LED ordering | **Implemented driver storage** | A fixed 256-entry RGB buffer (768 bytes) serializes raw indices in increasing order. Logical coordinates use the physically established top-left-start vertical serpentine column mapping documented below. |
| Development input for display testing | **Implemented specifically for CE** | `pico2-dev` supplies deterministic synthetic NMEA/PPS through the same parsers and timebase before display composition. The WS2812 clock consumes the resulting shared presentation snapshot. |
| Time formatting and centered layout | **Actual-width centering and unpadded 12-hour formatting implemented** | Format civil time first, measure the active font's actual rendered width, then center every draw on 32 columns using `x = (32 - rendered_width) / 2` with deterministic integer placement. 12-hour presentation omits the leading zero; 24-hour presentation retains two hour digits. The selected build is currently 24-hour. See [Consumer Edition Requirements, Section 5](Consumer-Edition-Requirements#5-12--24-hour-display). |
| Everyday clock color | **Implemented; physically selected** | Default logical color is soft white RGB(255, 240, 220) at brightness 2, selected after physical evaluation on the panel. The current light-gray grid plus diffuser is a reported optical change; exact appearance through this stack needs rechecking. Amber RGB(255, 96, 0) at brightness 6 remains a named calibration/reference alternate. Development `color <r> <g> <b>` still supports temporary 0..255 logical values. |
| Compile-time personalization and development source | **Selected design / approved requirement** | Typed compile-time profile settings and real/simulated input selection are implemented. Production personalization uses compile-time configuration. The development profile supports deterministic simulated GPS/PPS through the shared timing engine. Stage 2C.1 console controls and Stage 2C.2 receiver fault injection are implemented; acquisition/recovery demonstrations and calendar/theme scenarios remain future work. |
| GPS lock, unsynced, and holdover states | **Selected design / approved requirement** | After fully qualified GPS/PPS is lost, local time enters a continuous 24-hour qualified HOLDOVER window. Only full normal requalification clears its timer/state. At expiry enter HOLDOVER_EXPIRED, keep displaying and counting best-effort time as unqualified, and flash the holdover pixel until full requalification. This timing state machine is not implemented; inherited firmware invalidates UTC on reference loss. |
| Conservative brightness and current limiting | **Brightness implemented; limiter future** | WS2812 brightness is a software appearance control, range 0..255, default 2. The amber RGB(255, 96, 0) plus brightness 6 appearance remains a calibration anchor, not a current limit. No current budget or limiter is implemented or measured. The provisional system target is 5 V / 2 A and requires measured verification. |
| Optical calibration context | **Earlier stack physically evaluated; current stack reported changed** | The amber RGB(255,96,0) / brightness 6 appearance was previously observed on the BTF-LIGHTING 8x32 panel through a black grid and LCD-derived diffuser. The current stack is reported as a light-gray printed grid plus diffuser; matching appearance has not been reverified after this change. Do not assume optical appearance carries across assemblies. Enclosure, ventilation, thermal behavior, and electrical current remain unmeasured. |
| External WS2812 status chain | **General AAC-CE option; not selected for Dad's build** | Approximately 3–4 discrete LEDs may form a separate chain and logical device, never appended to the 256-pixel panel. This remains valid for other configurations/builds; its driver would own count, meanings/colors, data GPIO, and update cadence. |
| External status data path | **Planned option; GPIO unassigned** | Example path: Pico GPIO TBD → spare SN74HCT541N channel → separate approximately 330 Ω series resistor → external WS2812 LEDs. No Pico GPIO is assigned or reserved. |
| External status PIO | **Possible future option; unallocated** | The panel uses PIO0 SM0. An external status driver may use PIO0 SM1 if available and may share the loaded four-instruction WS2812 program. SM1 is not currently allocated or reserved. |
| Dad's local calendar strip | **Build-specific presentation requirement; implemented in WS2812 overlay** | One steady blue marker uses the local civil day: day 1→(0,7), day 15→(14,7), day 31→(30,7). x=31 is not used. Current blue is acceptable for now; exact tuning remains provisional. |
| Dad's on-panel timing-health pixel | **Build-specific semantic requirement; proof colors implemented, final timing-state mapping pending** | At (31,4), health represents the complete accepted timing path, never satellite count alone. Only fully qualified timing may show green for 5+ satellites used in the fix or amber for exactly 4 used; unhealthy, stale, acquiring, or reacquiring states are red. Satellites visible/in-view are diagnostic only. The status layer consumes authoritative health state and does not reproduce qualification logic. |
| Dad's on-panel holdover pixel | **Build-specific presentation requirement; breathing proof physically approved, real state and beacon future** | At (31,3), holdover under 24 hours uses the physically approved pure-red, brightness-only ~5 s breathing presentation with stable active codes 1..8 and an intentional OFF dwell. At 24 hours it becomes a restrained mostly-OFF red beacon, initially targeted at a 150–250 ms flash every 1.5–2.5 s; exact cadence is physically unverified and remains tunable. The real timing state machine does not exist, so production indication remains inactive. |
| Informational overlay priority | **Build-specific presentation requirement; implemented** | Calendar and status pixels are applied after proof background and clock glyph paint. Future scene/effect layers must also precede these persistent marks. Calendar occupies only row 7 x=0..30; (31,7) remains available. Other WS2812 builds may reuse this pattern; external chains remain a general AAC-CE option. |
| Time-event effects and recentering | **Resolver, ordinary dissolve, and special precedence implemented; recenter physically verified; special effects future** | One deterministic primary transition owns each civil-time event: the four major 6 AM/noon/6 PM/midnight events supersede an ordinary hourly dissolve, which supersedes an ordinary minute redraw. The implemented ordinary dissolve uses eight deterministic 35 ms pixel buckets in each direction and a 40 ms midpoint; physical verification is pending. Primary transitions settle before any after-effect. Ordinary hourly dissolve is followed by recenter only when actual centered x changes. Major transitions currently settle directly and suppress ordinary dissolve/recenter; their future choreography owns any layout change and lands directly at final center. Physically, `5:59`→`6:00` retains effectively the same origin, while noon and midnight `11:59`→`12:00` require a small shift that their special choreography must absorb. Themes modify palette/background treatment without replacing this hierarchy. See [Consumer Edition Requirements, Section 8](Consumer-Edition-Requirements#time-event-effect-hierarchy). |
| USB serial diagnostics | **Selected design / approved requirement** | Preserve a bounded service/debug path for receiver state, satellites used and optionally in view, fix quality, GPS UTC/date, PPS presence/age/cadence, RMC age and PPS association timing, timing-engine/holdover state, and UART/NMEA activity. Optional position/altitude is verbose bring-up information. These details remain serial-only and need not stream continuously; inherited USB diagnostics exist, but the final CE command/field set is not implemented. |
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
snapshot through the existing civil-time function and renders the approved
5x7 variable-width clock glyphs. The original 5x7 bring-up font, 6x8
candidate, and earlier compact candidates remain available as references.
The approved glyph appearance and rendering behavior have been physically
verified. Glyphs occupy y=0..6; the glyph layer does not paint row 7. Dad's
build uses row 7 x=0..30 for the local calendar marker and leaves (31,7)
available; input health and holdover occupy (31,4) and (31,3). The physical formatter
always uses unpadded 12-hour time and centers actual rendered width on every
draw.

The selected WS2812 driver keeps a 256-entry static RGB buffer with raw access
and a logical 32x8 helper. Physical observation established raw index 0 at
top-left, left-to-right column progression, and alternating vertical column
traversal (first column top-to-bottom). The raw chase, RGB transport, corners,
perimeter, row/column proofs, both prior font candidates, and live clock mode
are physically verified. The full-height 6x8 candidate was judged too large.
The approved clock font uses seven glyph rows at y=0..6; the primary time
renderer does not paint row 7. Dad's build uses row 7 x=0..30 for the day
marker and leaves (31,7) available. The former
fills, geometry scans,
fixed text hold, and raw chase are retired from normal runtime. Development
renders local civil `HH:MM` from
`presentation::State`, using fixed 12-hour civil formatting and the everyday
soft-white RGB `(255, 240, 220)` at brightness 2. Earlier physical evaluation
through the BTF-LIGHTING 8x32 panel's black grid and LCD-derived diffuser
selected this as the strongest everyday presentation; brightness 3
also looked good. Neutral white RGB(255, 255, 255) at brightness 3 looked clean
and pleasant and remains a useful low-intensity gray-like reference, though it
seemed brighter than desired for everyday use. Tested warmer values RGB(255,
220, 180), RGB(255, 200, 140), and RGB(255, 180, 100) looked too green or
otherwise less desirable. These judgments are specific to the optical stack
used for those observations. The current light-gray printed grid plus diffuser
has not been re-evaluated against them. The alternate amber RGB(255, 96,
0) at brightness 6 remains the physical calibration anchor: it produces the
desired amber appearance, and future gamma/transfer correction must preserve
that appearance unless deliberately re-evaluated physically. The palette and
compositor are implemented in the presentation layer: background, glyph-mask clock paint, then optional
overlay paint. Opaque black forces a pixel dark; transparent paint preserves
the prior layer. Glyph geometry remains independent of palette, compositor,
and transfer. Global software brightness is an unsigned 0..255 output control,
default 2; integer nearest
scaling `(channel * brightness + 127) / 255` produces `(6, 2, 0)` at the
default. Scaling occurs during GRB
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
from transparent/no-op. The fixed-size logical compositor implements
background, glyph-mask paint, then optional overlay paint; static development
proofs are `p norm`, `p amber`, `p invert`, and `p bg`. The normal everyday
default is soft white RGB(255, 240, 220) at brightness 2, and amber remains the
named reference alternate. See [Consumer Edition Requirements, Section 8](Consumer-Edition-Requirements#8-matrix-status-pixels-and-visual-effects)
for the palette and composition contract.

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
comparison if useful. No LUT is used. A development-only transfer
characterization selector is implemented in `display_transfer.hpp` and the
WS2812 serializer; its selection occurs after global brightness scaling.
`off` is the current identity behavior and remains the development default.
`g2` applies the conventional inverse-gamma-2 comparison
`round(sqrt(code * 255))`. `ws` applies a deliberately illustrative
continuous ease-out curve, `code + round(code * (255 - code) / 255)`, which
expands low codes and tapers to identity at full scale. It is motivated by
reported WS2812-family low-code behavior, is not derived from this BTF panel,
and is not claimed to correct it. Neither candidate is approved for
production. There is no LUT. Palette values remain logical RGB and are
independent of transfer selection; the physical BTF panel plus its installed
black grid and LCD-derived diffuser
decide the result. The logical RGB(255, 96, 0) / global brightness 6 reference
amber remains the physical appearance anchor through these comparisons.

The development console commands are `g {off|g2|ws}`, `pat {amber|r|g|b|w|c|m|off}`,
and `ramp {r|g|b|gray}`. `pat amber` displays the reference RGB at brightness
6; `pat off` restores the current clock. Each ramp fills all 32×8 pixels:
column x contains exactly `RGB(x,0,0)`, `RGB(0,x,0)`, `RGB(0,0,x)`, or
`RGB(x,x,x)` for x=0..31. Ramp serialization uses brightness 255, so these
values reach the selected transfer unchanged and are transferred once. Solid
color tests use the current brightness. Pattern rendering clears the full
framebuffer first; control changes dirty the frame immediately, while the main
clock, timing engine, and watchdog continue their normal cooperative service.
These are development patterns only, not a graphics demo or scene system.

Physical bench observations on the BTF-LIGHTING 8x32 through the installed
black grid and diffuser (user reported): `off` amber is correct and remains the
reference; off ramps appeared red/green/bright white for red/green/gray, dark
blue for blue, and showed no visible stepping. The red off ramp's final column
appeared amber. `g2` ramps were far too bright with no visible stepping; blue
hue looked more desirable, and a ramp's final column appeared bright white.
`ws` amber hue looked acceptable but too bright; red looked deep and good,
general colors looked better than off, gray was too bright, and no stepping was
visible. These are observations, not measurements or a transfer selection.

Source tracing found no channel leak or last-column special case: ramp writes
each logical x=0..31 at all eight rows, logical-to-raw mapping rejects x=32,
brightness 255 is applied once, then each channel is transferred once and
packed GRB. A red ramp therefore serializes green=blue=0 for every pixel; the
reported amber/white channel appearance cannot be explained by the source math.
The transfer mode was sampled separately for each pixel in a chunked frame,
however, so a mode command during transmission could mix transfer modes across
one frame. The driver now snapshots transfer mode at frame start, matching its
existing brightness snapshot. The previous host test sampled selected
framebuffer rows and did not verify every pixel together with serialized
words. Rendering now clears the full frame first; regression tests verify all
256 pixels, channel isolation, pattern replacement, bounds, and transfer-stage
inputs. The frame-mixing bug is fixed, but it does not explain cross-channel
amber/white observations; their physical cause remains undetermined and needs
another controlled panel comparison.

`g2` is `round(sqrt(code * 255))`, an inverse-gamma-2 mapping. For codes below
255 it raises output codes (for example, 31 maps to 89), so it brightens low
codes; it is not the `code^2` direction used to linearize encoded display
values. This is consistent with the reported excessive brightness. `ws` adds
`round(code * (255 - code) / 255)` to the input, a positive lift at every
interior code that peaks near mid-scale and tapers to zero at 255. It likewise
brightens low and mid codes, consistent with the reported brighter appearance.
It remains an illustrative curve, not a validated WS2812 correction for this
panel. No transfer mode is approved for production.

RGB channel balance/white balance is another independent physical calibration
option: maximum red, green, and blue codes need not have equal perceived
brightness. If needed, document and validate channel calibration separately
from global brightness and transfer correction. No channel calibration is
implemented.

The user has physically evaluated this exact optical stack: the BTF-LIGHTING
8x32 panel, installed black pixel grid, and LCD-derived diffuser. Logical
RGB(255, 96, 0) at global brightness 6 produces the desired amber appearance
and is the current calibration anchor. Brightness remains an appearance
control, not electrical current protection. Other observations (including
the dim/red-shifting low brightness behavior) do not supersede this selected
reference. The anchor is specific to this panel and stack.

For further characterization, use the implemented development-only output
transfer modes (`off`, `g2`, and `ws`). Compare identical clock colors through
the installed black grid and diffuser at brightness 1, 2, 4, 6, 8, 12, and
16. Include the
reference amber at brightness 6 in every comparison and preserve its perceived
appearance unless physical re-evaluation deliberately changes the anchor. Judge perceived
brightness spacing, amber hue, dark-color fidelity, banding, smoothness of
fades/gradients, local trails/afterglow, and early loss of low green/blue
channels. Include smooth fades or trails because static digits alone do not
show transition quality. Select production behavior only from physical
evidence. These comparisons do not approve a production transfer function.

The formatting and measured-width centering contract is recorded in
[Consumer Edition Requirements, Section 5](Consumer-Edition-Requirements#5-12--24-hour-display);
recenter behavior is in [Section 8](Consumer-Edition-Requirements#8-matrix-status-pixels-and-visual-effects).
The approved event hierarchy and primary/after-effect sequencing are also in
Section 8. Future transitions consume old and new logical positions and remain
entirely in presentation; they cannot delay or change authoritative time. The
approved font and recenter layout behavior are implemented and physically
verified. The minimal time-event resolver, special-event precedence, and
ordinary hourly dissolve are implemented; the dissolve awaits physical
verification. Special transitions and after-effects remain unimplemented, and
no generalized effects framework exists.

Dad's current build has a steady provisional blue local day-of-month marker at row 7,
x=day−1 (day 1→x=0 through day 31→x=30); x=31 remains available. Its logical
color is a provisional presentation constant and needs physical tuning. Pink
was rejected through the current light-gray grid and diffuser; blue is the
current candidate. The live timing/GPS/PPS/satellite-health pixel is at (31,4), at very low output
intensity: green for valid GPS and present/locked PPS with >4 satellites,
amber at exactly 4, and muted red for invalid GPS, invalid/unlocked PPS, or
fewer than 4 satellites. Missing/stale satellite data resolves red. Four is
the minimum acceptable count. This is a calm status mark, not an alarm.

The holdover pixel is at (31,3). Both prior implementations were physically
rejected. The latest RGB(255,128,160) / serializer-brightness 2..24 candidate
was insufficiently red, too bright, visibly stepped, and occasionally caused
unrelated display flicker after sustained operation. Host enumeration of its
50 ms half-cycle finds only 23 final RGB states. A changed waveform alone
cannot fix the small number of dim integer channel codes.

The subsequent fractional-intensity pass used pure red RGB(255,0,0), Q8
average targets 0.5..1.5, and temporal error diffusion between codes 0/1/2.
Physical evaluation approved the red hue and found the DMA transport promising
(no unrelated glitches observed yet), but rejected breathing: obvious stepping,
uneven apparent changes and visible temporal structure. Repeated calls could
emit different codes for the same target; only three real LED levels existed.
The elapsed-time cosine phase had no coarse buckets or call-count increments;
integer rounding and accumulator history explain the software discontinuities.
The exact panel perceptual response remains unmeasured.

The current holdover presentation is physically approved for Dad's present
WS2812 panel: pure red RGB(255,0,0), active device codes 1..8, a cosine cycle
of about 5 seconds (about 2.5 seconds up and down), and an intentional OFF
dwell around the trough. The brightness and breathing shape are acceptable;
DMA transport is also approved. The stable active mapping preserves the former
shape, `red = 1 + round(7 * active^2)`, where
`active = (envelope - 0.5)/0.5` above the 0.5 cutoff; at/below the cutoff the
pixel is OFF. This is the physical presentation baseline for this build.


Phase uses monotonic microsecond differences with unsigned timer rollover and
modulo 5,000,000, then floating division; no integer phase buckets are used.
The sampling grid advances by elapsed 10 ms slots, skipping missed samples.
Loop jitter can delay a visible frame or skip codes, but does not change the
5-second animation speed or catch up by replaying outdated steps.

Development `lvl <0-32>` clears the panel except (31,3), which uses the exact
stable red device code, bypassing clock brightness and comparison transfers.
`lvl off` restores prior clock/overlay presentation; `h on`/`h off` also leave
level proof mode. This isolates brightness comparisons without modifying live
health, calendar or timing inputs. Host characterization of ranges 1..8, 1..12,
1..16, 1..24 and 1..32 were compared during the earlier direct-range pass.
These stable checks supported selection of the approved 1..8 active range. The
`lvl` controls remain development characterization tools; they do not affect
live timing or health inputs.

The transport investigation found a concrete failure mechanism: CPU-refilled
eight-word chunks can starve the PIO FIFO during main-loop/USB delays,
creating an unintended low gap inside a frame and potentially latching partial
data. Prior framebuffer snapshots did not prevent that. No bounds or
mid-frame framebuffer mutation defect was found; the physical cause of each
observed glitch is not instrumentally proven. The driver now prepares a
fixed 1,024-byte GRB stream and feeds it using one PIO-DREQ-paced DMA channel.
The RGB framebuffer remains 768 bytes; no second logical framebuffer is added.
All brightness/transfer and final local RGB preparation happens before DMA;
there is no per-pixel serializer brightness state. Holdover's final RGB is
already in device-code units and is painted after ordinary brightness/transfer
preparation so global brightness 2 and comparison curves cannot crush or lift
it. All other pixels retain the existing appearance path. The stream refuses
writes/restarts until DMA completion plus a conservative 270 us FIFO/OSR drain
and then 1 ms reset-low interval. Compositor work is deferred until this lease
ends. There is no DMA interrupt, blocking wait, delay, or timing-engine change.
The former divider 15 at 150 MHz actually yielded 1 Mbit/s; it now derives an
8 MHz PIO clock (divider 18.75 at 150 MHz) for the intended 800 kbit/s,
retaining the 3/7 and 6/4 high/low cycle ratios. Waveform verification is pending.

No actual holdover timing state exists, so production keeps this pixel off.
`h on` can run indefinitely as a presentation proof; `h off` returns to the
absent/off live source. Health colors and thresholds are physically approved
and unchanged. `s {good|min|bad}` force their existing proof colors,
`s off` forces black, and `s live` returns to actual health input.
`day {1-31|live}` follows the accepted provisional blue calendar marker.
Proofs never change authoritative timing. These informational pixels remain
last in composition after backgrounds, glyphs, and decorative layers. Separate
external status chains remain valid generally; Dad's build uses panel pixels.
The current correction is host/build verified only, not physically accepted.

A separate chain of approximately 3–4 status LEDs remains a valid general
AAC-CE option for other builds. It is not planned for Dad's current build. An
external status driver may own its LED count, meanings/colors, data GPIO, and
update cadence independently of the main 256-pixel chain. A spare HCT541
channel and separate series resistor remain the intended example electrical
path; GPIO selection and PIO0 SM1 remain unallocated. SM1 is only preferred if
available. Separate drivers may share low-level WS2812 transport code without
coupling their presentation ownership.

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

- Dad's build uses one WS2812B 8×32 RGB matrix, with a local date strip at
  row 7 x=0..30, input-health pixel (31,4), and holdover pixel (31,3). Date and
  health rendering and the physically approved holdover visual proof are
  implemented; real holdover timing/state remains unimplemented. A separate external WS2812 chain remains a
  valid general AAC-CE option for other builds, but is not planned for Dad's
  build; its GPIO is unassigned and no external status driver is implemented.
- The main panel uses GP8 through its SN74HCT541 channel and approximately
  330 Ω data resistor. Any external status path used by another build has
  unassigned/TBD GPIO.
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
  continuous loss. Any GPS/PPS return must complete all normal qualification
  rules before it clears HOLDOVER or HOLDOVER_EXPIRED and resets that interval.
  At 24 hours enter HOLDOVER_EXPIRED: continue the best available time estimate
  as unqualified, never blank/stop the clock, and flash the red pixel until full
  requalification; flash cadence remains open. Partial GPS/PPS/satellite
  observations do not reset the interval. A reset/power interruption does not
  preserve holdover. Accuracy is unmeasured; the real state machine is not
  implemented in inherited firmware.

## Conflicts with the older CE requirements

The [Consumer-Edition-Requirements](Consumer-Edition-Requirements) document was
updated during Stage 1. The table records earlier requirements superseded by
approved decisions and technical questions that remain open:

| Topic | Older document | Current baseline | Resolution / open point |
| --- | --- | --- | --- |
| MCU | — | RP2350/Pico 2 is the sole current target. | **Resolved:** no alternate target is supported or planned. |
| Settings | Timezone and 12/24-hour mode must persist; brightness/dimming/status may later persist. | Production personalization is compile-time; dev builds will be dedicated. | **Superseded for production personalization:** no profile infrastructure is introduced in Stage 1. User-adjustable settings are not selected. |
| Displays | Main display is simple room-readable time; secondary 2.25-inch ST7789 TFT is a candidate. | One WS2812B 8×32 RGB panel selected for Dad; VFD remains optional. | **Superseded:** TFT is not selected for Dad's build. Exact composition/status layout remains open. |
| Holdover | Earlier text required local timing after GPS/PPS loss and visible distinction. | RP2350 crystal-derived estimate for 24 continuous hours after qualified timing is lost; full requalification alone clears it. Expiry keeps best-effort time visible as unqualified and flashes the holdover pixel until requalification. | **Partly resolved:** this policy is documented but not implemented. Accuracy/uncertainty under temperature and supply conditions remains unmeasured; resets do not preserve the estimate. |
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

The timing engine, GPS/PPS windows, and real holdover state machine are not
changed in Stage 1; the 24-hour timing-state requirement remains unimplemented.
The development holdover visual proof is implemented and physically approved,
while production indication stays off until a real holdover state exists. The
dedicated inherited network
protocol documentation may describe historical AAC behavior; it is not an
active CE interface after this refactor.
