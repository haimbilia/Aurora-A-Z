from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "generate-embedded-settings.py"
SPEC = importlib.util.spec_from_file_location("generate_embedded_settings", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class EmbeddedSettingsTests(unittest.TestCase):
    def test_emit_c_preserves_bytes_and_size(self) -> None:
        generated = MODULE.emit_c(b"XUIB\x00\xff")
        self.assertIn("0x58u, 0x55u, 0x49u, 0x42u, 0x00u, 0xffu", generated)
        self.assertIn("g_auroraaz_embedded_settings_xur_size = 6u", generated)

    def test_emit_c_accepts_alternate_symbol(self) -> None:
        generated = MODULE.emit_c(b"XUIB", "g_filter_settings")
        self.assertIn("const uint8_t g_filter_settings[]", generated)
        self.assertIn("const uint32_t g_filter_settings_size = 4u", generated)

    def test_checked_in_resource_is_compiled_xur(self) -> None:
        resource = SCRIPT.parents[1] / "native" / "assets" / "AuroraAZ_Settings.xur"
        data = resource.read_bytes()
        self.assertTrue(data.startswith(b"XUIB"))
        self.assertGreater(len(data), 64)

    def test_checked_in_filter_resource_is_compiled_xur(self) -> None:
        resource = (
            SCRIPT.parents[1]
            / "native"
            / "assets"
            / "AuroraAZ_Settings_Filter.xur"
        )
        data = resource.read_bytes()
        self.assertTrue(data.startswith(b"XUIB"))
        self.assertGreater(len(data), 64)

    def test_settings_sources_use_radio_rows_and_mode_specific_focus(self) -> None:
        assets = SCRIPT.parents[1] / "native" / "assets"
        browse = (assets / "AuroraAZ_Settings.xui").read_text(encoding="utf-8")
        filter_mode = (
            assets / "AuroraAZ_Settings_Filter.xui"
        ).read_text(encoding="utf-8")
        for source in (browse, filter_mode):
            self.assertEqual(source.count("<XuiCheckbox>"), 2)
            self.assertEqual(source.count("<Visual>XuiRadioButton</Visual>"), 2)
        self.assertIn("<DefaultFocus>BrowseMode</DefaultFocus>", browse)
        self.assertIn("<DefaultFocus>FilterMode</DefaultFocus>", filter_mode)
        self.assertIn("<Text>Saved mode: Browse</Text>", browse)
        self.assertIn("<Text>Saved mode: Filter</Text>", filter_mode)


if __name__ == "__main__":
    unittest.main()
