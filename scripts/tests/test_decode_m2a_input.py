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


MODULE_PATH = Path(__file__).resolve().parents[1] / "decode-m2a-input.py"
SPEC = importlib.util.spec_from_file_location("auroraaz_decode_m2a_input", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
decoder = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = decoder
SPEC.loader.exec_module(decoder)


RECORD_SIZE = 160
CRC_OFFSET = 156


def repair_crc(record: bytearray) -> None:
    struct.pack_into(">I", record, CRC_OFFSET, zlib.crc32(record[:CRC_OFFSET]))


def make_record(
    generation: int = 1,
    *,
    seen_mask: int = 0,
    press_mask: int = 0,
    repeat_mask: int = 0,
    release_mask: int = 0,
    consumed_mask: int = 0,
    filter_queued_mask: int = 0,
    safety_flags: int | None = None,
    counters: dict[tuple[int, int], int] | None = None,
) -> bytearray:
    record = bytearray(RECORD_SIZE)
    record[:4] = b"AZI2"
    struct.pack_into(">HH", record, 4, 2, RECORD_SIZE)
    struct.pack_into(">I", record, 8, generation)
    struct.pack_into(">I", record, 12, 0x01020304)
    struct.pack_into(">I", record, 16, seen_mask)
    struct.pack_into(">I", record, 20, press_mask)
    struct.pack_into(">I", record, 24, repeat_mask)
    struct.pack_into(">I", record, 28, release_mask)
    struct.pack_into(">I", record, 32, consumed_mask)
    struct.pack_into(">I", record, 36, filter_queued_mask)
    if safety_flags is None:
        safety_flags = (1 if consumed_mask else 0) | (
            2 if filter_queued_mask else 0
        )
    struct.pack_into(">I", record, 40, safety_flags)
    struct.pack_into(">I", record, 44, 7)
    for (control, event), value in (counters or {}).items():
        struct.pack_into(">H", record, 48 + (control * 3 + event) * 2, value)
    struct.pack_into(">I", record, 90, 0x10203040)
    struct.pack_into(">I", record, 94, 2)
    record[98] = 1
    record[99] = 3
    struct.pack_into(">III", record, 100, 0x11223344, 0x55667788, 0x822113F8)
    struct.pack_into(">HHH", record, 112, 0x5814, 0x0005, 0x3456)
    record[118] = 4
    record[119] = 2
    record[120] = 1
    record[121] = 0
    record[122] = 0
    record[123] = 0
    record[124] = 1
    record[125] = 1
    record[126] = 3
    record[127] = 0x44
    repair_crc(record)
    return record


class RecordValidationTests(unittest.TestCase):
    def test_decodes_big_endian_counts_and_safety_fields(self) -> None:
        record = make_record(
            0xA1B2C3D4,
            seen_mask=0x09,
            press_mask=0x01,
            repeat_mask=0x08,
            release_mask=0x01,
            consumed_mask=0x01,
            filter_queued_mask=0x08,
            safety_flags=0x0F,
            counters={(0, 0): 0x1234, (0, 2): 3, (3, 1): 0xABCD},
        )
        record[120] = 2
        record[121] = 1
        record[122] = 1
        record[123] = 1
        repair_crc(record)

        result = decoder.decode_record(record)

        self.assertEqual(result["generation"], 0xA1B2C3D4)
        self.assertEqual(result["runtime_state"], "running")
        self.assertTrue(result["worker_entered"])
        self.assertEqual(result["controls"]["a"]["press"], 0x1234)
        self.assertEqual(result["controls"]["a"]["release"], 3)
        self.assertEqual(result["controls"]["dpad_left"]["repeat"], 0xABCD)
        self.assertTrue(result["controls"]["a"]["consumed"])
        self.assertTrue(result["controls"]["dpad_left"]["filter_queued"])
        self.assertEqual(result["controls"]["lstick_right"]["press"], 0)
        self.assertEqual(
            result["safety"],
            {
                "flags": 0x0F,
                "consumed_mask": 0x01,
                "filter_queued_mask": 0x08,
                "consumed": True,
                "filter_queued": True,
                "requested_not_observe": True,
                "effective_not_observe": True,
                "observe_only_ok": False,
            },
        )
        self.assertEqual(result["last"]["serial"], 0x11223344)
        self.assertEqual(result["last"]["caller_return_address"], 0x822113F8)
        self.assertEqual(result["last"]["control"], "dpad_left")
        self.assertEqual(result["last"]["event"], "repeat")
        self.assertEqual(result["last"]["requested_stage"], "consume")
        self.assertEqual(result["last"]["effective_stage"], "consume_verified")

    def test_requires_exact_record_size(self) -> None:
        record = make_record()
        for damaged in (record[:-1], record + b"\0"):
            with self.subTest(size=len(damaged)):
                with self.assertRaisesRegex(decoder.TelemetryDecodeError, "exactly 160"):
                    decoder.decode_record(damaged)

    def test_rejects_identity_and_format_fields(self) -> None:
        cases = ((0, 0x00, "magic"), (5, 3, "version"), (7, 159, "record size"))
        for offset, value, message in cases:
            with self.subTest(message=message):
                record = make_record()
                record[offset] = value
                repair_crc(record)
                with self.assertRaisesRegex(decoder.TelemetryDecodeError, message):
                    decoder.decode_record(record)

    def test_rejects_crc_corruption(self) -> None:
        record = make_record()
        record[12] ^= 0x80
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "CRC32 mismatch"):
            decoder.decode_record(record)

    def test_uses_ieee_crc32(self) -> None:
        self.assertEqual(decoder._crc32_ieee(b"123456789"), 0xCBF43926)

    def test_rejects_nonzero_reserved_bytes_even_with_valid_crc(self) -> None:
        record = make_record()
        record[143] = 1
        repair_crc(record)
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "reserved bytes"):
            decoder.decode_record(record)

    def test_rejects_invalid_mask_semantics(self) -> None:
        cases = (
            (16, 0x80, "unknown bit"),
            (20, 0x01, "subsets of seen_mask"),
            (32, 0x01, "subset of seen_mask"),
            (36, 0x01, "subset of seen_mask"),
            (40, 0x10, "unknown bit"),
        )
        for offset, value, message in cases:
            with self.subTest(offset=offset):
                record = make_record(safety_flags=0)
                struct.pack_into(">I", record, offset, value)
                repair_crc(record)
                with self.assertRaisesRegex(decoder.TelemetryDecodeError, message):
                    decoder.decode_record(record)

        record = make_record(seen_mask=1, consumed_mask=1, safety_flags=0)
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "consumed safety"):
            decoder.decode_record(record)
        record = make_record(seen_mask=1, filter_queued_mask=1, safety_flags=0)
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "filter-queued"):
            decoder.decode_record(record)

    def test_rejects_out_of_range_enums_and_booleans(self) -> None:
        byte_fields = (
            (98, 2, "worker_entered"),
            (99, 5, "last_command"),
            (118, 8, "last_control"),
            (119, 4, "last_event"),
            (120, 3, "last_requested_stage"),
            (121, 2, "last_effective_stage"),
            (122, 2, "last_consumed"),
            (123, 2, "last_filter_queued"),
            (124, 2, "last_would_handle"),
            (125, 2, "last_coverflow_active"),
        )
        for offset, value, message in byte_fields:
            with self.subTest(field=message):
                record = make_record()
                record[offset] = value
                repair_crc(record)
                with self.assertRaisesRegex(decoder.TelemetryDecodeError, message):
                    decoder.decode_record(record)

        record = make_record()
        struct.pack_into(">I", record, 94, 5)
        repair_crc(record)
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "runtime_state"):
            decoder.decode_record(record)


class SlotSelectionTests(unittest.TestCase):
    def test_selects_newer_valid_slot(self) -> None:
        result = decoder.select_newest(make_record(10), make_record(11))
        self.assertEqual(result["selected_slot"], "B")
        self.assertEqual(result["generation"], 11)

    def test_ignores_corrupt_slot_and_reports_it(self) -> None:
        corrupt = make_record(20)
        corrupt[70] ^= 1
        result = decoder.select_newest(make_record(10), corrupt)
        self.assertEqual(result["selected_slot"], "A")
        self.assertIn("CRC32 mismatch", result["invalid_slots"]["B"])

    def test_wrap_safe_generation_ordering(self) -> None:
        result = decoder.select_newest(make_record(0xFFFFFFFF), make_record(0))
        self.assertEqual(result["selected_slot"], "B")
        self.assertEqual(result["generation"], 0)
        self.assertTrue(decoder.generation_is_newer(0, 0xFFFFFFFF))
        self.assertFalse(decoder.generation_is_newer(0xFFFFFFFF, 0))

    def test_equal_or_ambiguous_generations_prefer_a(self) -> None:
        for generation_a, generation_b in ((7, 7), (0, 0x80000000)):
            with self.subTest(a=generation_a, b=generation_b):
                result = decoder.select_newest(
                    make_record(generation_a), make_record(generation_b)
                )
                self.assertEqual(result["selected_slot"], "A")

    def test_accepts_only_slot_b(self) -> None:
        result = decoder.select_newest(None, make_record(4))
        self.assertEqual(result["selected_slot"], "B")

    def test_rejects_when_neither_slot_is_valid(self) -> None:
        with self.assertRaisesRegex(decoder.TelemetryDecodeError, "no valid"):
            decoder.select_newest(b"torn", None)


class CommandLineTests(unittest.TestCase):
    def test_cli_emits_json_for_newest_slot(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            slot_a = Path(directory) / "A.bin"
            slot_b = Path(directory) / "B.bin"
            slot_a.write_bytes(make_record(41))
            slot_b.write_bytes(make_record(42))
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                result = decoder.main(
                    ["--slot-a", str(slot_a), "--slot-b", str(slot_b)]
                )
        self.assertEqual(result, 0)
        output = json.loads(stdout.getvalue())
        self.assertEqual(output["selected_slot"], "B")
        self.assertEqual(output["generation"], 42)
        self.assertEqual(output["controls"]["r3"]["repeat"], 0)
        self.assertTrue(output["safety"]["observe_only_ok"])

    def test_cli_returns_error_for_invalid_only_slot(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            slot = Path(directory) / "A.bin"
            slot.write_bytes(b"torn")
            stderr = io.StringIO()
            with redirect_stderr(stderr):
                result = decoder.main(["--slot-a", str(slot)])
        self.assertEqual(result, 1)
        self.assertIn("no valid AZI2 telemetry slot", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
