# Troubleshooting

Use Serial Monitor at 115200 baud and begin with a complete boot block plus at
least two `[diag]` snapshots. A single network timeout or transient status is
not enough evidence to identify a firmware failure.

## Upload does not start

1. Confirm `ESP32S3 Dev Module`, the correct serial port, and `Hardware CDC and
   JTAG` USB mode.
2. Try upload speed 460800 or 115200 instead of 921600.
3. Disconnect the HUB75 ribbon and retry. If that helps, verify that no panel
   signal uses GPIO0/3/45/46 or another strap pin.
4. Hold BOOT, tap RST, start upload, then release BOOT after connection begins.
5. Keep GPIO19/20 free because they are native USB D-/D+ on ESP32-S3.

## `OPI PSRAM not found`

Select **Tools → PSRAM → OPI PSRAM** and use `ESP32S3 Dev Module`. Confirm the
module is actually N16R8 or another supported OPI-PSRAM variant. Do not remove
`build_opt.h`; the double DMA framebuffer depends on it.

## Panel remains black

Cold power-on intentionally leaves HUB75 blank. Press the touch button once or
send `panel on`.

If `[display] ready` never appears:

- check PSRAM configuration and free memory;
- verify the 5 V panel supply and shared ground;
- verify OE=GPIO16, LAT=GPIO15, CLK=GPIO17, and the full pin table;
- wait for `[display] retrying DMA initialization` after an allocation failure;
- test a lower panel brightness only after DMA is ready.

If the ESP32 starts but the panel initializes only after RST, power sequencing
is suspect. Power the panel supply before or at the same time as the ESP32,
keep OE correctly wired, and inspect the 5 V ramp under load.

## Wrong colors

The reference K716 panel needs `PanelColorOrder::BRG`. A conventional panel
usually needs `PanelColorOrder::RGB`. `PANEL_SWAP_RED_BLUE` alone cannot correct
a three-channel permutation. Use a pure red/green/blue hardware test before
changing animation palettes.

## Flicker, ghosting, broken rows, or black trail pixels

- Shorten the ribbon/signal wiring.
- Confirm a solid common ground and clean 5 V supply.
- Add a 74AHCT245/74HCT245 level shifter close to the panel.
- Keep the tested `HZ_10M`, `clkphase=false`, and latch blanking 2 baseline.
- Change only one timing parameter at a time.
- Do not confuse CPU animation cadence with HUB75 scan refresh; DMA scan remains
  continuous even when a static screen is redrawn once per second.

## BH1750 is missing or unstable

- Power it from 3.3 V.
- Verify SDA=GPIO1 and SCL=GPIO2.
- The firmware probes address `0x23` (ADDR low/open) and `0x5C` (ADDR high).
- Add approximately 4.7 kΩ pull-ups to 3.3 V if the breakout has none.
- Watch `[sensor] BH1750 recovery=...` and `[diag][SENSORS]`.
- Use `brightness fixed` to separate sensor problems from display problems.

## Touch input misses or invents gestures

- Use a digital TTP223-style module, not a bare capacitive electrode.
- Power the module from 3.3 V and connect OUT to GPIO18.
- Confirm active-HIGH behavior or set `TOUCH_BUTTON_ACTIVE_HIGH=false`.
- Inspect `[button] fast tap`, `tap 1`, `tap 2`, and `down/up` logs.
- Long wires can act as antennas; shorten them and keep them away from HUB75
  clock and panel power wiring.
- A first press after cold power-up only starts the panel by design.

## Wi-Fi does not connect

- Confirm `secrets.h` exists beside the sketch.
- ESP32-S3 uses 2.4 GHz Wi-Fi; verify the SSID is available on 2.4 GHz.
- Confirm credentials, access-point client limits, and signal strength.
- `Association refused too many times` is an access-point association failure;
  the firmware continues retrying every ten seconds.
- Use `[diag][SYSTEM]` for steady Wi-Fi/IP/RSSI state. `[network]` intentionally
  reports transitions only and does not duplicate healthy status continually.

## HTTP and JSON errors

| Symptom | Meaning / action |
| --- | --- |
| HTTP `-11 read Timeout` | Server accepted or began a request but did not finish within the endpoint timeout. Check latency/server load; occasional failures are retained and retried. |
| HTTP `-1 connection refused` | Destination refused the TCP connection or was unreachable. Verify host, port, container binding, firewall, and LAN routing. |
| `JSON parse: IncompleteInput` | The body ended early. Inspect logged body length/content length and proxy behavior. |
| HTTP 200 plus parse failure | Response shape/content is not the expected JSON schema. Compare it with `docs/api-observations.md`. |

Requests use HTTP/1.0 and connection-close framing to avoid partial/chunked
stream issues seen with this ESP32-S3 deployment. Do not reduce timeouts until
the upstream latency distribution is known.

## Service letter remains visible

| Letter | Dependency |
| --- | --- |
| N | Wi-Fi/network |
| T | NTP time |
| F | live flights |
| A | alerts |
| H | aircraft history |
| W | weather/Open-Meteo |

The letter appears only after three consecutive failures and clears on the next
success. Use the matching `[diag]` category and HTTP status rather than the
letter alone.

## Unexpected reset

Capture the complete ESP-ROM reset line and the first `[diag][RESET]` after
reboot.

- `task-wdt`: compare retained main/network/render heartbeat timestamps. The
  stale task is the likely starvation site.
- brownout: inspect the ESP32 supply separately from panel 5 V, shared ground,
  wiring drop, and peak panel load.
- panic/software reset: capture the backtrace and ELF SHA; decode against the
  exact build.
- repeated DMA allocation failure: inspect heap, largest free block, PSRAM,
  and whether another panel driver was partly initialized.

The diagnostics show free stack **bytes** for loop, network, and render tasks.
Values trending toward zero require investigation even if total heap is large.

## Safe fault-isolation order

1. Force `brightness fixed` and `screen idle`.
2. Confirm stable ESP32 power with the panel unplugged.
3. Confirm Wi-Fi/NTP/API behavior without HUB75 DMA.
4. Connect HUB75 signal and common ground while panel power is off.
5. Start the panel at low brightness.
6. Add BH1750, then the touch module.
7. Restore `screen auto`, `brightness auto`, and `night auto`.
