# Changelog

All notable changes to Flight Above Head are documented here.

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
