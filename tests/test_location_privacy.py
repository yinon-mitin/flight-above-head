"""Regression checks for private location handling in the Arduino sketch.

These checks intentionally inspect the sketch source because the firmware cannot
be host-executed without ESP32 hardware. They protect requirements that must
remain true across refactors: coordinates never travel over HTTP, and a
secret-free build never requests weather for a placeholder location.
"""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SKETCH = ROOT / "FlightAboveHead.ino"
CONFIG = ROOT / "api_config.h"


class LocationPrivacyTests(unittest.TestCase):
    def test_open_meteo_url_uses_https(self) -> None:
        source = SKETCH.read_text()
        self.assertIn('"https://api.open-meteo.com/v1/forecast?', source)
        self.assertNotIn('"http://api.open-meteo.com/v1/forecast?', source)
        self.assertIn(
            'strncmp(url, "https://api.open-meteo.com/", 27) == 0', source
        )
        self.assertIn("secureClient.setCACert(OPEN_METEO_ROOT_CA)", source)

    def test_secret_free_build_skips_weather_polling_without_location(self) -> None:
        config = CONFIG.read_text()
        source = SKETCH.read_text()
        self.assertIn("HAS_LOCATION_CONFIGURATION", config)
        self.assertIn("if (!HAS_LOCATION_CONFIGURATION)", source)
        self.assertIn("Weather disabled: location not configured", source)


if __name__ == "__main__":
    unittest.main()
