# Contributing

Thanks for improving Flight Above Head. This project targets a physical ESP32-S3 / HUB75 installation, so changes should be safe for both the published source and the device.

## Before opening a pull request

1. Do not add `secrets.h`, `location.h`, logs, firmware binaries, or photos with private metadata.
2. Keep the Arduino sketch directory rule in mind: `FlightAboveHead.ino` must be built from a directory named `FlightAboveHead`.
3. Update README or a document in `docs/` when behavior, wiring, commands, or configuration changes.
4. Run the available checks:

```bash
python3 -m unittest tests/test_location_privacy.py -v
arduino-cli compile \
  --fqbn 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc' \
  /path/to/FlightAboveHead
```

5. Confirm the working tree contains no private configuration:

```bash
git check-ignore -v secrets.h location.h logs
git status --short
```

## Pull request expectations

- Describe the user-visible change and the hardware tested.
- Keep commits focused and avoid unrelated formatting churn.
- Include a test or a clear manual verification path for firmware behavior.
- Preserve alert priority and never weaken safety-related display visibility without explaining the trade-off.
- Keep GitHub Actions green.

## Issues

Use issues for reproducible bugs, documentation corrections, and concrete feature proposals. Do not include credentials, exact home coordinates, or private network details. For vulnerabilities, use the process in [SECURITY.md](SECURITY.md).
