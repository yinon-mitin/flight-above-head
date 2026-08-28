# Changelog

All notable changes to Flight Above Head are documented here.

## Unreleased

### Added

- Added a new interior-display project mark that depicts ambient screensavers,
  live aircraft, and rocket-alert priority.
- Adopted Apache License 2.0 with a redistribution NOTICE that credits Yinon
  Mitin and links to the original Flight Above Head project.
- Added live-aircraft acknowledgement: a double touch on the live flight card
  returns to the selected screensaver without suppressing a later aircraft.
- Added publication-ready English setup, deployment, GPIO, compatible-hardware,
  power, Serial console, diagnostics, privacy, and maintenance documentation.
- Added a detailed troubleshooting guide, pre-release privacy/validation
  checklist, and a pinned secret-free GitHub Actions Arduino build.
- Added a tracked `location.example.h` template while keeping real installation
  coordinates in ignored local configuration.
- Added a fourteenth compact weather-forecast screen with current/apparent and
  daily min/max temperature, current UV, maximum wind/direction, sunrise, and
  sunset.
- Added a persistent NVS boot counter and non-wrapping 64-bit uptime to boot
  and runtime diagnostics.
- Added reset-cause diagnostics plus RTC-retained loop/network/render
  heartbeats and approximate previous-session uptime.
- Added a calm rotating perspective square-pyramid screensaver.
- Added lightweight subpixel line coverage for smoother slow wireframe motion
  without FastLED, GFX_Lite, or an additional framebuffer.
- Added a minimal clock-and-date screensaver matching the original idle layout;
  its only animation is the subtle one-second colon breath.

### Changed

- Increased the live-aircraft arrival transition to a dedicated 16 ms target
  cadence with longer eased stages for more intermediate frames and smoother
  canvas fill, fly-through, and reveal motion.

### Fixed

- Restored a clearly visible, larger top-view aircraft silhouette in the live
  arrival transition and replaced its directional canvas wipe with a fast,
  full-panel ordered-dither fade-in.
- Fixed double-touch navigation after acknowledging a live aircraft: while its
  API entry remains active, a double touch on a screensaver now opens Last
  Aircraft instead of redundantly acknowledging the hidden flight again.
- Removed the second screensaver's minute-boundary wave jump. Its phase is now
  integrated continuously, while daytime wavelength, speed, and amplitude
  ease toward their targets over 45 seconds.
- Centered the complete current-condition and sunrise/sunset groups on the
  fourteenth compact forecast screen.
- Consolidated Open-Meteo weather, forecast, and solar data into one request
  aligned to each 15-minute model slot (about 96 calls/day normally), with a
  bounded 2/4/8/15-minute retry backoff and explicit real-call diagnostics.
- Replaced the text-heavy fourteenth forecast layout with a compact icon grid
  for conditions, current/apparent and min/max temperature, UV, directed wind,
  sunrise, and sunset; only the measured values remain as text.
- Changed the forecast UV value from the day's maximum to Open-Meteo's current
  `uv_index`, refreshed with the current-condition snapshot throughout the day.
- Moved the best-effort network worker to FreeRTOS idle priority. Long TLS or
  HTTP waits now share Core 0 with `IDLE0` instead of starving the five-second
  task watchdog, while the existing longer Alerts timeout remains available
  for slow but valid responses.
- Reassigned the former split-canvas startup reveal to live-aircraft arrival:
  the current screen is covered, a top-view aircraft cuts the canvas, and the
  flight card is revealed beneath it. Panel startup now uses a restrained
  perspective-runway takeoff with a circular dithered screensaver reveal.
- Grouped each periodic Serial Monitor diagnostic snapshot between visible
  dividers and added category labels for faster scanning.
- Removed the periodic standalone `wifi=ok` line; steady Wi-Fi/IP/RSSI state is
  now reported once in `[diag][SYSTEM]`, while `[network]` is reserved for
  connection transitions and retry activity.
- Classified HTTP/API failures under `[error]` instead of the misleading
  `[network]` prefix, increased the flight-request timeout to five seconds and
  the slower alert-proxy timeout to twelve seconds, and spaced alert/flight
  polls from completion so a timeout cannot cause an immediate retry burst.
- Centered the complete `LAST AIRCRAFT` information block on the 128x64 panel.

### Removed

- Removed the former screensavers 5, 13, and 15 (gravity rings, abstract
  contours, and the 4D tesseract) and compacted the remaining selection to
  thirteen effects.

### Fixed

- Removed the conflicting Open-Meteo `current_weather=true` legacy switch.
  The API now returns and the firmware strictly parses distinct
  `temperature_2m` and `apparent_temperature` values instead of silently
  copying current temperature into the missing feels-like field.
- Made all four edges of the live-aircraft card use one continuous amber
  border in NIGHT/SLEEP mode, including pixels beneath service indicators.
- Preserved an armed panel across software, panic, watchdog, brownout, and
  reset-button restarts so a transient controller reset no longer leaves the
  24/7 display waiting indefinitely for another touch. Cold power-on standby
  and explicit `panel off` remain unchanged.
- Corrected FreeRTOS stack high-water diagnostics to report ESP-IDF byte units
  without multiplying by `StackType_t`, and increased the render-task stack
  from 8 KiB to 12 KiB.
- Reworked the minute-sector clock's `59:59 → 00:00` rollback to use the NTP
  wall-clock fraction and cosine easing directly, eliminating opposing
  transition restarts and angular jumps. The softly feathered edge now moves
  explicitly counterclockwise at roughly 60 fps and reaches a fully empty
  background at minute `00`, including when this saver opens during `59:59`.

## v1.0 — 2026-08-01

First hardware-tested ESP32-S3/HUB75 release.

### Display and rendering

- Added the verified 128x64 HUB75E pin map for ESP32-S3-N16R8 without boot
  strap or Octal-PSRAM conflicts.
- Added six-bit color, dual DMA framebuffers in OPI PSRAM, and atomic frame
  swaps to prevent partially drawn updates.
- Split network work and rendering across the two ESP32-S3 cores.
- Added visible-state hashing and dynamic render cadence so static screens do
  not redraw unnecessarily while animated screens remain smooth.
- Added deferred HUB75 startup: the controller and services boot in the
  background, while the panel remains blank until the first touch or
  `panel on` command.
- Added non-blocking boot and screensaver-to-aircraft transition animations.

### Aircraft and alerts

- Added live aircraft and alert polling with fixed priority
  `ALERT > AIRCRAFT > selected UI`.
- Added centered aircraft and last-aircraft pages with callsign, route,
  altitude, speed, and heading.
- Added persisted flight-history loading so the last aircraft survives a
  controller restart.
- Added transient-failure hysteresis and compact corner indicators for
  network, time, flight, alert, history, and weather availability.
- Added bounded HTTP timeouts, reconnect handling, last-good data retention,
  and printable-ASCII normalization for the GFX font.

### Clock, environment, and brightness

- Added NTP time with the Israel daylight-saving timezone.
- Added Open-Meteo weather plus daily sunrise/sunset data for automatic
  DAY/NIGHT selection.
- Added BH1750 detection at `0x23`/`0x5C`, one-second sampling, smoothing,
  recovery, and logarithmic automatic brightness.
- Added NIGHT and low-lux SLEEP themes while keeping alerts prominent.
- Added long-touch switching between automatic brightness and manual
  `200/255` output.

### Input and ambient screens

- Added fast-pulse latching and asymmetric debounce for the GPIO18 touch
  module.
- Added single-touch screensaver navigation, double-touch last-aircraft view,
  long-touch brightness control, and consumption of the initial wake touch.
- Added NVS persistence for the selected UI page and screensaver.
- Added twelve direct DMA/GFX screensavers: radar clock, adaptive wave, fire,
  full-panel metaballs, gravity rings, bouncing aircraft, weather clock,
  minute-sector clock, flip clock, analog clock, flight through stars, and a
  glyph matrix.

### Diagnostics and documentation

- Added a non-blocking Serial control console for panel power, brightness,
  screen overrides, night mode, screensavers, and diagnostics.
- Added periodic Wi-Fi/API/display/memory/PSRAM/task-stack diagnostics without
  allowing an absent Serial Monitor to block runtime work.
- Added wiring, API, architecture, build, power-supply, and validation notes.
