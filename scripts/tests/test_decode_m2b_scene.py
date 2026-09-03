from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib


MODULE_PATH = Path(__file__).resolve().parents[1] / "decode-m2b-scene.py"
SPEC = importlib.util.spec_from_file_location("auroraaz_decode_m2b_scene", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
decoder = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = decoder
SPEC.loader.exec_module(decoder)


def repair_crc(record: bytearray) -> None:
    struct.pack_into(">I", record, 252, zlib.crc32(record[:252]))


def make_record(generation: int = 1) -> bytearray:
    record = bytearray(256)
    record[:4] = b"AZS2"
    struct.pack_into(">HHI", record, 4, 1, 256, generation)
    struct.pack_into(">IIII", record, 12, 3, 2, 1, 1)
    struct.pack_into(">I", record, 28, 2)
    struct.pack_into(">I", record, 32, 0x40)
    struct.pack_into(">II", record, 40, 1, 1)
    struct.pack_into(">IIII", record, 56, 10, 12, 12, 3)
    struct.pack_into(">IIII", record, 72, 0x90001000, 0x90001020, 0x12340001, 3)
    struct.pack_into(">IIIIII", record, 88, 3, 2, 1, 1, 1, 0)
    struct.pack_into(">I", record, 112 + 11 * 4, 1)
    struct.pack_into(">I", record, 112 + 12 * 4, 2)
    record[164] = 0
    record[165] = 11
    record[166] = 11
    record[167] = 1
    record[168] = 0
    record[169] = 0
    record[170] = 0
    record[171] = 0
    record[172] = 1
    record[173] = 1
    record[174] = 1
    struct.pack_into(">I", record, 216, 1)
    repair_crc(record)
    return record


class SceneDecoderTests(unittest.TestCase):
    def test_decodes_hardware_sequence(self) -> None:
        result = decoder.decode_record(make_record(0xA1B2C3D4))
        self.assertEqual(result["generation"], 0xA1B2C3D4)
        self.assertEqual(result["samples"], 3)
        self.assertEqual(result["raw_allowed"], 2)
        self.assertEqual(result["capture_eligible"], 1)
        self.assertEqual(result["max_scanned_nodes"], 3)
        self.assertEqual(result["reason_counts"]["main-focused"], 2)
        self.assertEqual(result["reason_counts"]["main-not-focused"], 1)
        self.assertEqual(result["safety"]["names"], ["system_ui_raw_allow"])
        self.assertEqual(result["last"]["decision_reason"], "main-not-focused")
        self.assertEqual(result["last"]["main_scene_handle"], 0x12340001)
        self.assertEqual(result["last"]["gate_configure_attempts"], 1)
        self.assertEqual(result["last"]["gate_configure_successes"], 1)

    def test_rejects_crc_reserved_and_semantic_corruption(self) -> None:
        corruptions = []
        record = make_record()
        record[12] ^= 1
        corruptions.append((record, "CRC32"))
        record = make_record()
        record[230] = 1
        repair_crc(record)
        corruptions.append((record, "reserved"))
        record = make_record()
        struct.pack_into(">I", record, 24, 3)
        repair_crc(record)
        corruptions.append((record, "eligible"))
        record = make_record()
        struct.pack_into(">I", record, 112, 4)
        repair_crc(record)
        corruptions.append((record, "counter"))
        for damaged, message in corruptions:
            with self.subTest(message=message):
                with self.assertRaisesRegex(decoder.TelemetryDecodeError, message):
                    decoder.decode_record(damaged)

    def test_rejects_fail_open_last_state(self) -> None:
        record = make_record()
        record[165] = 12
        record[166] = 12
        record[168] = 1
        record[169] = 1
        record[170] = 1
        record[171] = 1
        repair_crc(record)
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "fail-closed"):
            decoder.decode_record(record)

    def test_requires_exact_size_and_identity(self) -> None:
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "exactly 256"):
            decoder.decode_record(make_record()[:-1])
        record = make_record()
        record[0] = 0
        repair_crc(record)
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "magic"):
            decoder.decode_record(record)

    def test_selects_newest_and_tolerates_one_bad_slot(self) -> None:
        result = decoder.select_newest(make_record(10), make_record(11))
        self.assertEqual(result["selected_slot"], "B")
        bad = make_record(12)
        bad[0] ^= 1
        result = decoder.select_newest(make_record(10), bad)
        self.assertEqual(result["selected_slot"], "A")
        self.assertIn("B", result["invalid_slots"])

    def test_wrap_order_and_equal_prefer_a(self) -> None:
        self.assertEqual(
            decoder.select_newest(
                make_record(0xFFFFFFFF), make_record(0)
            )["selected_slot"],
            "B",
        )
        self.assertEqual(
            decoder.select_newest(make_record(7), make_record(7))["selected_slot"],
            "A",
        )

    def test_cli_outputs_json_and_reports_invalid_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            slot_a = Path(directory) / "A.bin"
            slot_a.write_bytes(make_record(9))
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                self.assertEqual(decoder.main(["--slot-a", str(slot_a)]), 0)
            self.assertEqual(json.loads(stdout.getvalue())["generation"], 9)

            slot_a.write_bytes(b"bad")
            stderr = io.StringIO()
            with redirect_stderr(stderr):
                self.assertEqual(decoder.main(["--slot-a", str(slot_a)]), 1)
            self.assertIn("no valid AZS2 telemetry slot", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
