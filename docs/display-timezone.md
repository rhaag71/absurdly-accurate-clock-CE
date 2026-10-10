# Display timezone and HH diagnostics

> **Current CE status:** there is no GP6 button or `ZONE` record. The selected
> zone currently defaults to UTC. HH and TXHH records are emitted by the
> selected PD-2200 backend, in the compact formats documented below. The former
> button behavior is retained here as historical implementation context.

## Historical inherited zone-button behavior

GP6 (physical pin 9) is an active-low button to GND using the RP2350 internal
pull-up. No external pull-up is needed. Every boot selects UTC. Stable short
presses cycle UTC → Eastern → Central → Mountain → Pacific → UTC. Selection is
RAM-only. Both press and release must be stable for 30 ms; holding never repeats.
A button held at boot must first be released for 30 ms. Pulses shorter than the
debounce interval are intentionally ignored. Polling uses wrap-safe millisecond
subtraction and no delays or button interrupts.

`presentation::convertUtcForDisplay` produces a separate civil calendar value,
abbreviation, UTC offset and daylight flag. Only the renderer's time/zone fields
use it. The authoritative UTC epoch, calendar, PPS association, GPS logic and
rolling-decade phase remain unchanged. There are no writes back to the timebase.
The existing top-row label cells 3–5 and time cells 7–14 (zero-based) are retained;
all changes use the existing changed-character/positioned-write path.

Rules are contemporary U.S. DST rules (2007 onward), **not historical timezone
rules**. Mountain observes DST; this is not an Arizona mode. Standard offsets are
−5/−6/−7/−8 hours, with +1 hour in daylight time. For each UTC calendar year,
calculate the second Sunday in March and first Sunday in November. Start is local
02:00 converted using the standard offset; end is local 02:00 converted using the
daylight offset. Compare the UTC epoch against the half-open [start, end) interval.
Then offset a separate epoch and convert its complete calendar, including day,
year and leap-day rollovers. No ambiguous local-time lookup or timezone database.

Labels are UTC, EST/EDT, CST/CDT, MST/MDT, PST/PDT. Without valid UTC/date,
non-UTC modes show E--/C--/M--/P-- with the existing unavailable time placeholders;
this avoids falsely asserting a standard/daylight state before synchronization.

## USB diagnostics

GPS/PPS transition messages remain in the common diagnostic queue. The PD-2200
backend emits these best-effort ASCII display records directly to USB serial;
records end in CRLF:

```text
HH ms=<uptime_ms> pps=<sequence> valid=<0|1> utc=<HH:MM:SS> zone=<name> civil=<HH:MM:SS> want=<HH> cache=<HH>
TXHH ms=<uptime_ms> pps=<sequence> kind=pair bytes=1B4807<tens_hex><ones_hex> want=<HH> cache=<HH>
```

`HH` records the first rendered frame and changes in desired HH or zone label.
Its cache is sampled before that service pass. When `valid=0`, the UTC/civil
fields are not valid time. It does not log every second or rolling-indicator
update. The removed `ZONE` record is intentionally retired with the physical
zone-selection button.

`TXHH` records completion of one contiguous HH pair accepted by UART, after
updating both cache cells. For example, `bytes=1B48073030` means ESC H, address
07, ASCII `00`. HH always starts at 07; normal updates never position directly
at 08. The header/tens can have been accepted on earlier loop iterations. Both
payloads refresh until tens is accepted, then freeze until ones succeeds. A
newer desired hour is submitted as another pair afterward. Thus `want` may
differ from the recorded frozen pair. The cache updates per accepted byte;
partial cache values in HH records are expected. A canceled header-only command
produces no TXHH record. These are firmware/UART observations, **not physical
VFD readback**. See [HH accommodation](hh-zero-investigation.md#implemented-pd-2200-accommodation-contiguous-hh).

Formatting occurs in the PD-2200 backend only on these events. It writes a whole
record only when USB reports enough free space; otherwise that record is
dropped without waiting. There is no display-diagnostic drop counter. These
records are UART submission observations, not physical VFD readback. No flash
logging or periodic refresh is added. The paired-HH accommodation retains
nonblocking retries; its effectiveness requires physical verification.

For a VFD diagnostic run, capture USB serial at 115200 across hour transitions.
Correct desired/cached/accepted bytes still cannot exclude corruption downstream
of the UART API; wire capture at the VFD input remains the next discriminator.

## Bench checks after a separate upload

- Boot/reboot in UTC; the CE build has no physical zone button.
- Confirm current standard/daylight abbreviations and previous-local-day hours.
- Check HH/TXHH on hour changes; ordinary seconds produce no new records.
- Disconnect/reconnect USB while running; time acquisition must remain responsive.
- Preserve any impossible HH evidence rather than assuming this feature fixed it.
