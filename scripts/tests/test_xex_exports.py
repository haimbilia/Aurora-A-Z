from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "xex_exports.py"
SPEC = importlib.util.spec_from_file_location("auroraaz_xex_exports", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
xex_exports = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = xex_exports
SPEC.loader.exec_module(xex_exports)


ORDINALS = (2, 3, 4, 5)
FUNCTION_RVAS = (0x1010, 0x1020, 0x1030, 0x1040)
IMAGE_BASE = 0x91D00000


def make_pe() -> bytes:
    data = bytearray(0x800)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    pe = 0x80
    data[pe : pe + 4] = b"PE\0\0"
    coff = pe + 4
    struct.pack_into("<HHIIIHH", data, coff, 0x01F2, 3, 0, 0, 0, 0xE0, 0x2002)
    optional = coff + 20
    struct.pack_into("<H", data, optional, 0x10B)
    struct.pack_into("<I", data, optional + 16, 0x1000)
    struct.pack_into("<I", data, optional + 20, 0x1000)
    struct.pack_into("<I", data, optional + 24, 0x2000)
    struct.pack_into("<I", data, optional + 28, IMAGE_BASE)
    struct.pack_into("<I", data, optional + 32, 0x1000)
    struct.pack_into("<I", data, optional + 36, 0x200)
    struct.pack_into("<I", data, optional + 56, 0x4000)
    struct.pack_into("<I", data, optional + 60, 0x200)
    struct.pack_into("<H", data, optional + 68, 0xE)
    struct.pack_into("<I", data, optional + 92, 16)
    struct.pack_into("<II", data, optional + 96, 0x2000, 0x60)

    section_table = optional + 0xE0
    sections = (
        (b".text", 0x100, 0x1000, 0x200, 0x200, 0x60000020),
        (b".rdata", 0x100, 0x2000, 0x200, 0x400, 0x40000040),
        (b".xexexp", 0x3C, 0x3000, 0x200, 0x600, 0x60000040),
    )
    for index, (name, vsize, rva, raw_size, raw, flags) in enumerate(sections):
        entry = section_table + index * 40
        data[entry : entry + len(name)] = name
        struct.pack_into("<IIII", data, entry + 8, vsize, rva, raw_size, raw)
        struct.pack_into("<I", data, entry + 36, flags)

    data[0x200 : 0x208] = b"\x60\x00\x00\x00" * 2
    export = 0x400
    struct.pack_into(
        "<IIHHIIIIIII",
        data,
        export,
        0,
        0,
        0,
        0,
        0x2028,
        2,
        4,
        0,
        0x2040,
        0,
        0,
    )
    data[export + 0x28 : export + 0x35] = b"AuroraAZ.xex\0"
    struct.pack_into("<4I", data, export + 0x40, *FUNCTION_RVAS)
    return bytes(data)


def map_pe(pe_data: bytes) -> bytes:
    pe = xex_exports.PeImage(pe_data)
    mapped = bytearray(pe.size_of_image)
    mapped[: pe.size_of_headers] = pe_data[: pe.size_of_headers]
    for section in pe.sections:
        mapped[
            section.virtual_address : section.virtual_address + section.raw_size
        ] = pe_data[section.raw_offset : section.raw_offset + section.raw_size]
    return bytes(mapped)


def make_xex(prepared_pe: bytes) -> bytes:
    mapped = map_pe(prepared_pe)
    security = 0x40
    basefile = 0x1000
    bff = 0x300
    page_size = 0x1000
    page_count = len(mapped) // page_size
    descriptors = security + 0x184
    data = bytearray(basefile + len(mapped))
    data[:4] = b"XEX2"
    struct.pack_into(">IIIII", data, 4, 0x9, basefile, 0, security, 2)
    struct.pack_into(">II", data, 24, 0x3FF, bff)
    struct.pack_into(">II", data, 32, 0x10201, IMAGE_BASE)
    struct.pack_into(">II", data, security, 0x184 + page_count * 0x18, len(mapped))
    data[security + 8 : security + 8 + 16] = b"Synthetic test\0\0"
    struct.pack_into(">I", data, security + 0x108, 0x174)
    struct.pack_into(">I", data, security + 0x10C, 0x10000000)
    struct.pack_into(">I", data, security + 0x110, IMAGE_BASE)
    struct.pack_into(">I", data, security + 0x178, 0xFFFFFFFF)
    struct.pack_into(">I", data, security + 0x17C, 0xFFFFFFFF)
    struct.pack_into(">I", data, security + 0x180, page_count)
    descriptor_values = (0x13, 0x11, 0x13, 0x11)
    for index, value in enumerate(descriptor_values):
        struct.pack_into(">I", data, descriptors + index * 0x18, value)
    struct.pack_into(">IHHII", data, bff, 16, 0, 1, len(mapped), 0)
    data[basefile : basefile + len(mapped)] = mapped

    for index in range(page_count - 1, -1, -1):
        descriptor = descriptors + index * 0x18
        page = mapped[index * page_size : (index + 1) * page_size]
        digest = hashlib.sha1(page + data[descriptor : descriptor + 0x18]).digest()
        if index:
            previous = descriptors + (index - 1) * 0x18
            data[previous + 4 : previous + 0x18] = digest
        else:
            data[security + 0x114 : security + 0x128] = digest
    header_hash = hashlib.sha1(
        data[security + 0x17C : basefile] + data[: security + 8]
    ).digest()
    data[security + 0x164 : security + 0x178] = header_hash
    return bytes(data)


class XexExportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.pe, self.prepared = xex_exports.prepare_pe_bytes(make_pe(), ORDINALS)

    def test_prepare_builds_stock_shape_table(self) -> None:
        self.assertEqual(self.prepared.reserve_rva, 0x3000)
        self.assertEqual(self.prepared.function_rvas, FUNCTION_RVAS)
        words = struct.unpack(">15I", self.prepared.table)
        self.assertEqual(words[:3], xex_exports.XEX_EXPORT_MAGIC)
        self.assertEqual(words[3:8], (0, 0, 0, 0, 0))
        self.assertEqual(words[8:11], (0x91D0, 4, 2))
        self.assertEqual(words[11:], FUNCTION_RVAS)

    def test_prepare_is_idempotent(self) -> None:
        second, info = xex_exports.prepare_pe_bytes(self.pe, ORDINALS)
        self.assertEqual(second, self.pe)
        self.assertEqual(info, self.prepared)

    def test_prepare_rejects_named_exports(self) -> None:
        bad = bytearray(make_pe())
        struct.pack_into("<I", bad, 0x400 + 24, 1)
        with self.assertRaisesRegex(xex_exports.ExportFormatError, "ordinal-only"):
            xex_exports.prepare_pe_bytes(bad, ORDINALS)

    def test_prepare_rejects_writable_reserve(self) -> None:
        bad = bytearray(make_pe())
        section_table = 0x80 + 4 + 20 + 0xE0
        flags = struct.unpack_from("<I", bad, section_table + 2 * 40 + 36)[0]
        struct.pack_into("<I", bad, section_table + 2 * 40 + 36, flags | 0x80000000)
        with self.assertRaisesRegex(xex_exports.ExportFormatError, "must not be writable"):
            xex_exports.prepare_pe_bytes(bad, ORDINALS)

    def test_finalize_and_validate_synthetic_xex(self) -> None:
        xex = make_xex(self.pe)
        security = 0x40
        before_hash = xex[security + 0x164 : security + 0x178]
        finalized = xex_exports.finalize_xex_bytes(xex, self.pe, ORDINALS)
        self.assertEqual(
            struct.unpack_from(">I", finalized, security + 0x160)[0],
            IMAGE_BASE + 0x3000,
        )
        self.assertEqual(finalized[security + 0x164 : security + 0x178], before_hash)
        xex_exports.validate_xex_bytes(finalized, self.pe, ORDINALS)

    def test_validator_rejects_mapped_table_tamper(self) -> None:
        finalized = bytearray(
            xex_exports.finalize_xex_bytes(make_xex(self.pe), self.pe, ORDINALS)
        )
        finalized[0x1000 + 0x3000] ^= 1
        with self.assertRaisesRegex(xex_exports.ExportFormatError, "differs"):
            xex_exports.validate_xex_bytes(finalized, self.pe, ORDINALS)

    def test_validator_rejects_page_hash_tamper(self) -> None:
        finalized = bytearray(
            xex_exports.finalize_xex_bytes(make_xex(self.pe), self.pe, ORDINALS)
        )
        finalized[0x40 + 0x184 + 4] ^= 1
        with self.assertRaisesRegex(xex_exports.ExportFormatError, "hash"):
            xex_exports.validate_xex_bytes(finalized, self.pe, ORDINALS)

    def test_validator_rejects_image_flag_page_size_mismatch(self) -> None:
        finalized = bytearray(
            xex_exports.finalize_xex_bytes(make_xex(self.pe), self.pe, ORDINALS)
        )
        struct.pack_into(">I", finalized, 0x40 + 0x10C, 0)
        with self.assertRaisesRegex(
            xex_exports.ExportFormatError, "image-flags/page-geometry mismatch"
        ):
            xex_exports.validate_xex_bytes(finalized, self.pe, ORDINALS)

    def test_validator_requires_matching_image_base_optional_header(self) -> None:
        finalized = bytearray(
            xex_exports.finalize_xex_bytes(make_xex(self.pe), self.pe, ORDINALS)
        )
        struct.pack_into(">I", finalized, 32 + 4, IMAGE_BASE + 0x10000)
        with self.assertRaisesRegex(
            xex_exports.ExportFormatError,
            "optional-header/security mismatch",
        ):
            xex_exports.validate_xex_bytes(finalized, self.pe, ORDINALS)

    def test_validator_rejects_absent_image_base_optional_header(self) -> None:
        malformed = bytearray(make_xex(self.pe))
        struct.pack_into(">I", malformed, 20, 1)
        with self.assertRaisesRegex(
            xex_exports.ExportFormatError,
            "Image Base Address optional header is absent",
        ):
            xex_exports.finalize_xex_bytes(malformed, self.pe, ORDINALS)

    def test_validator_rejects_synthetic_tls_optional_header(self) -> None:
        malformed = bytearray(make_xex(self.pe))
        struct.pack_into(">I", malformed, 20, 3)
        struct.pack_into(">II", malformed, 40, 0x20104, 0x300)
        with self.assertRaisesRegex(
            xex_exports.ExportFormatError,
            "must not contain a TLS optional header",
        ):
            xex_exports.finalize_xex_bytes(malformed, self.pe, ORDINALS)

    def test_validator_rejects_function_on_non_code_page(self) -> None:
        finalized = bytearray(
            xex_exports.finalize_xex_bytes(make_xex(self.pe), self.pe, ORDINALS)
        )
        function_page_descriptor = 0x40 + 0x184 + 0x18
        struct.pack_into(">I", finalized, function_page_descriptor, 0x13)
        with self.assertRaisesRegex(
            xex_exports.ExportFormatError,
            "export ordinal 2 must resolve to a code page",
        ):
            xex_exports.validate_xex_bytes(finalized, self.pe, ORDINALS)


if __name__ == "__main__":
    unittest.main()
