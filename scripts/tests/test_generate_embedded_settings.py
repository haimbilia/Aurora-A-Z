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

    def test_checked_in_resource_is_compiled_xur(self) -> None:
        resource = SCRIPT.parents[1] / "native" / "assets" / "AuroraAZ_Settings.xur"
        data = resource.read_bytes()
        self.assertTrue(data.startswith(b"XUIB"))
        self.assertGreater(len(data), 64)


if __name__ == "__main__":
    unittest.main()
