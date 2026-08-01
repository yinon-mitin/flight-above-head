# Flight Above Head v1.0

Autonomous ESP32-S3/HUB75 desktop display for overhead aircraft, high-priority
alerts, time, weather, and calm ambient clock screens.

Version 1.0 targets the K716 128x64 1/32-scan panel and ESP32-S3-N16R8. Network
requests and rendering run on separate FreeRTOS cores, complete frames are
published through double-buffered DMA, and unchanged static content is not
redrawn. The fixed priority is `ALERT > AIRCRAFT > selected UI`.

After power-up the controller, Wi-Fi, APIs, clock, and sensors start in the
background while HUB75 remains electrically blank and its DMA driver is not
initialized. The first touch starts the panel and reveals the last saved UI
page through the boot animation; that wake-up touch is not treated as a page
change, double tap, or brightness command.

Release history is maintained in [`CHANGELOG.md`](CHANGELOG.md). Runtime and
wiring details live under [`docs/`](docs/).

## Hardware

- Controller: ESP32-S3-N16R8 / HW678 V0.0.0
- Panel: K716-128x64-32S-V3.3, HUB75E, 128x64, 1/32 scan
- Brightness sensor: BH1750, SDA GPIO1, SCL GPIO2, address auto-detected at
  `0x23` or `0x5C`
- Touch module: TTP223-compatible digital output on GPIO18

The verified safe wiring for this N16R8 board is documented in
[`docs/hardware-pinout.md`](docs/hardware-pinout.md). The previous wiring used
boot-strapping pins GPIO3/45/46 and Octal-PSRAM pins GPIO35/37; do not use that
older mapping on this module.

## Build

Open `FlightAboveHead/FlightAboveHead.ino` in Arduino IDE. For the photographed
N16R8 board, use:

- Board: `ESP32S3 Dev Module`
- Flash size: `16MB`
- PSRAM: `OPI PSRAM`
- Flash mode: `QIO 80MHz`

Required libraries:

- `ESP32-HUB75-MatrixPanel-DMA`
- `ArduinoJson`

No external BH1750 library is required; the sketch uses the built-in `Wire`
library and the sensor's documented I2C commands directly.

Before network stages, copy `FlightAboveHead/secrets.example.h` to `FlightAboveHead/secrets.h` and fill local Wi-Fi credentials. `secrets.h` is ignored by Git.

HUB75 pin constants are intentionally kept at the top of
`FlightAboveHead/FlightAboveHead.ino`. Keep them synchronized with
`docs/hardware-pinout.md` if the wiring changes.

API endpoints are configured in `FlightAboveHead/api_config.h`. The current
profile reads live flights, flight history, and alerts from
`roshpinaoverhead.online`. On startup, the newest history item restores the
`LAST AIRCRAFT` page even before another aircraft flies overhead.

The same file contains the fixed installation coordinates. Sunrise, sunset,
current outdoor temperature, and apparent temperature are fetched from the
keyless Open-Meteo API in the `Asia/Jerusalem` timezone. The last successful
values remain active across temporary request failures; before the first
successful solar response the firmware safely falls back to `06:00` sunrise
and `19:00` sunset.

Do not remove `FlightAboveHead/build_opt.h`. It places the two HUB75 DMA frame
buffers in the N16R8 module's Octal PSRAM. Selecting anything other than
`OPI PSRAM` makes display initialization fail with an explicit serial message.

## Runtime profile

- Fixed fallback: idle `24`, aircraft `140`, screensaver `140`, alert `255`
- Button-selected manual brightness: `200`
- Automatic idle range: `1–52`
- Automatic aircraft range: `3–160`
- Automatic alert range: `150–255`
- Automatic daytime screensaver range: `3–160`
- Night ranges: idle `1–20`, screensaver `1–24`, aircraft `1–24`
- Sleep levels: idle `1`, screensaver `1`, aircraft `1`
- Six-bit panel color depth
- Minimum requested refresh: 120 Hz
- Double DMA buffer in Octal PSRAM
- Network task: core 0
- Render task: core 1
- Wi-Fi power saving disabled for predictable request latency

The idle screen contains a larger bold centered clock and a brighter small date. Its
colon completes one subtle sinusoidal brightness breath per second at 25
animation steps per second. Aircraft and alert layouts use short centered rows.
Unavailable dependencies appear in the top-right corner only while unhealthy:
`N` network, `F` flight source, `A` alert source, `H` history, `W` weather,
and `T` time sync. A symbol is
shown only after three consecutive failed checks; a successful check clears it
immediately. This prevents one-second Wi-Fi or API interruptions from flashing
on an otherwise calm desktop screen.

The render task hashes only visible state. Repeated API responses with identical
content do not touch the framebuffer. Only the idle colon and an active
screensaver deliberately request timed animation frames. See
[`docs/runtime-architecture.md`](docs/runtime-architecture.md).

At 115200 baud, serial diagnostics are printed at boot and every five seconds
while a Serial Monitor is connected.
They include Wi-Fi/IP/RSSI, API counts and latency, HTTP codes, refresh rate,
frame count, heap/PSRAM, visual mode, sunrise/sunset, solar refresh state, and
task stack headroom. The network task additionally
prints `wifi=ok` or `wifi=down` every ten seconds so connection state remains
visible even when the monitor misses the initial transition.
USB CDC writes use a zero timeout, so closing Arduino IDE or leaving its monitor
idle cannot block the button, BH1750 sampling, or network health updates.

The Serial Monitor also exposes a non-blocking runtime control console:

```text
panel on | panel off
brightness auto | fixed | 0..255
screen auto | idle | last | aircraft | test | saver | alert
saver next | saver 1..12
night auto | night on | night off
test | status | diag | help
```

`brightness auto` returns control to BH1750, `fixed` uses state fallback
levels, and a numeric value applies an exact global DMA brightness. A forced
`screen` is intended for visual testing, but a real aircraft or alert still
retains safety priority. `screen auto` returns to button-selected UI state.
`panel off` blanks output without stopping
Wi-Fi, API polling, sensors, or the console. If DMA has not yet been started,
`panel on` performs the same deferred initialization as the first button touch.

## Light sensor and touch test

BH1750 is sampled once per second. The measured value is smoothed and passed
through a logarithmic curve spanning darkness through 350 lux. At approximately
228 lux the target is about 46 for idle, 142 for aircraft/screensavers, and
243 for alerts;
near 1 lux the corresponding targets are about 3, 43, and 153. If the sensor is
absent, fixed fallback values remain active. Both legal BH1750 addresses are
detected automatically, and recovery is attempted every five seconds.
Flight and alert strings are filtered to the printable ASCII supported by the
built-in GFX font. UTF-8 placeholders such as an em dash no longer turn into
garbled multi-character glyphs; missing fields use explicit English fallbacks.

A first touch wakes a panel that is still in startup standby. Once awake, a
short touch immediately enters screensaver mode; subsequent short touches
advance to the next effect. If a second touch arrives within 280 ms, the first
action is rolled back to an exact saved state and the pair opens the retained
`LAST AIRCRAFT` page. Another double touch returns to idle. Pulses as short as
1 ms are latched separately so a very quick fingertip touch is not discarded
by debounce. There are twelve
restrained effects:

1. split-screen radar with clock and an NTP-synchronized, microsecond-smoothed
   seconds-hand sweep with a fading trail; a hidden target becomes visible
   only while the beam crosses its angle, then fades continuously for exactly
   63 seconds;
2. continuous thick wave with a centered clock; its wavelength and speed
   follow the time of day and become long and calm by 22:00;
3. fire whose daytime flames are tall and gradually subside toward evening;
4. eight sharper full-panel metaballs with a continuous full-spectrum color
   cycle and a bold compact rounded clock on a transparent background;
5. subpixel-smoothed expanding rings with a low-amplitude gravitational
   contraction front that reaches successive rings quickly and releases each
   one smoothly at every new minute;
6. a larger head-on antialiased aircraft bouncing around the panel; an exact
   corner hit unlocks a 30-second `- Yes, That just happened.` message;
7. weather clock with a rounded digital face, date, temperature, and apparent
   temperature;
8. centered rounded clock with a clockwise 60-minute fill; the sector stays
   still within a minute and smoothly fills exactly one additional minute at
   second `00` without a backward recoil; during `59:59` the full sector
   clears smoothly and reaches the empty state exactly at `00:00`;
9. four-card flip clock with shaded top/bottom faces and perspective folding;
10. second-accurate analog clock with discrete second/minute movement and a
    thick top-view aircraft crossing the exact center in a random direction
    at `00:00`;
11. a gently drifting, unlit and engine-free aircraft silhouette flying
    through a restrained star field, with a tiny drop event at `16:20`;
12. a fixed TomThumb matrix of letters, digits, and symbols with many
    independently falling column highlights and medium trails; every leading
    glyph uses the secondary color, and the matrix includes a hidden
    horizontal `Nice` behind the large outlined clock.

A long touch switches between BH1750 automatic brightness and exact manual
`200/255` brightness. Alerts keep their separate high-priority `150–255`
automatic range and are not reduced by the manual setting. Screensaver uses the
same daytime automatic `3–160` profile as the aircraft page (about `142/255`
around 228 lux), while the
existing `1–24` night range keeps it near-black after sunset.

The effects are lightweight implementations inspired by HUB75 DMA/Aurora
examples and restrained clock displays. They use a dynamic 1–50 fps render cadence,
draw directly through the existing DMA/GFX interface, and do not add its
GFX_Lite/FastLED dependency.
Every effect receives the same global DMA brightness selected for the current
DAY/NIGHT/SLEEP mode; there are no per-effect brightness overrides. Color
variation belongs to the artwork, not to panel output intensity.
Live aircraft and alerts override menu pages, button-driven screensavers, and
diagnostic Serial screens. The fixed priority is always
`ALERT > AIRCRAFT > selected UI`.

The physically selected UI page and screensaver number are saved in ESP32 NVS
only when they change. After a power loss the panel restores that page instead
of always returning to the first screen; animation frames never write flash.

After the first wake touch, the restored screensaver is rendered behind a solid
accent canvas. A top-view aircraft cuts through that canvas, then both feathered
halves move apart to reveal the already-running saved effect. The intro is
non-blocking: network and sensor work continues normally.

## Day, night, and sleep

`DAY` changes to a warm `NIGHT` theme at the current day's sunset and returns
at sunrise. Solar times are refreshed after connection and every six hours;
failed refreshes retry every ten minutes without discarding the last good
schedule. If BH1750 remains at or below `1 lux` for 60 seconds during the
night, `SLEEP` caps idle and screensaver output at `1/255` and aircraft at
`1/255`. It exits after 30 seconds at or above `3 lux`, or immediately at
sunrise. These delays prevent a hand shadow or a brief light change from
switching modes.

Alerts never inherit the warm theme or sleep caps: their automatic brightness
continues to use the prominent `150–255` range.

For debugging from Serial Monitor, send `night on` to force NIGHT, `night off`
to force DAY, and `night auto` to return to sunrise/sunset control. The `test`
command opens the eight-second sensor diagnostic page; `help` lists commands.

## High-brightness power

Use a dedicated regulated 5 V panel supply sized for the panel manufacturer's
maximum-current specification. Never route panel current through the ESP32
board. Use short, sufficiently thick power wires, a common ground, and a fuse
appropriate for the supply and wiring.

## MVP Order

1. Panel and color test
2. Wi-Fi and NTP
3. Alert API
4. Flight API
5. State machine
6. BH1750 automatic brightness
7. Button debounce and press handling
8. Resilience and long-running test
