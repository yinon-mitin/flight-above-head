# ESP32-S3-N16R8 / HW-678 pin assignment

This is the wiring target for the photographed HW678 V0.0.0 board and the
K716-128x64-32S-V3.3 HUB75E panel. The mapping avoids pins that can interfere
with boot, Octal PSRAM, native USB, UART0, or the onboard RGB LED.

## HUB75E wiring

| HUB75 signal | ESP32-S3 GPIO | Previous GPIO |
| --- | ---: | ---: |
| R1 | 4 | 8 |
| G1 | 5 | 3 |
| B1 | 6 | 46 |
| R2 | 7 | 9 |
| G2 | 8 | 10 |
| B2 | 9 | 11 |
| A | 10 | 12 |
| B | 11 | 13 |
| C | 12 | 14 |
| D | 13 | 21 |
| E | 14 | 15 |
| LAT / STB | 15 | 47 |
| OE | 16 | 48 |
| CLK | 17 | 45 |
| GND | GND | GND |

The cable photo is shown from two different viewing directions. Follow the
signal names printed on the panel and cable, not the apparent left/right
position in a photograph.

## Other peripherals

| Peripheral | Signal | ESP32-S3 GPIO |
| --- | --- | ---: |
| BH1750 | SDA | 1 |
| BH1750 | SCL | 2 |
| Touch button | input | 18 |

### BH1750 wiring

| BH1750 pin | Connect to |
| --- | --- |
| VCC | ESP32-S3 `3V3` |
| GND | ESP32-S3 `GND` |
| SDA | GPIO1 |
| SCL | GPIO2 |
| ADDR | GND/open for `0x23`, or 3V3 for `0x5C` |

The firmware probes both legal addresses. Power the breakout from 3.3 V unless
its exact schematic confirms a regulator and I2C level shifting. Most breakout
boards already include pull-up resistors; if the bus is unstable and the board
does not, add approximately 4.7 kOhm from SDA to 3V3 and from SCL to 3V3.

### TTP223-style touch module wiring

| Touch-module pin | Connect to |
| --- | --- |
| VCC | ESP32-S3 `3V3` |
| GND | ESP32-S3 `GND` |
| OUT / SIG / I/O | GPIO18 |

The default firmware expects momentary active-HIGH output, which is the common
factory configuration for a three-pin TTP223 module. Do not power it from 5 V:
its output would then exceed the ESP32-S3 input limit. For an active-LOW module,
set `TOUCH_BUTTON_ACTIVE_HIGH = false`; the firmware selects the matching
internal pull resistor automatically. A bare two-wire touch electrode is not a
drop-in replacement for the digital TTP223 module.

## Pins intentionally left unused

- GPIO0, GPIO3, GPIO45, GPIO46: boot-strapping pins. External HUB75 loads or
  pull resistors can select an unintended boot or flash configuration.
- GPIO19, GPIO20: USB D- and D+. Keeping them free preserves the native USB/OTG
  connector.
- GPIO26..32: flash/PSRAM interface group; not suitable as general project I/O.
- GPIO33..37: used by the 8 MB Octal PSRAM in the N16R8 module. The fact that
  some numbers appear on a carrier-board header does not make them free.
- GPIO43, GPIO44: UART0 TX/RX, retained for boot logs and serial recovery.
- GPIO48: commonly wired to the onboard WS2812-compatible RGB LED on this
  HW-678-style board. It is not needed because enough unshared pins are free.

GPIO21 and GPIO38..42, GPIO47 remain available for later peripherals. If a
future revision needs more I/O, prefer these before any reserved group above.

## Power and first test

1. Power the HUB75 panel from a suitable regulated 5 V supply; do not power a
   128x64 panel through the ESP32 board's 5 V pin.
2. Join panel-supply GND, HUB75 GND, and ESP32 GND.
3. Connect the 14 signal wires using the table above while power is off.
4. For the first electrical test after rewiring, temporarily reduce the
   state-specific brightness limits in the sketch, then verify that reset and
   firmware upload still work with the HUB75 cable connected before testing
   maximum alert output.
5. If pixels are unstable, add a 74AHCT245/74HCT245-style 3.3 V-to-5 V buffer
   close to the panel input and keep signal wires short. Do not change the GPIO
   mapping as a first response to signal-integrity problems.

The current desktop profile uses 24/255 while idle, 150/255 for aircraft, and
255/255 for alerts.
Use the panel manufacturer's maximum-current figure to size the regulated 5 V
supply and fuse. Panel current must not pass through the ESP32 board. Do not
connect or disconnect the HUB75 ribbon while either device is powered.

The verified wiring is retained. Tests showed a three-channel permutation
rather than a simple red/blue swap: driver red appears blue, driver blue
appears green, and driver green therefore supplies visible red. The firmware
uses `PanelColorOrder::BRG`, feeding desired B,R,G values into the driver's
R,G,B inputs. When adapting the project to a conventional panel, select
`PanelColorOrder::RGB`.

## Rationale

ESP32-S3 routes most peripheral signals through the GPIO matrix, so HUB75 does
not require specific fixed pins. A contiguous GPIO4..17 block keeps the
high-rate parallel bus away from boot, memory, USB, serial, and onboard-device
conflicts while leaving GPIO1/2 for I2C and GPIO18 for the existing button.

## References

- [Espressif ESP32-S3 GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)
- [Espressif ESP32-S3 hardware design guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)
- [ESP32-S3-WROOM-1/1U datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
- [ESP32-HUB75-MatrixPanel-DMA project](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)
