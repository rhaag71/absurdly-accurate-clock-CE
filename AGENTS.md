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

- Keep display hardware behind the display-independent backend boundary. The
  existing PD-2200/Noritake-compatible backend remains supported. Its current
  interface uses UART1 through a MAX3232; the display uses a Noritake-compatible
  serial command protocol.
- Dad's selected display is one 8x32 WS2812B RGB flexible PCB. WS2812 support is
  not implemented. The planned DATA signal is GP8 through an SN74HCT541N powered
  from 5 V, with approximately 330-ohm series resistance, 100 nF local bypass,
  and shared Pico/display ground. The level shifter and supporting parts are
  planned for a small perfboard attached to the display module. These are
  proposed electrical details, not qualified hardware.
- Do not invent matrix serpentine mapping, connector orientation, pixel order,
  measured current, or final power limits. Keep experimentally verified device
  behavior distinct from behavior inferred from manuals.
- Backends own their hardware. Display initialization must not configure
  unrelated hardware, and slow display operations must not block authoritative
  timing. The backend contract must not require every display to support RGB,
  pixels, framebuffers, animation, or instantaneous updates.
- Future multi-display composition is an approved architectural requirement:
  heterogeneous outputs may consume shared presentation state with separate
  responsibilities and update cadences. Concurrent backends and composition
  are not implemented; do not add them as part of unrelated work.

## Hardware ownership

- GPIO assignments are centralized in `include/pins.hpp`. Current assignments:
  GPS UART0 uses GP0/GP1; GPS PPS input uses GP2. The existing PD-2200 interface
  uses UART1 through MAX3232. GP8 is only a proposed WS2812 data output.
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
