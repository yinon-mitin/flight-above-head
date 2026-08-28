# Build and deploy

This is the canonical build recipe for the reference ESP32-S3-N16R8 / HUB75E
installation. Use it when you want the same secret-free build that GitHub
Actions validates.

## Arduino IDE 2.x

1. Add Espressif's Boards Manager URL:

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

2. Install **esp32 by Espressif Systems** version `3.3.11`.
3. Install these libraries through Library Manager:
   - `ESP32 HUB75 LED MATRIX PANEL DMA Display` `3.0.14`
   - `ArduinoJson` `7.4.3`
   - `Adafruit GFX Library` `1.12.6`
4. Open `FlightAboveHead.ino` in a directory named `FlightAboveHead`.
5. Select these key Tools settings:

| Option | Value |
| --- | --- |
| Board | `ESP32S3 Dev Module` |
| Flash Size | `16MB (128Mb)` |
| PSRAM | `OPI PSRAM` |
| Partition Scheme | `16M Flash (3MB APP/9.9MB FATFS)` |
| USB CDC On Boot | Enabled |
| USB Mode | Hardware CDC and JTAG |
| Upload mode | UART0 / Hardware CDC |
| Upload speed | `921600` (use `460800` or `115200` if needed) |

Keep `build_opt.h` beside `FlightAboveHead.ino`; it enables the PSRAM DMA
framebuffer used by the HUB75 driver.

## Arduino CLI

Clone into a directory that matches the sketch name, then install the pinned
toolchain and libraries:

```bash
git clone https://github.com/yinon-mitin/flight-above-head.git FlightAboveHead
cd FlightAboveHead

arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
arduino-cli lib install "ESP32 HUB75 LED MATRIX PANEL DMA Display@3.0.14"
arduino-cli lib install "ArduinoJson@7.4.3"
arduino-cli lib install "Adafruit GFX Library@1.12.6"
```

Compile the secret-free firmware:

```bash
arduino-cli compile \
  --fqbn 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc' \
  .
```

For a deployed build, first create local `secrets.h` and `location.h` from the
tracked templates. Never add either file to a commit.

## Upload and first light

1. Confirm the panel supply is off while wiring, then connect common ground,
   HUB75 signals, touch, and optional BH1750 according to the
   [hardware pinout](hardware-pinout.md).
2. Upload the sketch and open Serial Monitor at `115200` baud.
3. Apply power using a panel-rated fused 5 V supply.
4. On a true cold start, press the touch button once or send `panel on`.
   The first touch intentionally initializes the display but does not select a
   scene.
5. Confirm `[display] ready`, Wi-Fi, NTP, and the expected GPIO map before
   increasing brightness.

If upload, PSRAM, color order, or panel startup is not as expected, use
[troubleshooting](troubleshooting.md).
