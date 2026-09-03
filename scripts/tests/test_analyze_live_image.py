from __future__ import annotations

from contextlib import redirect_stdout
from dataclasses import replace
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "analyze-live-image.py"
SPEC = importlib.util.spec_from_file_location(
    "auroraaz_analyze_live_image", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
analyzer = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = analyzer
SPEC.loader.exec_module(analyzer)


def sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


def put_u16le(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<H", data, offset, value)


def put_u32le(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value)


def encode_target(target: int) -> tuple[int, int]:
    high = ((target + 0x8000) >> 16) & 0xFFFF
    low = target & 0xFFFF
    return analyzer.PPC_LIS_R11 | high, analyzer.PPC_ADDI_R11_R11 | low


def build_synthetic_pe() -> tuple[bytes, bytes, list[int]]:
    profile = analyzer.REV1655_PROFILE
    pe = bytearray(profile.pe_size)
    pe[:2] = analyzer.MZ_SIGNATURE
    put_u32le(pe, 0x3C, 0xF8)
    nt = 0xF8
    pe[nt:nt + 4] = analyzer.PE_SIGNATURE
    coff = nt + 4
    put_u16le(pe, coff, profile.machine)
    put_u16le(pe, coff + 2, profile.section_count)
    put_u16le(pe, coff + 16, profile.optional_header_size)
    put_u16le(pe, coff + 18, profile.characteristics)
    optional = coff + 20
    put_u16le(pe, optional, profile.optional_magic)
    put_u32le(pe, optional + 16, profile.entry_rva)
    put_u32le(pe, optional + 28, profile.image_base)
    put_u32le(pe, optional + 32, profile.section_alignment)
    put_u32le(pe, optional + 36, profile.file_alignment)
    put_u32le(pe, optional + 56, profile.image_size)
    put_u32le(pe, optional + 60, profile.header_size)
    put_u32le(pe, optional + 92, 16)
    directories = optional + 96
    put_u32le(pe, directories + profile.thunk_directory_index * 8,
              profile.thunk_rva)
    put_u32le(pe, directories + profile.thunk_directory_index * 8 + 4,
              profile.thunk_size)
    put_u32le(pe, directories + profile.iat_directory_index * 8,
              profile.iat_rva)
    put_u32le(pe, directories + profile.iat_directory_index * 8 + 4,
              profile.iat_size)
    section_table = optional + profile.optional_header_size
    for index, section in enumerate(profile.sections):
        offset = section_table + index * 40
        encoded_name = section.name.encode("ascii")
        pe[offset:offset + len(encoded_name)] = encoded_name
        put_u32le(pe, offset + 8, section.virtual_size)
        put_u32le(pe, offset + 12, section.virtual_address)
        put_u32le(pe, offset + 16, section.raw_size)
        put_u32le(pe, offset + 20, section.raw_offset)
        put_u32le(pe, offset + 36, section.characteristics)

    text = bytearray(profile.text_size)
    pattern = bytes((offset * 17 + 23) & 0xFF for offset in range(256))
    repeats = (profile.thunk_text_offset + len(pattern) - 1) // len(pattern)
    text[:profile.thunk_text_offset] = (
        pattern * repeats)[:profile.thunk_text_offset]
    thunk_ids: list[int] = []
    for index in range(profile.thunk_slot_count):
        library = index & 1
        ordinal = index + 1
        thunk_id = (library << 16) | ordinal
        thunk_ids.append(thunk_id)
        offset = profile.thunk_text_offset + index * profile.thunk_slot_size
        struct.pack_into(
            ">IIII", text, offset,
            analyzer.THUNK_WORD_0_TAG | thunk_id,
            analyzer.THUNK_WORD_1_TAG | thunk_id,
            analyzer.PPC_MTCTR_R11,
            analyzer.PPC_BCTR,
        )
    pe[profile.text_raw_offset:profile.text_raw_offset + profile.text_size] = text
    iat_offset = profile.iat_rva
    for index, thunk_id in enumerate(thunk_ids):
        struct.pack_into(">I", pe, iat_offset + index * 4, thunk_id)
    return bytes(pe), bytes(text), thunk_ids


def make_live_text(reference_text: bytes) -> bytes:
    profile = analyzer.REV1655_PROFILE
    live = bytearray(reference_text)
    for index in range(profile.thunk_slot_count):
        offset = profile.thunk_text_offset + index * profile.thunk_slot_size
        raw_id = struct.unpack_from(">I", reference_text, offset)[0] & 0xFFFFFF
        library = (raw_id >> 16) & 0xFF
        target = (0x81600000 if library == 0 else 0x80010000) + index * 4
        lis, addi = encode_target(target)
        struct.pack_into(">II", live, offset, lis, addi)
    return bytes(live)


def profile_for(pe: bytes, text: bytes | None = None):
    profile = analyzer.REV1655_PROFILE
    if text is None:
        text = pe[
            profile.text_raw_offset:profile.text_raw_offset + profile.text_size
        ]
    return replace(
        profile,
        name="synthetic Rev1655 fixture",
        pe_sha256=sha256(pe),
        header_sha256=sha256(pe[:profile.header_size]),
        text_sha256=sha256(text),
    )


class LiveImageAnalyzerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.pe, cls.reference_text, cls.thunk_ids = build_synthetic_pe()
        cls.header = cls.pe[:analyzer.REV1655_PROFILE.header_size]
        cls.live_text = make_live_text(cls.reference_text)
        cls.profile = profile_for(cls.pe, cls.reference_text)

    def test_valid_loader_rewrite_is_canonicalized(self) -> None:
        report = analyzer.analyze_image(
            self.pe, self.header, self.live_text, self.profile)
        self.assertTrue(report["compatible"])
        self.assertTrue(report["immutable_prefix"]["equal"])
        self.assertEqual(
            report["thunks"]["raw_marker_slot_count"], 350)
        self.assertEqual(
            report["thunks"]["live_lis_addi_slot_count"], 350)
        self.assertEqual(report["thunks"]["changed_word_count"], 700)
        self.assertEqual(
            report["thunks"]["mtctr_bctr_unchanged_slot_count"], 350)
        self.assertEqual(
            report["hashes"]["canonicalized_text_sha256"],
            self.profile.text_sha256,
        )
        first = report["thunks"]["slots"][0]
        second = report["thunks"]["slots"][1]
        self.assertEqual(first["library"], "xam.xex")
        self.assertEqual(first["ordinal"], 1)
        self.assertEqual(first["target"], "0x81600000")
        self.assertEqual(second["library"], "xboxkrnl.exe")
        self.assertEqual(second["ordinal"], 2)
        self.assertEqual(second["target"], "0x80010004")

    def test_cli_emits_json_and_success(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pe_path = root / "Aurora.exe"
            header_path = root / "header.bin"
            text_path = root / "text.bin"
            pe_path.write_bytes(self.pe)
            header_path.write_bytes(self.header)
            text_path.write_bytes(self.live_text)
            output = io.StringIO()
            with redirect_stdout(output):
                status = analyzer.main([
                    "--pe", str(pe_path),
                    "--header", str(header_path),
                    "--text", str(text_path),
                ], self.profile)
        self.assertEqual(status, 0)
        parsed = json.loads(output.getvalue())
        self.assertTrue(parsed["compatible"])
        self.assertEqual(parsed["schema"], analyzer.SCHEMA)

    def assert_error(self, code: str, *, pe: bytes | None = None,
                     header: bytes | None = None, text: bytes | None = None,
                     profile=None) -> analyzer.AnalysisError:
        with self.assertRaises(analyzer.AnalysisError) as raised:
            analyzer.analyze_image(
                self.pe if pe is None else pe,
                self.header if header is None else header,
                self.live_text if text is None else text,
                self.profile if profile is None else profile,
            )
        self.assertEqual(raised.exception.code, code)
        return raised.exception

    def test_rejects_wrong_source_pe_hash(self) -> None:
        bad = bytearray(self.pe)
        bad[-1] ^= 1
        self.assert_error("pe_sha256_mismatch", pe=bytes(bad))

    def test_rejects_live_header_difference(self) -> None:
        bad = bytearray(self.header)
        bad[7] ^= 1
        error = self.assert_error("live_header_mismatch", header=bytes(bad))
        self.assertEqual(error.details["difference_count"], 1)
        self.assertEqual(error.details["difference_offsets"], ["0x00000007"])

    def test_rejects_immutable_text_difference(self) -> None:
        bad = bytearray(self.live_text)
        bad[0x1234] ^= 1
        error = self.assert_error(
            "live_text_immutable_prefix_mismatch", text=bytes(bad))
        self.assertEqual(error.details["difference_count"], 1)

    def test_rejects_malformed_raw_marker(self) -> None:
        profile = analyzer.REV1655_PROFILE
        bad_pe = bytearray(self.pe)
        marker_offset = (
            profile.text_raw_offset + profile.thunk_text_offset)
        bad_pe[marker_offset] = 0x03
        bad_reference = bytes(bad_pe[
            profile.text_raw_offset:profile.text_raw_offset + profile.text_size
        ])
        bad_profile = profile_for(bytes(bad_pe), bad_reference)
        self.assert_error(
            "raw_thunk_marker_mismatch", pe=bytes(bad_pe),
            profile=bad_profile)

    def test_rejects_thunk_missing_from_iat(self) -> None:
        profile = analyzer.REV1655_PROFILE
        bad_pe = bytearray(self.pe)
        struct.pack_into(">I", bad_pe, profile.iat_rva, 0)
        bad_profile = replace(self.profile, pe_sha256=sha256(bad_pe))
        self.assert_error(
            "raw_thunk_iat_membership_mismatch", pe=bytes(bad_pe),
            profile=bad_profile)

    def test_rejects_live_thunk_opcode(self) -> None:
        profile = analyzer.REV1655_PROFILE
        bad = bytearray(self.live_text)
        struct.pack_into(">I", bad, profile.thunk_text_offset, 0x60000000)
        self.assert_error("live_thunk_encoding_mismatch", text=bytes(bad))

    def test_rejects_live_thunk_trailer(self) -> None:
        profile = analyzer.REV1655_PROFILE
        bad = bytearray(self.live_text)
        trailer = profile.thunk_text_offset + 8
        struct.pack_into(">I", bad, trailer, 0x60000000)
        self.assert_error("live_thunk_trailer_mismatch", text=bytes(bad))

    def test_rejects_unaligned_or_non_system_target(self) -> None:
        profile = analyzer.REV1655_PROFILE
        for target in (0x81000001, 0x82000000, 0x7FFFFFFC):
            with self.subTest(target=hex(target)):
                bad = bytearray(self.live_text)
                lis, addi = encode_target(target)
                struct.pack_into(">II", bad, profile.thunk_text_offset,
                                 lis, addi)
                self.assert_error(
                    "live_thunk_target_out_of_range", text=bytes(bad))

    def test_allows_observed_external_library_zero_target(self) -> None:
        profile = analyzer.REV1655_PROFILE
        live = bytearray(self.live_text)
        lis, addi = encode_target(0x91F06F28)
        struct.pack_into(">II", live, profile.thunk_text_offset, lis, addi)
        report = analyzer.analyze_image(
            self.pe, self.header, bytes(live), self.profile)
        self.assertEqual(
            report["thunks"]["slots"][0]["target"], "0x91F06F28")

    def test_rejects_target_owned_by_the_wrong_library(self) -> None:
        profile = analyzer.REV1655_PROFILE
        live = bytearray(self.live_text)
        lis, addi = encode_target(0x80061440)
        struct.pack_into(">II", live, profile.thunk_text_offset, lis, addi)
        self.assert_error("live_thunk_target_out_of_range", text=bytes(live))

    def test_cli_failure_is_machine_readable_and_nonzero(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pe_path = root / "Aurora.exe"
            header_path = root / "header.bin"
            text_path = root / "text.bin"
            pe_path.write_bytes(self.pe)
            header_path.write_bytes(self.header[:-1])
            text_path.write_bytes(self.live_text)
            output = io.StringIO()
            with redirect_stdout(output):
                status = analyzer.main([
                    "--pe", str(pe_path),
                    "--header", str(header_path),
                    "--text", str(text_path),
                ], self.profile)
        self.assertEqual(status, 2)
        parsed = json.loads(output.getvalue())
        self.assertFalse(parsed["compatible"])
        self.assertEqual(
            parsed["error"]["code"], "live_header_size_mismatch")


if __name__ == "__main__":
    unittest.main()
