# Flight Above Head

An autonomous 128×64 HUB75 desktop display built around an ESP32-S3. It shows
aircraft passing over a fixed location, high-priority alerts, local time and
weather, and a set of deliberately calm ambient screensavers.

The firmware is designed for unattended 24/7 operation. Network requests and
rendering run on separate ESP32-S3 cores, the panel is scanned continuously by
DMA, complete frames are swapped atomically, and unchanged static screens are
not redrawn. Runtime priority is always:

```text
ALERT > LIVE AIRCRAFT > selected user interface
```

This repository contains the hardware-tested firmware and documentation. It
does **not** contain Wi-Fi credentials, API keys, or installation coordinates.
See [Security and privacy](#security-and-privacy) before making a fork public.

## What it does

- Polls live aircraft, retained aircraft history, and alert endpoints.
- Displays callsign, route, altitude, speed, and heading for live and last
  aircraft.
- Synchronizes local time over NTP using the Israel daylight-saving rules.
- Fetches current weather, apparent temperature, current UV, daily min/max,
  wind, sunrise, and sunset from Open-Meteo.
- Uses sunrise/sunset plus BH1750 illumination to select DAY, NIGHT, and SLEEP
  visual modes.
- Keeps alerts bright even when the rest of the panel is in sleep mode.
- Provides fourteen low-distraction DMA/GFX screensavers.
- Supports a fast touch-button UI and a non-blocking Serial control console.
- Retains the selected UI, boot count, reset diagnostics, and the last aircraft.
- Shows small delayed health indicators only after repeated service failures.

## Reference hardware

The tested build uses:

| Part | Tested hardware | Notes |
| --- | --- | --- |
| MCU | ESP32-S3-N16R8, HW-678 V0.0.0 | 16 MB flash, 8 MB Octal PSRAM, dual core |
| LED panel | K716-128x64-32S-V3.3 | HUB75E, 128×64, 1/32 scan |
| Light sensor | BH1750 breakout | I²C address `0x23` or `0x5C` |
| Touch input | TTP223-compatible module | Momentary active-HIGH digital output |
| Panel supply | Regulated 5 V supply | Size for the panel manufacturer's maximum current |

The BH1750 and touch module are optional. Without a BH1750, the firmware uses
fixed brightness fallbacks. Without the touch module, the panel can be started
and controlled through Serial commands.

### Compatible alternatives

The firmware should also work with another ESP32-S3 board when it has:

- two application cores;
- PSRAM that supports the HUB75 DMA buffers (the reference build expects OPI
  PSRAM);
- at least fourteen available output GPIOs for HUB75E plus any desired sensor
  pins;
- a native or UART serial path for upload and diagnostics.

Other 128×64 HUB75E 1/32-scan panels are the easiest substitutions. A panel
with a different resolution, scan pattern, E-address requirement, signal
polarity, or color wiring needs corresponding changes to the constants and
driver configuration near the top of
[`FlightAboveHead.ino`](FlightAboveHead.ino). The tested panel
uses the unusual `BRG` color correction; conventional RGB panels should use
`PanelColorOrder::RGB`.

Classic ESP32, ESP32-C3, ESP32-S2, and ESP32-S3 boards without suitable PSRAM
are not drop-in replacements for this configuration. They may require a
different driver backend, framebuffer layout, pin map, or reduced color depth.

## GPIO layout

This mapping is verified on the photographed ESP32-S3-N16R8/HW-678 carrier. Do
not assume that header labels imply a GPIO is free on another carrier board.

### HUB75E

| HUB75 signal | ESP32-S3 GPIO | HUB75 signal | ESP32-S3 GPIO |
| --- | ---: | --- | ---: |
| R1 | 4 | R2 | 7 |
| G1 | 5 | G2 | 8 |
| B1 | 6 | B2 | 9 |
| A | 10 | B | 11 |
| C | 12 | D | 13 |
| E | 14 | LAT / STB | 15 |
| OE | 16 | CLK | 17 |
| GND | GND |  |  |

### Sensors and input

| Device | Signal | ESP32-S3 GPIO |
| --- | --- | ---: |
| BH1750 | SDA | 1 |
| BH1750 | SCL | 2 |
| TTP223 | OUT | 18 |

Power the BH1750 and TTP223 from **3.3 V**, not 5 V, and connect all grounds.
The firmware probes both legal BH1750 addresses. The TTP223 input defaults to
active HIGH; change `TOUCH_BUTTON_ACTIVE_HIGH` for an active-LOW module.

The mapping intentionally avoids boot-strapping pins, native USB, UART0, Octal
PSRAM, and the onboard RGB LED. See
[`docs/hardware-pinout.md`](docs/hardware-pinout.md) for the reserved-pin
rationale, peripheral wiring, power notes, and signal-integrity guidance.

## Electrical safety and panel power

HUB75 panels are not powered through the ESP32 board.

1. Use a dedicated regulated 5 V supply sized for the panel's worst-case draw.
2. Join the panel-supply ground, panel ground, and ESP32 ground.
3. Use short, adequately thick power conductors and an appropriate fuse.
4. Wire or disconnect the ribbon cable only while both devices are unpowered.
5. If signal integrity is poor, use a 74AHCT245/74HCT245-style 3.3-to-5 V level
   shifter close to the panel input.

High brightness can draw several amperes depending on the panel. Confirm the
actual panel specification instead of sizing the supply from average desktop
brightness.

## Software requirements

The release is built and tested with:

| Component | Tested version |
| --- | ---: |
| Arduino IDE | 2.x |
| Espressif `arduino-esp32` core | 3.3.11 |
| ESP32 HUB75 LED Matrix Panel DMA Display | 3.0.14 |
| ArduinoJson | 7.4.3 |
| Adafruit GFX Library | 1.12.6 |

`Wire`, `WiFi`, `HTTPClient`, and `Preferences` come from the ESP32 Arduino
core. No external BH1750 library is needed.

Install the ESP32 core through Arduino Boards Manager using Espressif's package
index:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Install the three user libraries above through Library Manager. The HUB75
library's upstream repository is
[`mrcodetastic/ESP32-HUB75-MatrixPanel-DMA`](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA).

## Local configuration

Two local files are deliberately ignored by Git.

### 1. Wi-Fi credentials

Copy the example beside the sketch:

```bash
cp secrets.example.h secrets.h
```

Edit only the copied file:

```cpp
constexpr const char *WIFI_SSID = "your-wifi-ssid";
constexpr const char *WIFI_PASSWORD = "your-wifi-password";
```

### 2. Installation coordinates

Copy the location template:

```bash
cp location.example.h location.h
```

Set latitude and longitude for the installation:

```cpp
#define HOME_LATITUDE 00.000000
#define HOME_LONGITUDE 00.000000
```

Exact home coordinates are treated as private configuration even though
Open-Meteo itself is keyless. A build without `location.h` still compiles for
CI, but deliberately disables weather and solar polling: it never requests a
misleading `0,0` placeholder location.

API endpoints are public configuration in
[`api_config.h`](api_config.h). The default profile uses the
project's public AirPlaneTracker endpoints. A self-hosted or adapted project
can replace these URLs as long as it preserves the documented JSON shape in
[`docs/api-observations.md`](docs/api-observations.md).

## Arduino IDE build and upload

Open [`FlightAboveHead.ino`](FlightAboveHead.ino) in Arduino
IDE and select the following Tools settings for ESP32-S3-N16R8:

| Arduino Tools option | Value |
| --- | --- |
| Board | `ESP32S3 Dev Module` |
| USB CDC On Boot | `Enabled` |
| CPU Frequency | `240MHz (WiFi)` |
| Core Debug Level | `None` for release; `Info` only while diagnosing core issues |
| Events Run On | `Core 1` |
| Flash Mode | `QIO 80MHz` |
| Flash Size | `16MB (128Mb)` |
| Arduino Runs On | `Core 1` |
| Partition Scheme | `16M Flash (3MB APP/9.9MB FATFS)` |
| PSRAM | `OPI PSRAM` |
| Upload Mode | `UART0 / Hardware CDC` |
| Upload Speed | `921600` (`460800` or `115200` if unreliable) |
| USB Mode | `Hardware CDC and JTAG` |

Do not remove [`build_opt.h`](build_opt.h). It defines
`SPIRAM_DMA_BUFFER=1`, allowing the two HUB75 DMA buffers to use the N16R8
Octal PSRAM. If `OPI PSRAM` is not selected, display initialization stops with
a clear Serial error.

Select the USB serial port, click **Upload**, then open Serial Monitor at
`115200 baud` with a newline line ending. If upload stalls, hold BOOT, tap RST,
start upload, and release BOOT when the connection begins.

### Arduino CLI build

For repeatable local or CI builds:

```bash
arduino-cli core install esp32:esp32@3.3.11
arduino-cli lib install \
  "ESP32 HUB75 LED MATRIX PANEL DMA Display@3.0.14" \
  "ArduinoJson@7.4.3" \
  "Adafruit GFX Library@1.12.6"

arduino-cli compile \
  --fqbn 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc' \
  .
```

The repository CI uses a secret-free placeholder build. Real credentials and
coordinates are never required by GitHub Actions.

## First boot and deployment

1. Power the ESP32 and panel, preferably with the panel supply already stable.
2. The controller starts Wi-Fi, NTP, APIs, BH1750, and the touch service while
   keeping HUB75 electrically blank.
3. Press the touch button once, or send `panel on`, to initialize DMA and start
   the panel. This first touch is consumed and does not change screens.
4. Confirm `[display] ready`, `wifi=ok`, `time=ok`, and the expected GPIO map in
   Serial Monitor.
5. Send `test` to inspect BH1750, button, panel, and current brightness.
6. Leave `screen auto`, `night auto`, and `brightness auto` for normal service.

After this deliberate cold-start arming, software, watchdog, brownout, and RST
restarts automatically restore the panel. A genuine power loss intentionally
returns to button-waiting standby. `panel off` explicitly disarms it.

## Touch controls

| Gesture | Action |
| --- | --- |
| First touch after cold power-up | Start HUB75; gesture is consumed |
| Short touch | Enter screensavers or select the next screensaver |
| Double touch within 280 ms | Open/close the retained last-aircraft screen |
| Double touch while a live aircraft is shown | Acknowledge that aircraft and return to the selected screensaver |
| Long touch | Toggle BH1750 auto brightness and manual `200/255` |

The GPIO is sampled every millisecond with fast-pulse latching, so short TTP223
pulses are not intentionally filtered out.
Acknowledging a live aircraft suppresses only its current identity. A different
aircraft is still shown immediately, and a successful empty live response
clears the acknowledgement for a later encounter. Active alerts cannot be
dismissed by the button.

## Serial console

Serial output and commands use `115200 baud`. Logging has a zero USB CDC write
timeout; disconnecting Arduino IDE cannot block the panel, button, or network
tasks.

```text
panel on | panel off
brightness auto | fixed | 0..255
screen auto | idle | last | aircraft | test | saver | alert
saver next | saver 1..14
night auto | night on | night off
test | status | diag | help
```

- `panel off` blanks output but leaves networking, APIs, sensors, and Serial
  alive.
- `brightness auto` uses BH1750; `fixed` selects state fallbacks; a number sets
  exact manual DMA brightness.
- `screen ...` forces a page for visual testing. Real alert and aircraft safety
  priority remains active. Use `screen auto` to release the override.
- `night on` forces NIGHT, `night off` forces DAY, and `night auto` returns to
  sunrise/sunset control.
- `status` and `diag` print an immediate diagnostic snapshot.

## Reading the logs

Boot messages describe configuration and the previous reset before periodic
diagnostics begin:

```text
[boot]       chip, flash, PSRAM, reset recovery, GPIO, task creation
[display]    standby, DMA initialization, refresh rate, allocation failures
[sensor]     BH1750 detection, address, readings, recovery
[network]    Wi-Fi connection transitions and retry activity only
[weather]    accepted Open-Meteo values and bounded retry schedule
[history]    retained aircraft history refresh
[button]     decoded touch gestures
[command]    accepted Serial commands
[error]      HTTP, JSON, or service errors
[fatal]      unrecoverable setup allocation failures
```

Every five seconds, a divided snapshot reports:

```text
[diag][SYSTEM]    boot/uptime, UI state, DAY/NIGHT/SLEEP, Wi-Fi, RSSI, IP, NTP
[diag][RESET]     prior reset cause and retained loop/network/render heartbeats
[diag][API]       alert/flight health, HTTP status, latency, request counters
[diag][SOLAR]     sunrise/sunset snapshot and refresh result
[diag][AUX]       history and weather health
[diag][FORECAST]  weather values and actual Open-Meteo call/failure counts
[diag][DISPLAY]   panel state, refresh rate, brightness, frames, render time
[diag][MEMORY]    heap, largest block, PSRAM, and task stack headroom
[diag][SENSORS]   BH1750 state/lux, auto brightness, button count, saver index
[diag][ERROR]     most recent retained error, when present
```

HTTP `-11` means a read timeout, while `-1` commonly means connection refused
or no route. One failure is not necessarily a firmware fault: health glyphs are
shown only after repeated failures, and last-good data is retained. Use the
HTTP code, request duration, RSSI, free heap, stack headroom, and reset cause
together rather than diagnosing from a single line.

See [`docs/troubleshooting.md`](docs/troubleshooting.md) for upload, blank-panel,
color, flicker, sensor, network, watchdog, and reset procedures.

## Network behavior and external services

- Alerts: every 3 seconds, measured from request completion, 12-second timeout.
- Live flights: every 5 seconds, measured from completion, 5-second timeout.
- History: every 5 minutes; failure retry every 30 seconds.
- Open-Meteo: one combined current/daily request per 15-minute model slot,
  normally about 96 calls/day, with 2/4/8/15-minute failure backoff.
- Wi-Fi power saving is disabled to make request latency predictable.
- NTP uses `pool.ntp.org` and `time.google.com`.

Open-Meteo is used only for a personal non-commercial deployment under its
[usage terms](https://open-meteo.com/en/terms). Adaptors must review the terms
for their own deployment. The firmware retains previous successful weather,
solar, history, aircraft, and alert data through brief outages.

## Brightness and visual modes

BH1750 is sampled once per second, smoothed, and passed through a logarithmic
curve. Daytime aircraft and screensavers target `3–160/255`; alerts target
`150–255/255`. NIGHT begins at fetched sunset and applies a warm theme. During
NIGHT, 60 seconds at or below 1 lux enters SLEEP; 30 seconds at or above 3 lux
exits it. Alerts never inherit the low sleep cap.

All screensavers share the same global state brightness. Their drawing cadence
is dynamic (roughly 1–50 fps) while the hardware HUB75 scan remains continuous
and safely above 120 Hz. Details of task ownership, state hashing, transitions,
and all fourteen effects are in
[`docs/runtime-architecture.md`](docs/runtime-architecture.md).

## Security and privacy

Before publishing a fork:

- verify `secrets.h` and `location.h` are
  ignored and untracked;
- never paste Serial output containing private SSIDs or infrastructure URLs;
- inspect photos for EXIF GPS metadata;
- review public API URLs and confirm you are allowed to redistribute/use them;
- run the tracked-file secret scan from the release checklist;
- do not commit Arduino build outputs, core dumps, or raw long-running logs.

The included panel pinout image contains no camera make/model or GPS metadata.
See [`docs/release-checklist.md`](docs/release-checklist.md) for the full audit.

## Repository map

```text
FlightAboveHead.ino         firmware
api_config.h                public service endpoints + private-location loader
build_opt.h                 PSRAM DMA compile option
secrets.example.h           Wi-Fi template
location.example.h          private-coordinate template
open_meteo_ca.h             pinned public CA for authenticated Open-Meteo TLS
docs/
  hardware-pinout.md        wiring, reserved pins, power, color order
  runtime-architecture.md   tasks, state priority, rendering, resilience
  api-observations.md       expected JSON response shapes
  troubleshooting.md        fault isolation and recovery
  release-checklist.md      pre-publication and release validation
.github/workflows/build.yml secret-free reproducible compile check
CHANGELOG.md                release history
```

## Release status and limitations

The reference hardware is operational and the current firmware compiles with
the pinned toolchain above. Before relying on an adapted build:

- perform a cold-start and overnight soak test;
- test Wi-Fi and upstream outages;
- validate brightness in both direct daylight and complete darkness;
- confirm the alert source and regional assumptions for the installation;
- verify panel power and temperature at maximum intended brightness.

This is a safety-adjacent information display, not a certified emergency
warning device. Do not use it as the sole source of civil-defense alerts or
aviation information.

## License

No open-source license has been selected yet. Copyright therefore remains with
the author and public visibility alone does not grant reuse rights. Choose and
add a `LICENSE` file before inviting redistribution or contributions.

## Third-party software and services

This project builds on:

- [Espressif Arduino-ESP32](https://github.com/espressif/arduino-esp32)
- [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Open-Meteo](https://open-meteo.com/) for current and daily weather data

Their licenses and service terms apply independently. The default aircraft and
alert endpoints are deployment-specific public services, not bundled server
components; confirm availability and permission before redistributing an
adapted configuration.

## Further documentation

- [Hardware pinout](docs/hardware-pinout.md)
- [Runtime architecture](docs/runtime-architecture.md)
- [API observations](docs/api-observations.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Release checklist](docs/release-checklist.md)
- [Changelog](CHANGELOG.md)
