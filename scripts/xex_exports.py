#!/usr/bin/env python3
"""Build and validate the ordinal-only XEX export table used by AuroraAZ.

SynthXEX v0.0.6 notices PE exports but does not emit the Xbox 360's
``HvImageExportTable`` or its security-header pointer.  This tool fills a
dedicated, already-mapped ``.xexexp`` PE section before SynthXEX hashes the
image, then sets only the security-header pointer after packaging.

The implementation intentionally supports one narrow contract: a PE32 Xbox
360 DLL, a contiguous set of ordinal-only function exports, an executable
read-only reserve section, and an unencrypted/uncompressed SynthXEX image.
Anything else fails closed.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import struct
import sys
from dataclasses import dataclass
from typing import Iterable, Sequence


XEX_EXPORT_MAGIC = (0x48000000, 0x00485645, 0x48000000)
XEX_EXPORT_FIXED_WORDS = 11
XEX_EXPORT_FIXED_SIZE = XEX_EXPORT_FIXED_WORDS * 4
DEFAULT_SECTION_NAME = ".xexexp"
DEFAULT_MODULE_FLAGS = 0x0000000A  # system DLL: exports-to-title | DLL
PE_MACHINE_POWERPCBE = 0x01F2
PE_MAGIC_32 = 0x010B
PE_SECTION_INITIALIZED_DATA = 0x00000040
PE_SECTION_EXECUTE = 0x20000000
PE_SECTION_READ = 0x40000000
PE_SECTION_WRITE = 0x80000000
XEX_BASEFILE_FORMAT_ID = 0x000003FF
XEX_IMAGE_PAGE_SIZE_4K = 0x10000000
XEX_SECTION_CODE = 1


class ExportFormatError(ValueError):
    """An input does not satisfy the deliberately narrow export contract."""


def _need(data: bytes | bytearray, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ExportFormatError(
            f"{label} is outside the file: offset=0x{offset:X}, size=0x{size:X}"
        )


def _u16le(data: bytes | bytearray, offset: int, label: str) -> int:
    _need(data, offset, 2, label)
    return struct.unpack_from("<H", data, offset)[0]


def _u32le(data: bytes | bytearray, offset: int, label: str) -> int:
    _need(data, offset, 4, label)
    return struct.unpack_from("<I", data, offset)[0]


def _u32be(data: bytes | bytearray, offset: int, label: str) -> int:
    _need(data, offset, 4, label)
    return struct.unpack_from(">I", data, offset)[0]


def _parse_ordinals(text: str | Iterable[int]) -> tuple[int, ...]:
    if isinstance(text, str):
        try:
            values = tuple(int(value.strip(), 0) for value in text.split(","))
        except ValueError as exc:
            raise ExportFormatError(f"invalid ordinal list: {text!r}") from exc
    else:
        values = tuple(int(value) for value in text)

    if not values:
        raise ExportFormatError("at least one export ordinal is required")
    if any(value <= 0 or value > 0xFFFF for value in values):
        raise ExportFormatError(f"export ordinals must be in 1..65535: {values}")
    if tuple(sorted(set(values))) != values:
        raise ExportFormatError(f"export ordinals must be unique and sorted: {values}")
    if values != tuple(range(values[0], values[-1] + 1)):
        raise ExportFormatError(f"export ordinals must be contiguous: {values}")
    return values


@dataclass(frozen=True)
class PeSection:
    name: str
    virtual_size: int
    virtual_address: int
    raw_size: int
    raw_offset: int
    characteristics: int

    def maps_raw(self, rva: int, size: int) -> bool:
        if rva < self.virtual_address:
            return False
        delta = rva - self.virtual_address
        return delta + size <= self.raw_size

    def contains_virtual(self, rva: int, size: int = 1) -> bool:
        span = max(self.virtual_size, self.raw_size)
        return rva >= self.virtual_address and rva + size <= self.virtual_address + span


class PeImage:
    """Small, strict PE32 reader sufficient for the export conversion."""

    def __init__(self, data: bytes | bytearray):
        self.data = data
        _need(data, 0, 0x40, "DOS header")
        if data[:2] != b"MZ":
            raise ExportFormatError("PE does not start with MZ")

        self.pe_offset = _u32le(data, 0x3C, "PE header offset")
        _need(data, self.pe_offset, 24, "PE/COFF header")
        if data[self.pe_offset : self.pe_offset + 4] != b"PE\0\0":
            raise ExportFormatError("PE signature is invalid")

        coff = self.pe_offset + 4
        self.machine = _u16le(data, coff, "PE machine")
        if self.machine != PE_MACHINE_POWERPCBE:
            raise ExportFormatError(
                f"PE machine must be POWERPCBE 0x{PE_MACHINE_POWERPCBE:04X}, "
                f"got 0x{self.machine:04X}"
            )
        section_count = _u16le(data, coff + 2, "PE section count")
        if section_count == 0 or section_count > 96:
            raise ExportFormatError(f"implausible PE section count: {section_count}")
        optional_size = _u16le(data, coff + 16, "PE optional-header size")
        optional = coff + 20
        _need(data, optional, optional_size, "PE optional header")
        if optional_size < 0x68:
            raise ExportFormatError("PE32 optional header is too small")
        if _u16le(data, optional, "PE optional-header magic") != PE_MAGIC_32:
            raise ExportFormatError("only PE32 images are supported")

        self.image_base = _u32le(data, optional + 28, "PE image base")
        self.section_alignment = _u32le(data, optional + 32, "PE section alignment")
        self.file_alignment = _u32le(data, optional + 36, "PE file alignment")
        self.size_of_image = _u32le(data, optional + 56, "PE image size")
        self.size_of_headers = _u32le(data, optional + 60, "PE header size")
        directory_count = _u32le(data, optional + 92, "PE data-directory count")
        if directory_count < 1 or optional_size < 104:
            raise ExportFormatError("PE has no export data-directory entry")
        self.export_rva = _u32le(data, optional + 96, "PE export-directory RVA")
        self.export_size = _u32le(data, optional + 100, "PE export-directory size")
        if self.export_rva == 0 or self.export_size < 40:
            raise ExportFormatError("PE export directory is absent or truncated")

        table = optional + optional_size
        _need(data, table, section_count * 40, "PE section table")
        sections: list[PeSection] = []
        for index in range(section_count):
            entry = table + index * 40
            raw_name = bytes(data[entry : entry + 8]).split(b"\0", 1)[0]
            try:
                name = raw_name.decode("ascii")
            except UnicodeDecodeError as exc:
                raise ExportFormatError(f"section {index} name is not ASCII") from exc
            section = PeSection(
                name=name,
                virtual_size=_u32le(data, entry + 8, f"{name} virtual size"),
                virtual_address=_u32le(data, entry + 12, f"{name} virtual address"),
                raw_size=_u32le(data, entry + 16, f"{name} raw size"),
                raw_offset=_u32le(data, entry + 20, f"{name} raw offset"),
                characteristics=_u32le(data, entry + 36, f"{name} characteristics"),
            )
            _need(data, section.raw_offset, section.raw_size, f"{name} raw data")
            sections.append(section)
        self.sections = tuple(sections)

    def section_named(self, name: str) -> PeSection:
        matches = [section for section in self.sections if section.name == name]
        if len(matches) != 1:
            raise ExportFormatError(
                f"PE must contain exactly one {name!r} section, found {len(matches)}"
            )
        return matches[0]

    def section_for_rva(self, rva: int, size: int = 1) -> PeSection:
        matches = [section for section in self.sections if section.contains_virtual(rva, size)]
        if len(matches) != 1:
            raise ExportFormatError(
                f"RVA 0x{rva:X} (size 0x{size:X}) does not map to exactly one section"
            )
        return matches[0]

    def raw_offset_for_rva(self, rva: int, size: int, label: str) -> int:
        matches = [section for section in self.sections if section.maps_raw(rva, size)]
        if len(matches) != 1:
            raise ExportFormatError(
                f"{label} RVA 0x{rva:X} (size 0x{size:X}) is not raw-backed by one section"
            )
        section = matches[0]
        return section.raw_offset + (rva - section.virtual_address)

    def function_exports(self, ordinals: Sequence[int]) -> tuple[int, ...]:
        ordinals = _parse_ordinals(ordinals)
        directory_offset = self.raw_offset_for_rva(
            self.export_rva, 40, "PE export directory"
        )
        fields = struct.unpack_from("<IIHHIIIIIII", self.data, directory_offset)
        ordinal_base = fields[5]
        function_count = fields[6]
        name_count = fields[7]
        function_table_rva = fields[8]

        if name_count != 0:
            raise ExportFormatError(
                f"exports must be ordinal-only (NONAME), found {name_count} named exports"
            )
        if ordinal_base != ordinals[0] or function_count != len(ordinals):
            raise ExportFormatError(
                "PE export span does not match required ordinals: "
                f"base={ordinal_base}, count={function_count}, required={ordinals}"
            )
        table_offset = self.raw_offset_for_rva(
            function_table_rva, function_count * 4, "PE export address table"
        )
        functions = tuple(
            _u32le(self.data, table_offset + index * 4, f"export ordinal {ordinal}")
            for index, ordinal in enumerate(ordinals)
        )
        for ordinal, rva in zip(ordinals, functions):
            if rva == 0:
                raise ExportFormatError(f"export ordinal {ordinal} has a null RVA")
            if self.export_rva <= rva < self.export_rva + self.export_size:
                raise ExportFormatError(f"export ordinal {ordinal} is a forwarder")
            section = self.section_for_rva(rva)
            if not section.characteristics & PE_SECTION_EXECUTE:
                raise ExportFormatError(
                    f"export ordinal {ordinal} RVA 0x{rva:X} is not executable"
                )
        return functions


def build_xex_export_table(
    image_base: int, ordinals: Sequence[int], function_rvas: Sequence[int]
) -> bytes:
    ordinals = _parse_ordinals(ordinals)
    if len(function_rvas) != len(ordinals):
        raise ExportFormatError("function RVA count does not match ordinal count")
    if image_base & 0xFFFF:
        raise ExportFormatError(
            f"XEX export format requires a 64-KiB-aligned image base, got 0x{image_base:08X}"
        )
    header = (
        *XEX_EXPORT_MAGIC,
        0,
        0,  # module number (two words)
        0,
        0,
        0,  # version (three words)
        image_base >> 16,
        len(ordinals),
        ordinals[0],
    )
    return struct.pack(">11I", *header) + struct.pack(
        f">{len(function_rvas)}I", *function_rvas
    )


@dataclass(frozen=True)
class PreparedPe:
    image_base: int
    image_size: int
    reserve_rva: int
    ordinals: tuple[int, ...]
    function_rvas: tuple[int, ...]
    table: bytes


def prepare_pe_bytes(
    data: bytes | bytearray,
    ordinals: Sequence[int],
    section_name: str = DEFAULT_SECTION_NAME,
) -> tuple[bytes, PreparedPe]:
    ordinals = _parse_ordinals(ordinals)
    output = bytearray(data)
    pe = PeImage(output)
    functions = pe.function_exports(ordinals)
    table = build_xex_export_table(pe.image_base, ordinals, functions)
    reserve = pe.section_named(section_name)

    required_flags = PE_SECTION_INITIALIZED_DATA | PE_SECTION_EXECUTE | PE_SECTION_READ
    if reserve.characteristics & required_flags != required_flags:
        raise ExportFormatError(
            f"{section_name} must be initialized, executable, and readable; "
            f"characteristics=0x{reserve.characteristics:08X}"
        )
    if reserve.characteristics & PE_SECTION_WRITE:
        raise ExportFormatError(f"{section_name} must not be writable")
    if reserve.virtual_size != len(table):
        raise ExportFormatError(
            f"{section_name} virtual size must be exactly 0x{len(table):X}, "
            f"got 0x{reserve.virtual_size:X}"
        )
    if reserve.raw_size < len(table):
        raise ExportFormatError(f"{section_name} raw backing is too small")
    if reserve.virtual_address + len(table) > pe.size_of_image:
        raise ExportFormatError(f"{section_name} extends past PE SizeOfImage")

    reserve_offset = reserve.raw_offset
    original = bytes(output[reserve_offset : reserve_offset + len(table)])
    if original not in (bytes(len(table)), table):
        raise ExportFormatError(
            f"{section_name} is neither a zero reserve nor the expected export table"
        )
    output[reserve_offset : reserve_offset + len(table)] = table
    prepared = PreparedPe(
        image_base=pe.image_base,
        image_size=pe.size_of_image,
        reserve_rva=reserve.virtual_address,
        ordinals=ordinals,
        function_rvas=functions,
        table=table,
    )
    return bytes(output), prepared


def inspect_prepared_pe(
    data: bytes | bytearray,
    ordinals: Sequence[int],
    section_name: str = DEFAULT_SECTION_NAME,
) -> PreparedPe:
    prepared_bytes, prepared = prepare_pe_bytes(data, ordinals, section_name)
    if prepared_bytes != bytes(data):
        raise ExportFormatError(
            f"{section_name} is still zero; run prepare-pe before SynthXEX"
        )
    return prepared


@dataclass(frozen=True)
class XexLayout:
    module_flags: int
    pe_offset: int
    security_offset: int
    image_size: int
    image_flags: int
    image_base: int
    export_pointer: int
    page_count: int
    page_size: int
    descriptors_offset: int


def _parse_xex(
    data: bytes | bytearray,
    prepared: PreparedPe,
    module_flags: int,
) -> XexLayout:
    _need(data, 0, 24, "XEX header")
    if data[:4] != b"XEX2":
        raise ExportFormatError("XEX does not start with XEX2")
    actual_flags = _u32be(data, 4, "XEX module flags")
    if actual_flags != module_flags:
        raise ExportFormatError(
            f"XEX module flags must be 0x{module_flags:08X}, got 0x{actual_flags:08X}"
        )
    pe_offset = _u32be(data, 8, "XEX basefile offset")
    security = _u32be(data, 16, "XEX security-info offset")
    optional_count = _u32be(data, 20, "XEX optional-header count")
    if optional_count == 0 or optional_count > 256:
        raise ExportFormatError(f"implausible XEX optional-header count: {optional_count}")
    _need(data, 24, optional_count * 8, "XEX optional-header entries")
    optional_entries: dict[int, int] = {}
    previous_id = -1
    for index in range(optional_count):
        entry = 24 + index * 8
        header_id = _u32be(data, entry, f"XEX optional header {index} ID")
        value = _u32be(data, entry + 4, f"XEX optional header {index} value")
        if header_id <= previous_id:
            raise ExportFormatError("XEX optional headers are not strictly ordered")
        if header_id in optional_entries:
            raise ExportFormatError(f"duplicate XEX optional header 0x{header_id:X}")
        optional_entries[header_id] = value
        previous_id = header_id
    if XEX_BASEFILE_FORMAT_ID not in optional_entries:
        raise ExportFormatError("XEX basefile-format optional header is absent")

    bff = optional_entries[XEX_BASEFILE_FORMAT_ID]
    _need(data, bff, 16, "XEX basefile-format header")
    bff_size, encryption, compression, data_size, zero_size = struct.unpack_from(
        ">IHHII", data, bff
    )
    if (bff_size, encryption, compression, zero_size) != (16, 0, 1, 0):
        raise ExportFormatError(
            "only SynthXEX unencrypted/uncompressed basefiles are supported; "
            f"got size={bff_size}, encryption={encryption}, "
            f"compression={compression}, zero_size={zero_size}"
        )

    _need(data, security, 0x184, "XEX security info")
    security_size = _u32be(data, security, "XEX security-info size")
    image_size = _u32be(data, security + 4, "XEX image size")
    image_info_size = _u32be(data, security + 0x108, "XEX image-info size")
    image_flags = _u32be(data, security + 0x10C, "XEX image flags")
    image_base = _u32be(data, security + 0x110, "XEX image base")
    export_pointer = _u32be(data, security + 0x160, "XEX export-table pointer")
    page_count = _u32be(data, security + 0x180, "XEX page-descriptor count")
    if image_info_size != 0x174:
        raise ExportFormatError(f"unexpected XEX image-info size: 0x{image_info_size:X}")
    if image_size != data_size or image_size != prepared.image_size:
        raise ExportFormatError(
            f"XEX/PE image-size mismatch: XEX=0x{image_size:X}, "
            f"basefile=0x{data_size:X}, PE=0x{prepared.image_size:X}"
        )
    if image_base != prepared.image_base:
        raise ExportFormatError(
            f"XEX/PE image-base mismatch: 0x{image_base:08X} != "
            f"0x{prepared.image_base:08X}"
        )
    if page_count == 0 or image_size % page_count:
        raise ExportFormatError(
            f"invalid XEX page geometry: size=0x{image_size:X}, count={page_count}"
        )
    page_size = image_size // page_count
    if page_size not in (0x1000, 0x10000):
        raise ExportFormatError(f"unsupported XEX page size: 0x{page_size:X}")
    flagged_page_size = (
        0x1000 if image_flags & XEX_IMAGE_PAGE_SIZE_4K else 0x10000
    )
    if page_size != flagged_page_size:
        raise ExportFormatError(
            "XEX image-flags/page-geometry mismatch: "
            f"flags=0x{image_flags:08X}, derived page size=0x{page_size:X}"
        )
    descriptors = security + 0x184
    expected_security_size = 0x184 + page_count * 0x18
    if security_size != expected_security_size:
        raise ExportFormatError(
            f"security-info size mismatch: 0x{security_size:X} != "
            f"0x{expected_security_size:X}"
        )
    _need(data, descriptors, page_count * 0x18, "XEX page descriptors")
    _need(data, pe_offset, image_size, "XEX mapped basefile")
    if data[pe_offset : pe_offset + 2] != b"MZ":
        raise ExportFormatError("XEX mapped basefile does not start with MZ")

    return XexLayout(
        module_flags=actual_flags,
        pe_offset=pe_offset,
        security_offset=security,
        image_size=image_size,
        image_flags=image_flags,
        image_base=image_base,
        export_pointer=export_pointer,
        page_count=page_count,
        page_size=page_size,
        descriptors_offset=descriptors,
    )


def _validate_page_hashes(data: bytes | bytearray, layout: XexLayout) -> None:
    security = layout.security_offset
    image = data[layout.pe_offset : layout.pe_offset + layout.image_size]
    for index in range(layout.page_count - 1, -1, -1):
        descriptor = layout.descriptors_offset + index * 0x18
        page = image[index * layout.page_size : (index + 1) * layout.page_size]
        digest = hashlib.sha1(page + data[descriptor : descriptor + 0x18]).digest()
        if index == 0:
            stored = bytes(data[security + 0x114 : security + 0x128])
            label = "security image hash"
        else:
            previous = layout.descriptors_offset + (index - 1) * 0x18
            stored = bytes(data[previous + 4 : previous + 0x18])
            label = f"page descriptor {index - 1} chained hash"
        if digest != stored:
            raise ExportFormatError(f"{label} does not match mapped image data")
    last = layout.descriptors_offset + (layout.page_count - 1) * 0x18
    if any(data[last + 4 : last + 0x18]):
        raise ExportFormatError("last XEX page-descriptor chain value is not zero")


def _validate_header_hash(data: bytes | bytearray, layout: XexLayout) -> None:
    security = layout.security_offset
    end_image_info = security + 0x17C
    if end_image_info > layout.pe_offset:
        raise ExportFormatError("XEX image-info overlaps the basefile")
    expected = hashlib.sha1(
        data[end_image_info : layout.pe_offset] + data[: security + 8]
    ).digest()
    stored = bytes(data[security + 0x164 : security + 0x178])
    if expected != stored:
        raise ExportFormatError("XEX header hash does not match the header bytes")


def validate_xex_bytes(
    data: bytes | bytearray,
    prepared_pe_data: bytes | bytearray,
    ordinals: Sequence[int],
    section_name: str = DEFAULT_SECTION_NAME,
    module_flags: int = DEFAULT_MODULE_FLAGS,
) -> XexLayout:
    prepared = inspect_prepared_pe(prepared_pe_data, ordinals, section_name)
    layout = _parse_xex(data, prepared, module_flags)
    expected_pointer = prepared.image_base + prepared.reserve_rva
    if layout.export_pointer != expected_pointer:
        raise ExportFormatError(
            f"XEX export pointer must be 0x{expected_pointer:08X}, "
            f"got 0x{layout.export_pointer:08X}"
        )
    table_offset = layout.pe_offset + prepared.reserve_rva
    _need(data, table_offset, len(prepared.table), "mapped XEX export table")
    table = bytes(data[table_offset : table_offset + len(prepared.table)])
    if table != prepared.table:
        raise ExportFormatError("mapped XEX export table differs from the prepared PE")

    words = struct.unpack(f">{XEX_EXPORT_FIXED_WORDS + len(prepared.ordinals)}I", table)
    if tuple(words[:3]) != XEX_EXPORT_MAGIC:
        raise ExportFormatError("XEX export-table magic is invalid")
    if any(words[3:8]):
        raise ExportFormatError("XEX export module/version words must be zero")
    if words[8] != prepared.image_base >> 16:
        raise ExportFormatError("XEX export image-base word is invalid")
    if words[9] != len(prepared.ordinals) or words[10] != prepared.ordinals[0]:
        raise ExportFormatError("XEX export count/base fields are invalid")
    if tuple(words[11:]) != prepared.function_rvas:
        raise ExportFormatError("XEX export function offsets are invalid")

    for ordinal, function_rva in zip(
        prepared.ordinals, prepared.function_rvas
    ):
        if function_rva + 4 > layout.image_size:
            raise ExportFormatError(
                f"export ordinal {ordinal} RVA 0x{function_rva:X} "
                "is outside the XEX image"
            )
        function_page = function_rva // layout.page_size
        function_descriptor = (
            layout.descriptors_offset + function_page * 0x18
        )
        function_info = _u32be(
            data,
            function_descriptor,
            f"XEX export ordinal {ordinal} page descriptor",
        )
        if function_info & 0xF != XEX_SECTION_CODE:
            raise ExportFormatError(
                f"export ordinal {ordinal} must resolve to a code page, "
                f"descriptor=0x{function_info:08X}"
            )

    reserve_page = prepared.reserve_rva // layout.page_size
    if reserve_page >= layout.page_count:
        raise ExportFormatError("XEX export reserve page is outside the image")
    reserve_descriptor = layout.descriptors_offset + reserve_page * 0x18
    reserve_info = _u32be(data, reserve_descriptor, "XEX export page descriptor")
    if reserve_info & 0xF != XEX_SECTION_CODE:
        raise ExportFormatError(
            f"XEX export table must be on a code page, descriptor=0x{reserve_info:08X}"
        )

    _validate_page_hashes(data, layout)
    _validate_header_hash(data, layout)
    return layout


def finalize_xex_bytes(
    data: bytes | bytearray,
    prepared_pe_data: bytes | bytearray,
    ordinals: Sequence[int],
    section_name: str = DEFAULT_SECTION_NAME,
    module_flags: int = DEFAULT_MODULE_FLAGS,
) -> bytes:
    prepared = inspect_prepared_pe(prepared_pe_data, ordinals, section_name)
    output = bytearray(data)
    layout = _parse_xex(output, prepared, module_flags)
    expected_pointer = prepared.image_base + prepared.reserve_rva
    if layout.export_pointer not in (0, expected_pointer):
        raise ExportFormatError(
            f"refusing to replace existing XEX export pointer "
            f"0x{layout.export_pointer:08X}"
        )
    struct.pack_into(">I", output, layout.security_offset + 0x160, expected_pointer)
    validate_xex_bytes(output, prepared_pe_data, ordinals, section_name, module_flags)
    return bytes(output)


def _atomic_write(path: Path, data: bytes) -> None:
    mode = path.stat().st_mode
    temporary = path.with_name(path.name + ".xexexports.tmp")
    try:
        temporary.write_bytes(data)
        os.chmod(temporary, mode)
        os.replace(temporary, path)
    finally:
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass


def _prepared_summary(prepared: PreparedPe) -> str:
    exports = ", ".join(
        f"{ordinal}=+0x{rva:X}"
        for ordinal, rva in zip(prepared.ordinals, prepared.function_rvas)
    )
    return (
        f"base=0x{prepared.image_base:08X}, reserve=+0x{prepared.reserve_rva:X}, "
        f"exports=[{exports}]"
    )


def _add_common_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--pe", required=True, type=Path, help="linked PE32 DLL")
    parser.add_argument(
        "--ordinals", default="2,3,4,5", help="required contiguous ordinals"
    )
    parser.add_argument(
        "--section", default=DEFAULT_SECTION_NAME, help="dedicated PE reserve section"
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    prepare_parser = subparsers.add_parser(
        "prepare-pe", help="fill the mapped PE export-table reserve"
    )
    _add_common_arguments(prepare_parser)
    finalize_parser = subparsers.add_parser(
        "finalize-xex", help="set the XEX security export pointer and validate"
    )
    _add_common_arguments(finalize_parser)
    finalize_parser.add_argument("--xex", required=True, type=Path)
    finalize_parser.add_argument(
        "--module-flags", type=lambda value: int(value, 0), default=DEFAULT_MODULE_FLAGS
    )
    validate_parser = subparsers.add_parser(
        "validate", help="validate the complete prepared PE and packaged XEX"
    )
    _add_common_arguments(validate_parser)
    validate_parser.add_argument("--xex", required=True, type=Path)
    validate_parser.add_argument(
        "--module-flags", type=lambda value: int(value, 0), default=DEFAULT_MODULE_FLAGS
    )
    args = parser.parse_args(argv)

    try:
        ordinals = _parse_ordinals(args.ordinals)
        pe_data = args.pe.read_bytes()
        if args.command == "prepare-pe":
            output, prepared = prepare_pe_bytes(pe_data, ordinals, args.section)
            _atomic_write(args.pe, output)
            print(f"prepared PE XEX exports: {_prepared_summary(prepared)}")
            return 0
        if args.command == "finalize-xex":
            xex_data = args.xex.read_bytes()
            output = finalize_xex_bytes(
                xex_data,
                pe_data,
                ordinals,
                args.section,
                args.module_flags,
            )
            _atomic_write(args.xex, output)
            prepared = inspect_prepared_pe(pe_data, ordinals, args.section)
            print(f"finalized and validated XEX exports: {_prepared_summary(prepared)}")
            return 0
        if args.command == "validate":
            xex_data = args.xex.read_bytes()
            validate_xex_bytes(
                xex_data,
                pe_data,
                ordinals,
                args.section,
                args.module_flags,
            )
            prepared = inspect_prepared_pe(pe_data, ordinals, args.section)
            print(f"validated XEX exports: {_prepared_summary(prepared)}")
            return 0
        raise AssertionError(f"unhandled command: {args.command}")
    except (ExportFormatError, OSError) as exc:
        print(f"xex_exports.py: error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
