# What the panel can do

Flight Above Head is designed to look like a decorative object first and an
information surface when the moment calls for it. The display transitions
between ambient scenes, time, weather, live aircraft, and regional alerts
without a phone, dashboard, or browser open on the desk.

## The ambient gallery

Select any effect with `saver next` or `saver 1..14`. A short touch also enters
or advances the active saver.

1. **Radar** — rotating sweep, contacts, and time.
2. **Daylight wave** — a continuously eased wave whose energy follows the day.
3. **Fire** — warm animated flame field.
4. **Lava lamp** — slow, colorful metaballs.
5. **Bouncing aircraft** — a large aircraft sprite moving across the panel.
6. **Weather clock** — local time with current outdoor conditions.
7. **Radial fill clock** — a minimalist circular time composition.
8. **Flip clock** — digital flip-card-inspired time.
9. **Analog clock** — hands, markers, and an occasional midnight fly-by.
10. **Flight through stars** — head-on aircraft silhouette with a star field.
11. **Glyph field** — calm falling glyphs with a hidden `Nice` message.
12. **Square pyramid** — low-speed subpixel wireframe geometry.
13. **Minimal clock** — a quiet time/date display with a gentle colon breath.
14. **Daily forecast** — current/apparent temperature, min/max, UV, wind,
    sunrise, and sunset.

The renderer uses an effect-specific cadence rather than needlessly redrawing
every frame. Static information is only redrawn when visible data changes;
animated effects run at their intended smoothness, while HUB75 DMA scanning
continues independently.

## Live moments

The display applies a deliberate priority order:

```text
ALERT → AIRCRAFT → hardware test → screensaver → last aircraft → idle clock
```

- **Aircraft card** — a current overhead aircraft replaces ambient content with
  an animated flight-card arrival. Double-touch acknowledges only that current
  identity and returns to the selected saver; a different aircraft can still
  take priority.
- **Alert card** — a regional rocket alert overrides every other view and cannot
  be dismissed from the physical button.
- **Last Aircraft** — retains the most recent non-empty aircraft result after a
  pass has ended.
- **Hardware test** — available on demand to inspect panel, sensor, touch, and
  brightness behavior.

This is an informational display, not a certified emergency-warning device.
Keep independent official alert channels available.

## A panel that looks after itself

### Automatic light and night behavior

`night auto` is the default behavior. Sunrise/sunset from the configured
location select the day/night palette. At night, the panel switches to a warm,
dimmer treatment. With a connected BH1750 sensor, sustained darkness moves it
into a near-dark `SLEEP` state; sustained light or sunrise restores normal
operation.

- DAY uses normal color and brightness ranges.
- NIGHT begins at local sunset and uses a warm, lower-brightness palette. The
  transition from DAY to NIGHT is eased continuously over 60 seconds, including
  both the palette and automatic panel brightness.
- SLEEP begins after 60 continuous seconds at or below 1 lux and exits after
  30 seconds at or above 3 lux, or at sunrise.
- Alerts retain their high-visibility range even during NIGHT and SLEEP.

Use `night on` or `night off` for a temporary visual override, then return with
`night auto`.

### Controls

| Input | What it does |
| --- | --- |
| First touch after a true power-on | Initializes HUB75; the touch is intentionally consumed. |
| Short touch | Enters or advances ambient screensavers. |
| Double touch | Opens/closes Last Aircraft, or acknowledges the active aircraft. |
| Long touch | Toggles BH1750 automatic brightness and the manual brightness preset. |
| `panel on` / `panel off` | Arms or blanks the panel without stopping network, time, or sensors. |
| `brightness auto` / `brightness fixed` / `brightness 0..255` | Selects sensor, state defaults, or an exact output level. |
| `saver next` / `saver 1..14` | Selects the ambient scene. |
| `screen auto` | Returns from a forced visual-test screen to normal live priority. |
| `night auto` / `night on` / `night off` | Selects automatic, forced night, or forced day mode. |
| `diag` / `status` | Prints a compact health snapshot on USB Serial. |

## Built for continuous operation

The firmware is designed for unattended, 24/7 use on the reference hardware:

- HUB75 DMA scans continuously while a dedicated render task owns every display
  call; double buffering prevents half-drawn frames.
- Network I/O, rendering, touch, and light sensing are separated so a slow HTTP
  response cannot block panel scanning or button handling.
- Live data uses timeouts, bounded retries, failure hysteresis, and retained
  last-good values.
- The panel stays blank on a true cold boot until intentional arming. Once
  armed, its state survives software, watchdog, brownout, and reset-button
  restarts through RTC-retained recovery state.
- Diagnostics expose heap, PSRAM, stack headroom, reset cause, and worker
  heartbeats for a meaningful soak test.

24/7 suitability still depends on panel cooling, a correctly sized fused 5 V
supply, stable Wi-Fi, and an actual overnight soak test in the final enclosure.
Follow the [hardware pinout and power guidance](hardware-pinout.md) and the
[release checklist](release-checklist.md) before treating an installation as
continuous service.

## Build your own

Start with the safe templates in the repository root:

```bash
cp secrets.example.h secrets.h
cp location.example.h location.h
```

Then follow the concise [README quick start](../README.md#quick-start). Exact
wiring, recovery, and compatibility details stay in the technical docs:

- [Hardware pinout and electrical safety](hardware-pinout.md)
- [Runtime architecture](runtime-architecture.md)
- [Troubleshooting](troubleshooting.md)
- [Release checklist](release-checklist.md)
