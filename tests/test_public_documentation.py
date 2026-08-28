"""Public-documentation regression checks for Flight Above Head."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SKETCH = ROOT / "FlightAboveHead.ino"
PINOUT = ROOT / "docs" / "hardware-pinout.md"
GUIDE = ROOT / "docs" / "experience-guide.md"
BUILD_GUIDE = ROOT / "docs" / "build-and-deploy.md"
PROVIDER_GUIDE = ROOT / "docs" / "data-providers.md"


class PublicDocumentationTests(unittest.TestCase):
    def test_experience_guide_lists_every_screensaver(self) -> None:
        sketch = SKETCH.read_text()
        guide = GUIDE.read_text()
        self.assertIn("SCREENSAVER_COUNT = 14", sketch)
        for number in range(1, 15):
            self.assertIn(f"{number}. ", guide)

    def test_pinout_documentation_matches_firmware_constants(self) -> None:
        sketch = SKETCH.read_text()
        pinout = PINOUT.read_text()
        expected = {
            "HUB75_R1_PIN": ("R1", 4),
            "HUB75_G1_PIN": ("G1", 5),
            "HUB75_B1_PIN": ("B1", 6),
            "HUB75_R2_PIN": ("R2", 7),
            "HUB75_G2_PIN": ("G2", 8),
            "HUB75_B2_PIN": ("B2", 9),
            "HUB75_A_PIN": ("A", 10),
            "HUB75_B_PIN": ("B", 11),
            "HUB75_C_PIN": ("C", 12),
            "HUB75_D_PIN": ("D", 13),
            "HUB75_E_PIN": ("E", 14),
            "HUB75_LAT_PIN": ("LAT / STB", 15),
            "HUB75_OE_PIN": ("OE", 16),
            "HUB75_CLK_PIN": ("CLK", 17),
        }
        for constant, (label, pin) in expected.items():
            self.assertIn(f"{constant} = {pin}", sketch)
            self.assertIn(f"| {label} | {pin} |", pinout)

    def test_public_repository_excludes_internal_migration_notes(self) -> None:
        self.assertFalse((ROOT / "docs" / "source-inventory.md").exists())
        self.assertFalse((ROOT / "docs" / "mvp-plan.md").exists())

    def test_data_provider_guide_discloses_the_runtime_boundary(self) -> None:
        guide = PROVIDER_GUIDE.read_text()
        for phrase in (
            "core runtime dependency",
            "not included in this repository",
            "non-commercial",
            "roshpinaoverhead.online",
        ):
            self.assertIn(phrase, guide)

    def test_build_guide_has_the_canonical_secret_free_compile_recipe(self) -> None:
        build_guide = BUILD_GUIDE.read_text()
        self.assertIn("esp32:esp32:esp32s3", build_guide)
        self.assertIn("PSRAM=opi", build_guide)
        self.assertIn("PartitionScheme=app3M_fat9M_16MB", build_guide)
        self.assertIn("FlightAboveHead.ino", build_guide)

    def test_experience_guide_covers_controls_and_automatic_modes(self) -> None:
        guide = GUIDE.read_text()
        for command in (
            "panel on",
            "panel off",
            "saver next",
            "night auto",
            "night on",
            "night off",
            "brightness auto",
            "screen auto",
        ):
            self.assertIn(f"`{command}`", guide)
        for mode in ("DAY", "NIGHT", "SLEEP"):
            self.assertIn(mode, guide)
        self.assertIn("24/7", guide)


if __name__ == "__main__":
    unittest.main()
