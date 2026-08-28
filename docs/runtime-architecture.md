# Concurrent runtime and static-frame rendering

## Task split

The firmware uses two explicit FreeRTOS tasks:

| Task | Core | Priority | Responsibility |
| --- | ---: | ---: | --- |
| `network` | 0 | 0 (idle) | Wi-Fi reconnect, NTP status, alert, flight/history, solar, and weather APIs |
| `render` | 1 | 2 | State selection, visible-state comparison, complete frame drawing and DMA flip |

Arduino's normal `loop` task only prints a diagnostic snapshot every five
seconds. TLS parsing and HTTP timeouts therefore cannot interrupt drawing.
Panel scanning itself is continuous hardware DMA and does not depend on either
task being scheduled at a precise instant.

## Shared state

Network results are committed to a small shared model under a FreeRTOS mutex.
The render task holds the mutex only long enough to copy that model, then
releases it before drawing. No HTTP client, JSON document, or display method is
shared between tasks.

`api_config.h` points live flight, persistent history, and alert requests to
the AirPlaneTracker service at `roshpinaoverhead.online`. Exact installation
coordinates are loaded from the ignored local `location.h`; the tracked
`location.example.h` is safe to publish.

The network task also fetches daily sunrise/sunset, current outdoor/apparent
temperature, current UV index, min/max temperature, and daily maximum wind from
Open-Meteo for the fixed home coordinates. One bundled request supplies current
weather, forecast, and solar fields; it is aligned to the next 15-minute model
slot with a 30-second grace period. This removes the former separate solar
request and normally uses about 96 Open-Meteo calls per day. Failed updates use
a bounded 2/4/8/15-minute backoff. The history endpoint
refreshes every five minutes and restores the latest aircraft after boot.
Failures retain the previous good values. A secret-free build disables this
poll entirely rather than requesting a placeholder location.
Open-Meteo is accessed over HTTPS with a pinned public ISRG Root X1 CA, so
location-bearing requests are encrypted and certificate-authenticated.
Requests force HTTP/1.0, connection-close framing, and explicit JSON/User-Agent
headers to avoid chunked-stream incompatibilities. Weather uses the supported
`current` and `daily` objects in one request. The legacy `current_weather=true`
switch is deliberately absent because it suppresses the modern `current`
object on the deployed endpoint and therefore omits apparent temperature.

## No redraw for identical content

The renderer calculates an FNV-1a signature from only values that are visible:

- selected state (`IDLE`, `LAST_AIRCRAFT`, `AIRCRAFT`, `HARDWARE_TEST`,
  `SCREENSAVER`, or `ALERT`);
- unavailable-service indicators;
- displayed aircraft or alert fields;
- idle time/date and the current colon-breath animation step;
- screensaver pattern and animation step.

API receipt timestamps, request duration, RSSI changes, and diagnostic counters
are intentionally excluded. Polling the same data therefore does not write a
single panel pixel. TTL expiration, a visible value change, the subtle idle
colon, or active screensaver animation produces a new signature and frame.

The desktop brightness policy uses a logarithmic lux curve with state-specific
ranges: a dim `1–52` clock for `IDLE`, a brighter shared `3–160` range for
`AIRCRAFT` and daytime screensavers, and high visibility for `ALERT`. Healthy service
indicators remain hidden; only unavailable network, flight, alert, history,
weather, or time
dependencies are drawn in the top-right corner.

The visual policy has three modes. `DAY` uses the normal ranges. `NIGHT` starts
at fetched sunset, applies a centralized warm color transform, and uses lower
idle/screensaver/aircraft ranges. `SLEEP` requires 60 continuous seconds at or
below 1 lux and caps those states at `1/1/1`; it exits after 30 seconds above
3 lux or at sunrise. `ALERT` always renders with the daytime color mapping and
the normal `150–255` alert range.

BH1750 and the touch input are serviced by the Arduino loop task without
sharing I2C or GPIO operations with the network task. Only compact sensor state
is committed under the shared mutex. The render task remains the sole owner of
display calls and applies brightness changes independently from framebuffer
redraws, so changing ambient light does not rebuild a static screen.
The touch line is sampled every 1 ms with asymmetric 2 ms press / 5 ms release
debounce and a 280 ms double-tap window. Pulses ending after at least 1 ms are
latched even if they end before the stable-press transition. A first short tap
after wake enters or advances the screensaver immediately. A second tap restores an exact
snapshot of the prior UI state. On ordinary UI pages this opens the retained
last-aircraft page. While a live aircraft is visible, it instead acknowledges
only that current aircraft and returns to the selected screensaver; another
identity still takes priority, and the acknowledgement is cleared by a
successful empty live response. Active alerts remain non-dismissible.
The resulting physical-button page and screensaver index are persisted with
`Preferences` only when their values change and restored before the worker
tasks start. Render frames and sensor samples never write NVS.

On a genuine power-on, HUB75 starts in a hardware-blank standby state while the
network, time, APIs, BH1750, and touch input run normally. DMA allocation is
deferred until the first touch or a `panel on` console command. Once armed, an
RTC no-init marker restores panel power automatically after software, panic,
watchdog, brownout, or external-button resets without writing flash. An
explicit `panel off` clears that marker. The wake touch is consumed solely by
this transition and cannot also change a page or brightness. The boot intro
then runs as an overlay in the normal render task: a short perspective runway
takeoff carries a top-view aircraft through the vanishing point while a round
ordered-dither aperture reveals the restored screensaver. A live alert or aircraft cancels the overlay immediately
and retains normal safety priority.

When a live aircraft takes priority, a short transition first covers the
current UI with a fast full-panel ordered-dither fade, then a large outlined
top-view aircraft cuts the canvas and reveals the live flight card beneath it.
The reveal uses a dedicated 16 ms target
cadence (about 62 fps) and longer eased stages rather than inheriting the
40 fps screensaver cadence. NIGHT/SLEEP draws the card border last in one
explicit amber, so status glyphs cannot interrupt or recolor its top edge.

Service indicators use failure hysteresis. Network and time must fail three
consecutive one-second checks; flight and alert sources must fail three
consecutive polls at their normal polling intervals. History and weather use
the same three-failure threshold. A successful sample resets
the relevant counter and hides its indicator immediately.

The application priority is
`ALERT > AIRCRAFT > HARDWARE_TEST > SCREENSAVER > LAST_AIRCRAFT > IDLE`.
Button-driven screens therefore cannot hide an aircraft or active alert,
except for the explicit double-tap acknowledgement of the currently displayed
aircraft described above. The
last non-empty aircraft response is stored separately, so a later empty poll
does not erase the history page.

Idle colon breathing runs at 25 bounded animation steps per second. Screensaver
render cadence is selected per effect: the weather screen wakes once per
second, slow metaballs and the square pyramid run at about 30 fps, and most
motion runs at 40 fps. The second screensaver integrates its wave phase from
frame elapsed time and eases wavelength, speed, and amplitude over 45 seconds;
the day-to-evening behavior therefore has no minute-boundary discontinuity.
Static aircraft, alert, history, and test pages wake the render task only every
100 ms and redraw only when their visible-state signature changes. When the
panel is off, the render task checks every 250 ms. The HUB75 DMA scan rate
remains safely high and independent, so this CPU optimization cannot introduce
panel flicker.

All fourteen effects use direct DMA/GFX drawing. No FastLED, GFX_Lite, extra
framebuffer, or Aurora pattern class is linked into the firmware. The radar
uses a cached polar-angle map and contains only contacts born behind the
scanline. The full-panel metaballs use fractional centers, full-spectrum HSV,
and dithered 5/6/5 output without increasing the six-bit color depth. The
larger bouncing aircraft uses bilinear sprite coverage.
The tenth effect projects a small fixed star field outward from the panel
center and anchors it with a head-on aircraft silhouette. The eleventh uses a
fixed fine glyph matrix with independent falling highlight columns and a hidden
`Nice` message.
The twelfth effect is a depth-weighted perspective square pyramid. Its
wireframe uses lightweight subpixel line coverage and runs at roughly 30 fps;
motion speed remains low so it does not compete for attention on a desk.
The thirteenth effect intentionally returns to the original clock-and-date-only
layout. It suppresses service icons and wakes at 25 fps solely for the subtle
one-second colon brightness breath.
The fourteenth effect is a static compact daily forecast with current/apparent
and min/max temperature, current UV, maximum wind/direction, sunrise, and
sunset. It reuses the same retained last-good Open-Meteo snapshot as the
weather clock. Its top and solar rows are centered as complete visual groups,
not as independent text fragments.
All effects share the same state-level DMA brightness. Flight data is polled
every five seconds on the separate network task. Alert and flight intervals
are measured from request completion, preventing a slow or timed-out request
from being followed by an immediate retry burst. The alert proxy has its own
twelve-second timeout because TLS establishment plus the upstream alert fetch
can legitimately exceed five seconds; the frequent flight endpoint keeps its
shorter five-second bound.

The Serial Monitor accepts non-blocking runtime controls for panel power,
BH1750/fixed/manual `0..255` brightness, visual mode, forced screen selection,
screensaver selection, diagnostics, and the hardware test. `help` prints the
complete command list. A forced screen intentionally bypasses normal alert and
aircraft priority for visual testing; `screen auto` restores normal selection.
The physical long press toggles BH1750 control and exact `200/255` manual
brightness; alert brightness retains its independent safety range. USB CDC uses
a zero transmit timeout and periodic diagnostics are skipped when
no host is connected. Consequently an absent or stalled Serial Monitor cannot
block either the main button/sensor loop or the separate network task.
Steady Wi-Fi state appears only in `[diag][SYSTEM]`; standalone `[network]`
messages are limited to connect/disconnect/retry transitions. HTTP and JSON
failures use `[error]`, so an unavailable API is not mislabeled as a Wi-Fi
failure.
Boot and periodic diagnostics include the ESP-IDF reset reason, whether panel
state was automatically recovered, approximate previous-session uptime, and
the last RTC heartbeats of the Arduino loop, network task, and render task.
This information survives the reset that it diagnoses.

API text passes through a panel-specific ASCII filter before entering visible
state. Common UTF-8 dashes/arrows are normalized, unsupported code points are
dropped, and punctuation-only placeholders trigger readable fallback labels.

## Double buffering

`build_opt.h` enables an Octal-PSRAM DMA framebuffer for the N16R8 module.
`mxconfig.double_buff = true` allocates two complete buffers:

1. DMA continuously scans the front buffer.
2. The render task clears and draws the hidden back buffer.
3. `flipDMABuffer()` publishes the completed frame.
4. The previous front buffer becomes the next hidden drawing target.

This removes partially cleared or partially drawn frames from the visible
output. Six-bit color reduces bandwidth and memory use while retaining ample
color resolution for the text-oriented interface.

## Serial diagnostics

Serial speed is 115200 baud. Boot output confirms:

- chip, revision, core count, and CPU frequency;
- persistent NVS boot count and 64-bit monotonic uptime;
- flash and detected PSRAM;
- free heap;
- active HUB75 pin mapping;
- DMA buffer mode, color depth, refresh rate, and brightness;
- successful creation and assigned core of both tasks.

Every five seconds the `[diag]` output is wrapped in a `SNAPSHOT` header and a
closing divider. Category prefixes (`SYSTEM`, `API`, `SOLAR`, `AUX`, `DISPLAY`,
`MEMORY`, `SENSORS`, and optional `ERROR`) make each block easy to scan. The
block reports:

- persistent boot count, 64-bit uptime, and selected application state;
- Wi-Fi state, RSSI, IP address, and NTP readiness;
- per-endpoint request count, HTTP code, and last duration;
- total request failures;
- current DAY/NIGHT/SLEEP mode and active sunrise/sunset schedule;
- calculated panel refresh, brightness, frame count, and render duration;
- current/minimum heap, largest heap block, free PSRAM;
- minimum remaining stack in ESP-IDF byte units for the network and render
  tasks. The render task reserves 12 KiB to retain safe headroom through its
  deepest animation paths.

During a static aircraft or alert screen, API request counters should continue
increasing while the display frame counter stays unchanged. Idle and
screensaver intentionally animate at bounded rates.
