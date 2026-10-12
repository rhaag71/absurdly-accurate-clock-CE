# AAC-CE Engineering Instructions

These instructions extend the global DIM8 engineering standards. They record
the Consumer Edition's architecture, hardware contract, and verification
requirements. Keep implemented behavior distinct from approved future work and
physical verification.

## Target and build profiles

- The sole target is Raspberry Pi Pico 2 / RP2350 using PlatformIO and the
  Arduino-Pico framework. Do not add RP2040 compatibility work.
- `pico2` is production; `pico2-dev` is development. Both use the same
  authoritative timing engine and display abstraction.
- Build-profile selection and display-backend selection are independent
  compile-time choices. Maintain compile-time configuration; do not add runtime
  persistent settings without authorization.
- Production selects physical GPS UART/PPS and excludes simulator and
  development-console code. Development selects simulated GPS/PPS and includes
  the development-only USB console. Development controls must not affect
  production behavior.

## Authoritative timekeeping

- The authoritative clock operates exclusively in UTC. GPS RMC supplies UTC
  labels; captured GPS PPS establishes second boundaries. Receiving RMC must
  never directly advance authoritative UTC.
- `clock_model::Timebase` is the authoritative qualification engine. Preserve
  its RMC/PPS association, consecutive-label and pulse-cadence qualification,
  timeout/reacquisition behavior, and the physical receiver timing contract.
  Do not weaken qualification to accommodate simulations.
- Physical PPS capture and UART reception metadata must retain the established
  atomic sampling behavior. Preserve coherent PPS sequence/reception
  timestamps, monotonic timer servicing, and rollover handling.
- Never introduce another independent authoritative clock. Receiver-supplied
  labels, qualified authoritative UTC, and civil/presentation time are distinct
  data. Source changes and deliberate simulated receiver restarts invalidate
  qualification and require reacquisition.
- Simulated input must enter the existing NMEA parsers and normal PPS/RMC
  qualification path. Simulated UTC must never be written directly into
  `Timebase`. Simulated lock is not evidence of physical GPS accuracy.

## Civil time and presentation

- Convert UTC to civil time downstream, in presentation code. DST and 12/24-hour
  formatting are presentation responsibilities; birthdays and holidays use
  local civil dates.
- Format the selected civil time before measuring its active font's actual
  rendered width. Center every draw on the 32-column panel with
  `x = (32 - rendered_width) / 2` and deterministic integer placement. The
  physical consumer clock face is always 12-hour: omit the leading zero and
  show 12 at midnight and noon. Other representations may use 24-hour time.
  Presentation may subtly animate only a changed centering origin, using old
  and new logical positions; it must not delay or affect the current time.
  Effects consume layout positions and never own formatting or timekeeping.
- Preserve the original AAC timezone/DST implementation. Do not add a competing
  timezone conversion system.
- Presentation consumes a coherent, read-only clock snapshot. Display hardware
  and backends must never qualify, advance, or otherwise control authoritative
  timing.
- CE personalization is compile-time: timezone/DST, hour format, birthdays and
  holidays, display appearance/theme, and brightness/current limits once
  characterized. Dad's clock has no Wi-Fi, AAC-Bridge, or physical timezone
  selection button.

## Display architecture

- Compile-time display selection uses `AAC_DISPLAY_STDOUT`, `AAC_DISPLAY_VFD`,
  and `AAC_DISPLAY_WS2812`. With no explicit selection the application selects
  STDOUT. Explicit physical drivers do not add STDOUT unless its flag is also
  selected. No runtime discovery or plugin loading is used.
- Keep presentation state device-independent and read-only to display code.
  The generic display framework only composes selected drivers and passes the
  same snapshot to each; it contains no device protocol or rendering rules.
- Drivers own formatting, buffers, update cadence, hardware initialization,
  transport, and backpressure. Keep service bounded and nonblocking so display
  faults or absence cannot stop GPS/PPS timekeeping or watchdog progress.
  Multiple heterogeneous drivers may be composed statically.
- STDOUT is a separate display driver using the shared USB CDC transport. It
  is distinct from diagnostics and the development console, respects quiet
  console mode, and retains at most one replaceable output line.
- The optional PD-2200 adapter/driver preserves verified Noritake-compatible
  behavior. When selected it owns UART1 TX on GP4 (physical pin 6) through
  MAX3232; UART1 RX is disabled and GP5 remains available. MAX3232 labeling and
  DB9 wiring quirks are specific to the external hardware and do not change
  the Pico GPIO contract. No default consumer build initializes UART1 or
  assumes MAX3232 hardware.
- Dad's selected display is the **WS2812 8x32 Panel**, one 8-row by 32-column
  matrix with 256 RGB pixels. Its optional PIO driver is implemented and owns
  GP8 (Pico 2 physical pin 11) only when selected. The external SN74HCT541N
  level shifter and approximately 330-ohm series resistor feed panel DIN; the
  panel uses a separate regulated 5 V development supply and shared ground.
  Electrical behavior remains unqualified. Do not invent pixel order or power
  limits.
- Do not allocate GP8, PIO, DMA, a pixel framebuffer, or configure panel
  hardware in builds that do not select its driver.
- WS2812 logical coordinates follow the physically observed topology: raw
  index 0 is top-left; columns progress left-to-right; even columns traverse
  top-to-bottom and odd columns bottom-to-top. Do not invent measured current
  or final power limits.
- Backends own their hardware. Display initialization must not configure
  unrelated hardware, and slow display operations must not block authoritative
  timing. The backend contract must not require every display to support RGB,
  pixels, framebuffers, animation, or instantaneous updates.
- Multi-display composition is implemented as a small compile-time tuple
  framework. Each selected driver receives one shared presentation snapshot;
  scheduling remains cooperative and drivers must keep each service call
  bounded. Do not add dynamic registries or heap allocation.
- The WS2812 driver owns a fixed 768-byte framebuffer and PIO0 SM0 only in
  selected builds. The 32x8 mapping, transport, corners, perimeter, row/column
  scans, temporary 5x7 font, full-height 6x8 candidate, and continuous clock
  mode were physically evaluated. The approved physical clock glyphs are the
  exact variable-width 5x7 table in Consumer Edition Requirements Section 5;
  digit 1 is width 3, colon width 1, and spacing is 1. The physical face is
  always unpadded 12-hour time with 12 at midnight/noon and draws at y=0..6.
  Glyph geometry stays independent of effects. The approved glyphs and
  formatting have been physically verified. The former geometry/fill/raw-chase
  walkthrough is retired from normal runtime. Development console `color`
  reports the current color and `color <r> <g> <b>` selects temporary logical
  RGB (each channel 0..255); color changes refresh the frame upstream of
  output brightness. The physical amber calibration anchor is logical
  RGB(255, 96, 0) at global brightness 6 on the BTF-LIGHTING 8x32 panel through
  the installed black grid and LCD-derived diffuser. The everyday default is
  soft white RGB(255, 240, 220) at brightness 2; amber remains an alternate
  reference at brightness 6. Future gamma/transfer
  correction must preserve that perceived appearance unless physically
  re-evaluated. Brightness remains an appearance control, not a current limit.
  Effects should decorate around the readable glyph mask;
  glyph shapes stay conventional and independent of effects. WS2812 output
  applies software global
  brightness (0..255, default 2) while serializing pixels; development builds
  expose `brightness` and `brightness <0-255>`. This is an appearance control,
  not a current limiter, and does not vary the panel's 5 V supply.
- Keep clock glyph geometry independent of its logical RGB paint. The logical
  compositor applies background, glyph-mask paint, then optional overlay.
  Black/off paint is opaque RGB(0, 0, 0), while transparent/no-op leaves
  underlying pixels unchanged; preserve this boundary. Follow [Consumer Edition Requirements, Section 8](docs/Consumer-Edition-Requirements#8-matrix-status-pixels-and-visual-effects).
- Keep future WS2812 intensity and transfer work downstream of logical
  rendering: effects/local per-pixel intensity, then global brightness, then
  perceptual/LED transfer conversion before serialization. Brightness and
  local intensity are distinct from gamma and electrical current protection.
  If reducing intended visual intensity, scale before transfer correction.
  The future current-budget limiter's ordering relative to transfer
  correction is unresolved; do not implement or prescribe it from this note.
  Development builds include the characterization-only `g {off|g2|ws}`
  selector and `pat`/`ramp` patterns. `off` is identity; candidates are not
  approved for production. Preserve the RGB(255, 96, 0) at brightness 6
  physical amber anchor through this BTF 8x32 panel's installed black grid and
  LCD-derived diffuser. The everyday default is soft white RGB(255, 240, 220)
  at brightness 2; amber at brightness 6 remains the named reference alternate.
  Keep palette values as logical RGB, independent of transfer selection. See
  the engineering baseline for comparison formulas and plan.
- For Dad's current WS2812 8x32 build, reserve panel coordinate (31,7) for
  timing/GPS/PPS/satellite health and (31,6) for actual holdover indication.
  These are planned presentation overlays, not implemented behavior. Give
  them final or near-final priority over future scenes/effects. A separate
  chain of approximately 3–4 WS2812 status LEDs remains a valid general AAC-CE
  option for other builds, but is not planned for Dad's build. Do not assign
  external status GPIO or reserve PIO0 SM1 before implementation; SM1 is only
  a preferred option if available.

## Hardware ownership

- GPIO assignments are centralized in `include/pins.hpp`. GPS UART0 uses
  GP0/GP1 and GPS PPS uses GP2 in production. GP4 is optional VFD UART1 TX;
  GP5 is unassigned and available. GP8 is WS2812 DATA only when its driver is
  selected; default builds leave it uninitialized.
- Inspect the pin map and owning module before changing assignments. Do not
  treat planned pins or platform-capable peripherals as active allocations.

## Timing holdover

Up to 24 hours of local-oscillator holdover after qualified GPS/PPS is an
approved future requirement, not implemented behavior. Holdover must remain
distinguishable from physical GPS lock, must not fabricate valid UTC on cold
startup, cannot survive power loss without an independent persistent time
reference, and expires to invalid/unsynchronized state. Do not describe it as
operational or implement it without an authorized task.

## Development console

Development-console changes must preserve production isolation,
bounded nonblocking I/O, and authoritative UTC qualification.
Fault-injection capabilities require an explicitly approved task.
Consult the engineering baseline and current source for actual
implementation and verification status.

## Resource ledger

Maintain the permanent resource ledger at `docs/ce-resource-ledger.md`. Record
production and development profiles separately, including build-derived flash
and static SRAM, stack/heap observations or estimates, GPIO reservations, PIO
state machines and instruction memory, DMA, UART/SPI/I2C/PWM, timers,
interrupts, watchdog, important latency constraints, display-backend budgets,
capacity concerns, and peripheral conflicts as applicable. At meaningful
milestones, record deltas and new consumers. Label facts as measured,
build-derived, calculated, estimated, or unverified; mark unavailable data and
never invent values or infer peripheral allocation from capability. The ledger
is a separate required artifact; do not omit it from future engineering work.

## Verification and documentation

Standard verification commands, run from the repository root:

```sh
pio run -e pico2
pio run -e pico2-dev
sh test/host/run.sh
git diff --check
git status --short
```

Build both profiles when shared source changes. Add focused host coverage for
testable timing, configuration, simulation, and presentation behavior. Preserve
coverage for UTC qualification; RMC/PPS association and atomic reception
sampling; PPS cadence, monotonic timestamps and rollover; production/development
source selection; simulation acquisition/reacquisition; console validation and
mode behavior; timezone/DST; and display-backend isolation. Do not upload
firmware or operate physical hardware without explicit authorization. A build
or host test is not hardware qualification; report the test environment
accurately.

Keep project documentation concise, accurate, and consistent with source and
the engineering baseline. State whether functionality is implemented,
planned, physically verified, or unresolved. Report contradictions for
engineering review instead of expanding scope to reconcile them.
