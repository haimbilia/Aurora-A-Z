#!/usr/bin/env python3
"""Validate Aurora Rev1655 CPU-visible image and resolver evidence.

Aurora's Xbox 360 loader rewrites the first two words of each import thunk at
the very end of .text.  Everything else is immutable.  This tool verifies the
source PE, the captured immutable bytes, and the exact loader rewrite shape,
then reconstructs the original .text before hashing it.  Optional v4 IAT and
resolver artifacts add cross-view comparisons without treating module-name or
owner-pointer observations as proof of ownership.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import sys
from typing import Any, Sequence


SCHEMA = "auroraaz-live-image-analysis-v1"
PE_SIGNATURE = b"PE\0\0"
MZ_SIGNATURE = b"MZ"
THUNK_WORD_0_TAG = 0x01000000
THUNK_WORD_1_TAG = 0x02000000
THUNK_ID_MASK = 0x00FFFFFF
PPC_MTCTR_R11 = 0x7D6903A6
PPC_BCTR = 0x4E800420
PPC_LIS_R11 = 0x3D600000
PPC_LIS_R11_MASK = 0xFFFF0000
PPC_ADDI_R11_R11 = 0x396B0000
PPC_ADDI_R11_R11_MASK = 0xFFFF0000
MAX_DIFF_OFFSETS = 64

# The reviewed Rev1655 IAT has 365 big-endian cells.  Its two zero cells split
# and terminate the library groups.  These 13 xboxkrnl entries have no matching
# executable thunk and are therefore classified only as data imports; their
# live values are retained as observations, not interpreted as code targets.
REV1655_DATA_IMPORT_RVAS = (
    0x670, 0x6C0, 0x770, 0x784, 0x794, 0x7CC, 0x7E4,
    0x7E8, 0x8A0, 0x8AC, 0x8E8, 0x918, 0x928,
)
REV1655_IAT_SEPARATOR_RVAS = (0x660, 0x9B0)

RESOLVER_MAGIC = b"AZRE"
RESOLVER_VERSION = 4
RESOLVER_SIZE = 160
RESOLVER_MODULE_COUNT = 4
RESOLVER_SLOT_COUNT = 2
RESOLVER_MODULES_OFFSET = 24
RESOLVER_MODULE_RECORD_SIZE = 12
RESOLVER_SLOTS_OFFSET = 72
RESOLVER_SLOT_RECORD_SIZE = 44
RESOLVER_STATUS_NOT_CALLED = 0xFFFFFFFF
RESOLVER_STATUS_CAPTURE_COMPLETE = 0x80000000
RESOLVER_MODULE_IDENTITIES = (
    (b"XAM ", "xam.xex"),
    (b"KRNL", "xboxkrnl.exe"),
    (b"LNCH", "launch.xex"),
    (b"NOVA", "Nova.xex"),
)
RESOLVER_SLOT_IDENTITIES = (
    (60, 0x0217, 0x82B661BC, 0x4F0),
    (65, 0x01FC, 0x82B6620C, 0x504),
)


@dataclass(frozen=True)
class SectionLayout:
    name: str
    virtual_size: int
    virtual_address: int
    raw_size: int
    raw_offset: int
    characteristics: int


@dataclass(frozen=True)
class ImageProfile:
    name: str
    pe_size: int
    pe_sha256: str
    header_size: int
    header_sha256: str
    machine: int
    section_count: int
    optional_header_size: int
    characteristics: int
    optional_magic: int
    entry_rva: int
    image_base: int
    section_alignment: int
    file_alignment: int
    image_size: int
    text_rva: int
    text_size: int
    text_raw_size: int
    text_raw_offset: int
    text_sha256: str
    thunk_directory_index: int
    thunk_rva: int
    thunk_slot_size: int
    thunk_slot_count: int
    iat_directory_index: int
    iat_rva: int
    iat_size: int
    sections: tuple[SectionLayout, ...]

    @property
    def thunk_size(self) -> int:
        return self.thunk_slot_size * self.thunk_slot_count

    @property
    def thunk_text_offset(self) -> int:
        return self.thunk_rva - self.text_rva


REV1655_SECTIONS = (
    SectionLayout(".rdata", 0x001BD91C, 0x00000400, 0x001BDA00,
                  0x00000400, 0x40000040),
    SectionLayout(".pdata", 0x000452E8, 0x001BDE00, 0x00045400,
                  0x001BDE00, 0x40000040),
    SectionLayout(".text", 0x009573DC, 0x00210000, 0x00957400,
                  0x00203200, 0x60000020),
    SectionLayout("_TEXT", 0x0000039C, 0x00B67400, 0x00000400,
                  0x00B5A600, 0x60000020),
    SectionLayout(".data", 0x0005B040, 0x00B70000, 0x0003A400,
                  0x00B5AA00, 0xC0000040),
    SectionLayout(".XBMOVIE", 0x0000000C, 0x00BCB200, 0x00000200,
                  0x00B94E00, 0xC0000040),
    SectionLayout(".idata", 0x00000624, 0x00BD0000, 0x00000800,
                  0x00B95000, 0xC0000040),
    SectionLayout(".XBLD", 0x00000170, 0x00BE0000, 0x00000200,
                  0x00B95800, 0x42000040),
    SectionLayout(".reloc", 0x0007E688, 0x00BE0200, 0x0007E800,
                  0x00B95A00, 0x42000040),
)


REV1655_PROFILE = ImageProfile(
    name="Aurora 0.7b.2 Rev1655",
    pe_size=0x00C14200,
    pe_sha256="5bb5baf8df4ccb197241b34935eb400f36c8c20648cc074e2c30fa80add37e3c",
    header_size=0x400,
    header_sha256="5f741cadd089b32b2ef5fcdddfdf668a9e4344ae61df6f3f6e76ff1198236925",
    machine=0x01F2,
    section_count=9,
    optional_header_size=0xE0,
    characteristics=0x0102,
    optional_magic=0x010B,
    entry_rva=0x008050E0,
    image_base=0x82000000,
    section_alignment=0x10000,
    file_alignment=0x200,
    image_size=0x00D47E00,
    text_rva=0x00210000,
    text_size=0x009573DC,
    text_raw_size=0x00957400,
    text_raw_offset=0x00203200,
    text_sha256="ee2fb2eba844ee1c444ad5d10a52d6474bc4cd9fe1b89b06b65b3e92d2a177eb",
    thunk_directory_index=7,
    thunk_rva=0x00B65DFC,
    thunk_slot_size=16,
    thunk_slot_count=350,
    iat_directory_index=12,
    iat_rva=0x00000400,
    iat_size=0x000005B4,
    sections=REV1655_SECTIONS,
)


LIBRARIES = {
    0: "xam.xex",
    1: "xboxkrnl.exe",
}

# The clean v2 capture resolves library 0 mostly in 0x816-0x819 and has two
# observed external targets in the title-module region.  Their ownership is
# not inferred here.  Library 1 resolves in the kernel range.  These are
# accepted address envelopes, not exact function identities; every decoded
# target remains visible in the report for independent comparison with IAT
# and module-ownership evidence.
LIBRARY_TARGET_RANGES = {
    0: ((0x81600000, 0x82000000), (0x90000000, 0xA0000000)),
    1: ((0x80000000, 0x80200000),),
}


class AnalysisError(Exception):
    def __init__(
        self,
        code: str,
        message: str,
        details: dict[str, Any] | None = None,
    ) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.details = details or {}


def _sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


def _hex(value: int, width: int = 8) -> str:
    return f"0x{value:0{width}X}"


def _require(condition: bool, code: str, message: str, **details: Any) -> None:
    if not condition:
        raise AnalysisError(code, message, details)


def _u16le(data: bytes, offset: int) -> int:
    _require(offset >= 0 and offset + 2 <= len(data), "pe_header_truncated",
             "PE field extends beyond the file", offset=_hex(offset))
    return struct.unpack_from("<H", data, offset)[0]


def _u32le(data: bytes, offset: int) -> int:
    _require(offset >= 0 and offset + 4 <= len(data), "pe_header_truncated",
             "PE field extends beyond the file", offset=_hex(offset))
    return struct.unpack_from("<I", data, offset)[0]


def _u32be(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def _difference_summary(reference: bytes, live: bytes) -> dict[str, Any]:
    count = 0
    examples: list[str] = []
    for offset, (expected, actual) in enumerate(zip(reference, live)):
        if expected != actual:
            count += 1
            if len(examples) < MAX_DIFF_OFFSETS:
                examples.append(_hex(offset))
    count += abs(len(reference) - len(live))
    return {
        "equal": count == 0,
        "difference_count": count,
        "difference_offsets": examples,
        "difference_offsets_truncated": count > len(examples),
    }


def _rva_to_file_offset(
    rva: int,
    sections: Sequence[SectionLayout],
    header_size: int,
) -> int:
    if 0 <= rva < header_size:
        return rva
    for section in sections:
        span = max(section.virtual_size, section.raw_size)
        if section.virtual_address <= rva < section.virtual_address + span:
            delta = rva - section.virtual_address
            _require(delta < section.raw_size, "pe_rva_not_file_backed",
                     "required RVA is not backed by raw file data",
                     rva=_hex(rva), section=section.name)
            return section.raw_offset + delta
    raise AnalysisError(
        "pe_rva_unmapped",
        "required RVA is not covered by a PE section",
        {"rva": _hex(rva)},
    )


def _validate_pe(
    pe: bytes,
    profile: ImageProfile,
) -> tuple[bytes, bytes, list[int]]:
    _require(len(pe) == profile.pe_size, "pe_size_mismatch",
             "source PE has the wrong size", expected=profile.pe_size,
             actual=len(pe))
    pe_digest = _sha256(pe)
    _require(pe_digest == profile.pe_sha256, "pe_sha256_mismatch",
             "source PE is not the reviewed Rev1655 image",
             expected=profile.pe_sha256, actual=pe_digest)
    _require(pe[:2] == MZ_SIGNATURE, "pe_bad_dos_signature",
             "source PE is missing the MZ signature")

    nt_offset = _u32le(pe, 0x3C)
    _require(nt_offset == 0xF8, "pe_nt_offset_mismatch",
             "source PE has an unexpected NT-header offset",
             expected=_hex(0xF8), actual=_hex(nt_offset))
    _require(pe[nt_offset:nt_offset + 4] == PE_SIGNATURE,
             "pe_bad_nt_signature", "source PE is missing the PE signature")
    coff = nt_offset + 4
    optional = coff + 20
    machine = _u16le(pe, coff)
    section_count = _u16le(pe, coff + 2)
    optional_size = _u16le(pe, coff + 16)
    characteristics = _u16le(pe, coff + 18)
    observed_coff = (machine, section_count, optional_size, characteristics)
    expected_coff = (profile.machine, profile.section_count,
                     profile.optional_header_size, profile.characteristics)
    _require(observed_coff == expected_coff, "pe_coff_layout_mismatch",
             "source PE COFF identity does not match Rev1655",
             expected=[_hex(value, 4) for value in expected_coff],
             actual=[_hex(value, 4) for value in observed_coff])

    optional_values = (
        _u16le(pe, optional),
        _u32le(pe, optional + 16),
        _u32le(pe, optional + 28),
        _u32le(pe, optional + 32),
        _u32le(pe, optional + 36),
        _u32le(pe, optional + 56),
        _u32le(pe, optional + 60),
        _u32le(pe, optional + 92),
    )
    expected_optional = (
        profile.optional_magic,
        profile.entry_rva,
        profile.image_base,
        profile.section_alignment,
        profile.file_alignment,
        profile.image_size,
        profile.header_size,
        16,
    )
    _require(optional_values == expected_optional,
             "pe_optional_layout_mismatch",
             "source PE optional-header layout does not match Rev1655",
             expected=[_hex(value) for value in expected_optional],
             actual=[_hex(value) for value in optional_values])

    directory_base = optional + 96
    directories: list[tuple[int, int]] = []
    for index in range(16):
        directories.append((
            _u32le(pe, directory_base + index * 8),
            _u32le(pe, directory_base + index * 8 + 4),
        ))
    _require(
        directories[profile.thunk_directory_index] ==
        (profile.thunk_rva, profile.thunk_size),
        "pe_thunk_directory_mismatch",
        "source PE thunk-directory layout does not match Rev1655",
        expected=[_hex(profile.thunk_rva), _hex(profile.thunk_size)],
        actual=[_hex(value) for value in
                directories[profile.thunk_directory_index]],
    )
    _require(
        directories[profile.iat_directory_index] ==
        (profile.iat_rva, profile.iat_size),
        "pe_iat_directory_mismatch",
        "source PE IAT-directory layout does not match Rev1655",
        expected=[_hex(profile.iat_rva), _hex(profile.iat_size)],
        actual=[_hex(value) for value in
                directories[profile.iat_directory_index]],
    )

    section_table = optional + optional_size
    parsed_sections: list[SectionLayout] = []
    for index in range(section_count):
        offset = section_table + index * 40
        _require(offset + 40 <= profile.header_size,
                 "pe_section_table_out_of_bounds",
                 "PE section table extends past the reviewed header")
        name_bytes = pe[offset:offset + 8]
        name = name_bytes.split(b"\0", 1)[0].decode("ascii", "strict")
        parsed_sections.append(SectionLayout(
            name=name,
            virtual_size=_u32le(pe, offset + 8),
            virtual_address=_u32le(pe, offset + 12),
            raw_size=_u32le(pe, offset + 16),
            raw_offset=_u32le(pe, offset + 20),
            characteristics=_u32le(pe, offset + 36),
        ))
    _require(tuple(parsed_sections) == profile.sections,
             "pe_section_layout_mismatch",
             "source PE section table does not match Rev1655")

    text_section = next(
        (section for section in parsed_sections if section.name == ".text"),
        None,
    )
    _require(text_section is not None, "pe_text_section_missing",
             "source PE has no .text section")
    assert text_section is not None
    expected_text_section = (
        profile.text_rva, profile.text_size,
        profile.text_raw_offset, profile.text_raw_size,
    )
    actual_text_section = (
        text_section.virtual_address, text_section.virtual_size,
        text_section.raw_offset, text_section.raw_size,
    )
    _require(actual_text_section == expected_text_section,
             "pe_text_layout_mismatch",
             "source PE .text layout does not match Rev1655")
    _require(profile.text_raw_offset + profile.text_size <= len(pe),
             "pe_text_truncated", "source PE .text bytes are truncated")
    reference_header = pe[:profile.header_size]
    reference_text = pe[
        profile.text_raw_offset:profile.text_raw_offset + profile.text_size
    ]
    header_digest = _sha256(reference_header)
    text_digest = _sha256(reference_text)
    _require(header_digest == profile.header_sha256,
             "pe_header_sha256_mismatch",
             "source PE header hash does not match the reviewed identity",
             expected=profile.header_sha256, actual=header_digest)
    _require(text_digest == profile.text_sha256,
             "pe_text_sha256_mismatch",
             "source PE .text hash does not match the reviewed identity",
             expected=profile.text_sha256, actual=text_digest)

    iat_offset = _rva_to_file_offset(
        profile.iat_rva, parsed_sections, profile.header_size)
    _require(iat_offset + profile.iat_size <= len(pe), "pe_iat_truncated",
             "source PE IAT bytes are truncated")
    _require(profile.iat_size % 4 == 0, "profile_iat_size_invalid",
             "profile IAT size is not word aligned")
    iat_words = [
        _u32be(pe, iat_offset + offset)
        for offset in range(0, profile.iat_size, 4)
    ]
    return reference_header, reference_text, iat_words


def _decode_source_iat(
    iat_words: Sequence[int],
    profile: ImageProfile,
) -> tuple[list[dict[str, Any]], dict[int, dict[str, Any]]]:
    expected_cells = profile.iat_size // 4
    _require(len(iat_words) == expected_cells,
             "source_iat_cell_count_mismatch",
             "source PE IAT has the wrong number of cells",
             expected=expected_cells, actual=len(iat_words))

    data_rvas = set(REV1655_DATA_IMPORT_RVAS)
    separator_rvas = set(REV1655_IAT_SEPARATOR_RVAS)
    _require(data_rvas.isdisjoint(separator_rvas),
             "profile_iat_special_rva_overlap",
             "reviewed data-import and separator RVAs overlap")
    for rva in data_rvas | separator_rvas:
        _require(
            profile.iat_rva <= rva < profile.iat_rva + profile.iat_size and
            (rva - profile.iat_rva) % 4 == 0,
            "profile_iat_special_rva_invalid",
            "reviewed special IAT RVA is outside or misaligned",
            rva=_hex(rva),
        )

    entries: list[dict[str, Any]] = []
    functions: dict[int, dict[str, Any]] = {}
    data_ids: set[int] = set()
    for index, word in enumerate(iat_words):
        rva = profile.iat_rva + index * 4
        if rva in separator_rvas:
            _require(word == 0, "source_iat_separator_mismatch",
                     "reviewed source IAT separator is not zero",
                     iat_index=index, rva=_hex(rva), value=_hex(word))
            entries.append({
                "kind": "separator",
                "index": index,
                "rva": rva,
                "source_word": word,
            })
            continue

        _require(word & ~THUNK_ID_MASK == 0,
                 "source_iat_identifier_encoding_mismatch",
                 "source PE IAT identifier uses unexpected high bits",
                 iat_index=index, rva=_hex(rva), value=_hex(word))
        thunk_id = word & THUNK_ID_MASK
        library_id = (thunk_id >> 16) & 0xFF
        ordinal = thunk_id & 0xFFFF
        _require(thunk_id != 0, "raw_thunk_iat_membership_mismatch",
                 "source PE function-IAT cell has no import identifier",
                 iat_index=index, rva=_hex(rva), iat_occurrences=0)
        _require(library_id in LIBRARIES,
                 "source_iat_identifier_mismatch",
                 "source PE IAT cell has an invalid import identifier",
                 iat_index=index, rva=_hex(rva), value=_hex(word))
        entry = {
            "kind": "data" if rva in data_rvas else "function",
            "index": index,
            "rva": rva,
            "source_word": word,
            "thunk_id": thunk_id,
            "library_id": library_id,
            "ordinal": ordinal,
        }
        entries.append(entry)
        if rva in data_rvas:
            _require(library_id == 1, "source_iat_data_library_mismatch",
                     "reviewed data import is not in the xboxkrnl group",
                     iat_index=index, rva=_hex(rva),
                     library_id=library_id, ordinal=ordinal)
            _require(thunk_id not in data_ids,
                     "source_iat_duplicate_data_identifier",
                     "source PE has a duplicate reviewed data-import identifier",
                     iat_index=index, rva=_hex(rva),
                     thunk_id=_hex(thunk_id, 6))
            data_ids.add(thunk_id)
        else:
            _require(thunk_id not in functions,
                     "source_iat_duplicate_function_identifier",
                     "source PE has a duplicate function-import identifier",
                     iat_index=index, rva=_hex(rva),
                     thunk_id=_hex(thunk_id, 6))
            functions[thunk_id] = entry

    _require(len(functions) == profile.thunk_slot_count,
             "source_iat_function_count_mismatch",
             "source PE function-IAT count does not match the thunk table",
             expected=profile.thunk_slot_count, actual=len(functions))
    _require(len(data_ids) == len(REV1655_DATA_IMPORT_RVAS),
             "source_iat_data_count_mismatch",
             "source PE data-import count does not match Rev1655",
             expected=len(REV1655_DATA_IMPORT_RVAS), actual=len(data_ids))
    overlap = sorted(data_ids.intersection(functions))
    _require(not overlap, "source_iat_data_function_identifier_overlap",
             "source PE uses one import identifier as both data and function",
             thunk_ids=[_hex(value, 6) for value in overlap])
    return entries, functions


def _decode_raw_thunks(
    reference_text: bytes,
    source_iat_functions: dict[int, dict[str, Any]],
    profile: ImageProfile,
) -> list[dict[str, Any]]:
    offset = profile.thunk_text_offset
    _require(offset >= 0 and offset + profile.thunk_size == len(reference_text),
             "raw_thunk_location_mismatch",
             "the reviewed thunk table is not the exact .text suffix",
             text_size=len(reference_text), thunk_offset=_hex(offset),
             thunk_size=profile.thunk_size)
    seen: set[int] = set()
    thunks: list[dict[str, Any]] = []
    for index in range(profile.thunk_slot_count):
        slot_offset = offset + index * profile.thunk_slot_size
        words = tuple(
            _u32be(reference_text, slot_offset + word * 4)
            for word in range(4)
        )
        thunk_id = words[0] & THUNK_ID_MASK
        library_id = (thunk_id >> 16) & 0xFF
        ordinal = thunk_id & 0xFFFF
        valid = (
            words[0] & ~THUNK_ID_MASK == THUNK_WORD_0_TAG and
            words[1] & ~THUNK_ID_MASK == THUNK_WORD_1_TAG and
            (words[1] & THUNK_ID_MASK) == thunk_id and
            words[2] == PPC_MTCTR_R11 and
            words[3] == PPC_BCTR
        )
        _require(valid, "raw_thunk_marker_mismatch",
                 "source PE contains a malformed raw import thunk",
                 slot=index, text_offset=_hex(slot_offset),
                 words=[_hex(word) for word in words])
        _require(thunk_id not in seen, "raw_thunk_duplicate_id",
                 "source PE contains a duplicate import-thunk identifier",
                 slot=index, thunk_id=_hex(thunk_id, 6))
        _require(library_id in LIBRARIES, "raw_thunk_unknown_library",
                 "source PE thunk refers to an unknown import library",
                 slot=index, library_id=library_id, ordinal=ordinal)
        iat_entry = source_iat_functions.get(thunk_id)
        _require(iat_entry is not None,
                 "raw_thunk_iat_membership_mismatch",
                 "source PE thunk identifier has no reviewed function-IAT cell",
                 slot=index, thunk_id=_hex(thunk_id, 6),
                 iat_occurrences=0)
        assert iat_entry is not None
        seen.add(thunk_id)
        thunks.append({
            "index": index,
            "text_offset": _hex(slot_offset),
            "rva": _hex(profile.text_rva + slot_offset),
            "virtual_address": _hex(
                profile.image_base + profile.text_rva + slot_offset),
            "library_id": library_id,
            "library": LIBRARIES[library_id],
            "ordinal": ordinal,
            "thunk_id": _hex(thunk_id, 6),
            "iat_index": int(iat_entry["index"]),
            "iat_rva": _hex(int(iat_entry["rva"])),
            "raw_words": [_hex(word) for word in words],
            "_raw_words": words,
            "_thunk_id": thunk_id,
            "_iat_index": int(iat_entry["index"]),
            "_iat_rva": int(iat_entry["rva"]),
        })
    unused = sorted(set(source_iat_functions).difference(seen))
    _require(not unused, "source_iat_function_without_thunk",
             "source PE function-IAT cell has no executable thunk",
             thunk_ids=[_hex(value, 6) for value in unused[:MAX_DIFF_OFFSETS]],
             truncated=len(unused) > MAX_DIFF_OFFSETS)
    return thunks


def _decode_target(lis_word: int, addi_word: int) -> int:
    high = lis_word & 0xFFFF
    low = addi_word & 0xFFFF
    signed_low = low if low < 0x8000 else low - 0x10000
    return ((high << 16) + signed_low) & 0xFFFFFFFF


def _target_is_allowed(library_id: int, target: int) -> bool:
    return target % 4 == 0 and any(
        start <= target < end
        for start, end in LIBRARY_TARGET_RANGES[library_id]
    )


def _target_policy(library_id: int) -> list[str]:
    return [
        f"{_hex(start)}..{_hex(end)} (exclusive)"
        for start, end in LIBRARY_TARGET_RANGES[library_id]
    ]


def _analyze_live_iat(
    live_iat: bytes,
    source_entries: Sequence[dict[str, Any]],
    raw_thunks: Sequence[dict[str, Any]],
    profile: ImageProfile,
) -> tuple[dict[str, Any], list[int]]:
    _require(len(live_iat) == profile.iat_size,
             "live_iat_size_mismatch", "live IAT has the wrong size",
             expected=profile.iat_size, actual=len(live_iat))
    live_words = [
        _u32be(live_iat, offset)
        for offset in range(0, len(live_iat), 4)
    ]
    thunks_by_id = {
        int(thunk["_thunk_id"]): thunk
        for thunk in raw_thunks
    }
    function_count = 0
    data_imports: list[dict[str, Any]] = []
    separators: list[dict[str, Any]] = []
    for entry in source_entries:
        index = int(entry["index"])
        rva = int(entry["rva"])
        value = live_words[index]
        kind = str(entry["kind"])
        if kind == "separator":
            _require(value == 0, "live_iat_separator_mismatch",
                     "live IAT changed a reviewed zero separator",
                     iat_index=index, rva=_hex(rva),
                     expected=_hex(0), actual=_hex(value))
            separators.append({
                "iat_index": index,
                "rva": _hex(rva),
                "source_value": _hex(int(entry["source_word"])),
                "live_value": _hex(value),
            })
            continue
        if kind == "data":
            library_id = int(entry["library_id"])
            data_imports.append({
                "iat_index": index,
                "rva": _hex(rva),
                "library": LIBRARIES[library_id],
                "ordinal": int(entry["ordinal"]),
                "source_identifier": _hex(int(entry["thunk_id"]), 6),
                "live_value": _hex(value),
                "classification": "reviewed-source-data-import",
                "live_value_interpretation": "not-inferred",
            })
            continue

        thunk_id = int(entry["thunk_id"])
        thunk = thunks_by_id.get(thunk_id)
        _require(thunk is not None, "live_iat_function_mapping_missing",
                 "function-IAT cell has no decoded live thunk",
                 iat_index=index, rva=_hex(rva),
                 thunk_id=_hex(thunk_id, 6))
        assert thunk is not None
        target = int(thunk["_live_target"])
        _require(value == target, "live_iat_function_target_mismatch",
                 "live function-IAT cell does not equal its decoded thunk target",
                 iat_index=index, rva=_hex(rva),
                 thunk_slot=int(thunk["index"]),
                 expected=_hex(target), actual=_hex(value))
        function_count += 1

    _require(function_count == profile.thunk_slot_count,
             "live_iat_function_count_mismatch",
             "validated live function-IAT count does not match Rev1655",
             expected=profile.thunk_slot_count, actual=function_count)
    return ({
        "size": len(live_iat),
        "sha256": _sha256(live_iat),
        "function_count": function_count,
        "function_targets_match_thunks": True,
        "data_import_count": len(data_imports),
        "separator_count": len(separators),
        "data_imports": data_imports,
        "separators": separators,
    }, live_words)


def _resolver_module_queried_bit(index: int) -> int:
    return 1 << index


def _resolver_module_found_bit(index: int) -> int:
    return 1 << (index + 4)


def _resolver_slot_bit(index: int, field: int) -> int:
    return 1 << (index * 5 + 8 + field)


def _nt_success(status: int) -> bool:
    return status & 0x80000000 == 0


def _analyze_resolver(
    resolver: bytes,
    raw_thunks: Sequence[dict[str, Any]],
    live_text: bytes,
    live_iat_words: Sequence[int],
    profile: ImageProfile,
) -> dict[str, Any]:
    _require(len(resolver) == RESOLVER_SIZE,
             "resolver_size_mismatch", "resolver evidence has the wrong size",
             expected=RESOLVER_SIZE, actual=len(resolver))
    _require(resolver[:4] == RESOLVER_MAGIC,
             "resolver_magic_mismatch", "resolver evidence has the wrong magic",
             expected=RESOLVER_MAGIC.decode("ascii"),
             actual=resolver[:4].hex())
    version, record_size, status, module_count, slot_count = struct.unpack_from(
        ">IIIII", resolver, 4)
    _require(version == RESOLVER_VERSION,
             "resolver_version_mismatch", "resolver evidence has the wrong version",
             expected=RESOLVER_VERSION, actual=version)
    _require(record_size == RESOLVER_SIZE,
             "resolver_record_size_mismatch",
             "resolver record_size does not match its reviewed layout",
             expected=RESOLVER_SIZE, actual=record_size)
    _require(module_count == RESOLVER_MODULE_COUNT and
             slot_count == RESOLVER_SLOT_COUNT,
             "resolver_record_count_mismatch",
             "resolver module/slot counts do not match v4",
             expected=[RESOLVER_MODULE_COUNT, RESOLVER_SLOT_COUNT],
             actual=[module_count, slot_count])

    known_status_mask = RESOLVER_STATUS_CAPTURE_COMPLETE
    for index in range(RESOLVER_MODULE_COUNT):
        known_status_mask |= _resolver_module_queried_bit(index)
        known_status_mask |= _resolver_module_found_bit(index)
    for index in range(RESOLVER_SLOT_COUNT):
        for field in range(5):
            known_status_mask |= _resolver_slot_bit(index, field)
    _require(status & ~known_status_mask == 0,
             "resolver_status_unknown_bits",
             "resolver status contains undefined bits",
             status=_hex(status), unknown=_hex(status & ~known_status_mask))
    _require(status & RESOLVER_STATUS_CAPTURE_COMPLETE != 0,
             "resolver_status_incomplete",
             "resolver capture-complete status bit is not set",
             status=_hex(status))

    modules: list[dict[str, Any]] = []
    for index, (expected_tag, requested_identity) in enumerate(
            RESOLVER_MODULE_IDENTITIES):
        offset = RESOLVER_MODULES_OFFSET + index * RESOLVER_MODULE_RECORD_SIZE
        tag = resolver[offset:offset + 4]
        query_status, handle = struct.unpack_from(">II", resolver, offset + 4)
        _require(tag == expected_tag, "resolver_module_tag_mismatch",
                 "resolver module record has the wrong identity tag",
                 module_index=index,
                 expected=expected_tag.decode("ascii"), actual=tag.hex())
        queried = status & _resolver_module_queried_bit(index) != 0
        found = status & _resolver_module_found_bit(index) != 0
        _require(queried and query_status != RESOLVER_STATUS_NOT_CALLED,
                 "resolver_module_status_mismatch",
                 "resolver module record was not completely queried",
                 module_index=index, status=_hex(status),
                 query_status=_hex(query_status))
        expected_found = _nt_success(query_status) and handle != 0
        _require(found == expected_found,
                 "resolver_module_found_mismatch",
                 "resolver module-found bit disagrees with query evidence",
                 module_index=index, found_flag=found,
                 query_status=_hex(query_status), handle=_hex(handle))
        modules.append({
            "index": index,
            "tag": tag.decode("ascii"),
            "requested_identity": requested_identity,
            "query_status": _hex(query_status),
            "query_succeeded": _nt_success(query_status),
            "handle": _hex(handle),
            "found": found,
        })
    _require(modules[0]["found"], "resolver_xam_not_found",
             "resolver evidence cannot make the required direct xam queries")

    slots: list[dict[str, Any]] = []
    direct_match_count = 0
    for record_index, identity in enumerate(RESOLVER_SLOT_IDENTITIES):
        thunk_index, expected_ordinal, expected_thunk_va, expected_iat_rva = identity
        _require(thunk_index < len(raw_thunks),
                 "resolver_profile_slot_out_of_range",
                 "reviewed resolver slot is outside the thunk table",
                 thunk_slot=thunk_index)
        thunk = raw_thunks[thunk_index]
        source_identity = (
            int(thunk["ordinal"]),
            profile.image_base + profile.text_rva +
            profile.thunk_text_offset + thunk_index * profile.thunk_slot_size,
            int(thunk["_iat_rva"]),
        )
        _require(source_identity == (
                    expected_ordinal, expected_thunk_va, expected_iat_rva),
                 "resolver_profile_slot_identity_mismatch",
                 "reviewed source PE no longer maps the resolver slot identity",
                 thunk_slot=thunk_index,
                 expected=[_hex(expected_ordinal, 4),
                           _hex(expected_thunk_va), _hex(expected_iat_rva)],
                 actual=[_hex(source_identity[0], 4),
                         _hex(source_identity[1]), _hex(source_identity[2])])

        offset = RESOLVER_SLOTS_OFFSET + record_index * RESOLVER_SLOT_RECORD_SIZE
        values = struct.unpack_from(">11I", resolver, offset)
        (ordinal, thunk_va, iat_rva, thunk_word_0, thunk_word_1,
         decoded_target, iat_value, procedure_status, procedure_target,
         pc_header, owner_ldr) = values
        actual_identity = (ordinal, thunk_va, iat_rva)
        expected_identity = (
            expected_ordinal, expected_thunk_va, expected_iat_rva)
        _require(actual_identity == expected_identity,
                 "resolver_slot_identity_mismatch",
                 "resolver slot record does not identify the reviewed import",
                 resolver_slot=record_index, thunk_slot=thunk_index,
                 expected=[_hex(expected_ordinal, 4),
                           _hex(expected_thunk_va), _hex(expected_iat_rva)],
                 actual=[_hex(ordinal, 4), _hex(thunk_va), _hex(iat_rva)])

        required_bits = 0
        for field in range(5):
            required_bits |= _resolver_slot_bit(record_index, field)
        _require(status & required_bits == required_bits,
                 "resolver_slot_status_incomplete",
                 "resolver slot is missing required mapped/query evidence",
                 resolver_slot=record_index, thunk_slot=thunk_index,
                 required=_hex(required_bits), actual=_hex(status & required_bits))
        _require(procedure_status != RESOLVER_STATUS_NOT_CALLED,
                 "resolver_procedure_status_missing",
                 "resolver direct procedure query has no result",
                 resolver_slot=record_index, thunk_slot=thunk_index)

        text_offset = profile.thunk_text_offset + (
            thunk_index * profile.thunk_slot_size)
        live_words = (
            _u32be(live_text, text_offset),
            _u32be(live_text, text_offset + 4),
        )
        _require((thunk_word_0, thunk_word_1) == live_words,
                 "resolver_slot_live_thunk_mismatch",
                 "resolver-recorded thunk words differ from the live .text capture",
                 resolver_slot=record_index, thunk_slot=thunk_index,
                 expected=[_hex(value) for value in live_words],
                 actual=[_hex(thunk_word_0), _hex(thunk_word_1)])
        live_target = int(thunk["_live_target"])
        _require(decoded_target == _decode_target(thunk_word_0, thunk_word_1)
                 and decoded_target == live_target,
                 "resolver_slot_decoded_target_mismatch",
                 "resolver decoded target differs from its words or live thunk",
                 resolver_slot=record_index, thunk_slot=thunk_index,
                 expected=_hex(live_target), actual=_hex(decoded_target))
        live_iat_index = (iat_rva - profile.iat_rva) // 4
        live_iat_value = live_iat_words[live_iat_index]
        _require(iat_value == live_iat_value and iat_value == live_target,
                 "resolver_slot_iat_target_mismatch",
                 "resolver-recorded IAT value differs from live IAT or thunk",
                 resolver_slot=record_index, thunk_slot=thunk_index,
                 expected=_hex(live_target),
                 recorded=_hex(iat_value), live_iat=_hex(live_iat_value))

        direct_equals_live = procedure_target == live_target
        direct_match_count += int(direct_equals_live)
        owner_matches = [
            {
                "module_index": int(module["index"]),
                "tag": str(module["tag"]),
                "requested_identity": str(module["requested_identity"]),
            }
            for module in modules
            if int(str(module["handle"]), 16) != 0 and
            int(str(module["handle"]), 16) == owner_ldr
        ]
        slots.append({
            "resolver_slot": record_index,
            "thunk_slot": thunk_index,
            "library": str(thunk["library"]),
            "ordinal": ordinal,
            "thunk_va": _hex(thunk_va),
            "iat_rva": _hex(iat_rva),
            "procedure_status": _hex(procedure_status),
            "procedure_query_succeeded": _nt_success(procedure_status),
            "targets": {
                "recorded_decoded": _hex(decoded_target),
                "recorded_iat": _hex(iat_value),
                "live_thunk": _hex(live_target),
                "live_iat": _hex(live_iat_value),
                "direct_resolution": _hex(procedure_target),
            },
            "comparisons": {
                "recorded_thunk_words_equal_live": True,
                "recorded_decoded_equal_live_thunk": True,
                "recorded_iat_equal_live_iat": True,
                "live_iat_equal_live_thunk": True,
                "direct_resolution_equal_live_thunk": direct_equals_live,
                "direct_resolution_equal_live_iat":
                    procedure_target == live_iat_value,
            },
            "owner_evidence": {
                "pc_header": _hex(pc_header),
                "owner_ldr": _hex(owner_ldr),
                "owner_ldr_matches_queried_module_handles": owner_matches,
                "attribution": "pointer-equality-evidence-only",
            },
        })

    return {
        "size": len(resolver),
        "sha256": _sha256(resolver),
        "version": version,
        "status": _hex(status),
        "capture_complete": True,
        "modules": modules,
        "slots": slots,
        "direct_resolution_match_count": direct_match_count,
    }


def analyze_image(
    pe: bytes,
    live_header: bytes,
    live_text: bytes,
    profile: ImageProfile = REV1655_PROFILE,
    live_iat: bytes | None = None,
    resolver: bytes | None = None,
) -> dict[str, Any]:
    """Analyze required image artifacts and optional v4 evidence."""
    _require(resolver is None or live_iat is not None,
             "resolver_requires_live_iat",
             "resolver evidence requires the matching live IAT artifact")
    reference_header, reference_text, iat_words = _validate_pe(pe, profile)
    _require(len(live_header) == profile.header_size,
             "live_header_size_mismatch", "live header has the wrong size",
             expected=profile.header_size, actual=len(live_header))
    _require(len(live_text) == profile.text_size,
             "live_text_size_mismatch", "live .text has the wrong size",
             expected=profile.text_size, actual=len(live_text))

    header_diff = _difference_summary(reference_header, live_header)
    _require(header_diff["equal"], "live_header_mismatch",
             "CPU-visible header differs from the reviewed PE header",
             **header_diff)

    source_iat, source_iat_functions = _decode_source_iat(iat_words, profile)
    raw_thunks = _decode_raw_thunks(
        reference_text, source_iat_functions, profile)
    prefix_size = profile.thunk_text_offset
    reference_prefix = reference_text[:prefix_size]
    live_prefix = live_text[:prefix_size]
    prefix_diff = _difference_summary(reference_prefix, live_prefix)
    _require(prefix_diff["equal"], "live_text_immutable_prefix_mismatch",
             "CPU-visible .text differs outside the loader-owned thunk suffix",
             **prefix_diff)

    canonical = bytearray(live_text)
    reported_thunks: list[dict[str, Any]] = []
    total_changed_words = 0
    mtctr_bctr_unchanged = 0
    library_counts: dict[str, int] = {name: 0 for name in LIBRARIES.values()}
    target_min = 0xFFFFFFFF
    target_max = 0
    for raw in raw_thunks:
        index = int(raw["index"])
        slot_offset = profile.thunk_text_offset + index * profile.thunk_slot_size
        live_words = tuple(
            _u32be(live_text, slot_offset + word * 4)
            for word in range(4)
        )
        raw_words = raw["_raw_words"]
        changed_words = sum(
            expected != actual
            for expected, actual in zip(raw_words, live_words)
        )
        total_changed_words += changed_words
        trailer_ok = (
            live_words[2] == PPC_MTCTR_R11 and live_words[3] == PPC_BCTR
        )
        _require(trailer_ok, "live_thunk_trailer_mismatch",
                 "CPU-visible import thunk changed its mtctr/bctr trailer",
                 slot=index, text_offset=_hex(slot_offset),
                 live_words=[_hex(word) for word in live_words])
        mtctr_bctr_unchanged += 1
        encoding_ok = (
            live_words[0] & PPC_LIS_R11_MASK == PPC_LIS_R11 and
            live_words[1] & PPC_ADDI_R11_R11_MASK == PPC_ADDI_R11_R11
        )
        _require(encoding_ok, "live_thunk_encoding_mismatch",
                 "CPU-visible import thunk is not lis r11 + addi r11,r11",
                 slot=index, text_offset=_hex(slot_offset),
                 live_words=[_hex(word) for word in live_words])
        _require(changed_words == 2, "live_thunk_changed_word_count_mismatch",
                 "CPU-visible import thunk changed words outside its loader-owned pair",
                 slot=index, expected=2, actual=changed_words)
        target = _decode_target(live_words[0], live_words[1])
        raw["_live_target"] = target
        library_id = int(raw["library_id"])
        _require(_target_is_allowed(library_id, target),
                 "live_thunk_target_out_of_range",
                 "decoded import target is outside its accepted address envelope",
                 slot=index, target=_hex(target),
                 library=raw["library"],
                 allowed=_target_policy(library_id))
        target_min = min(target_min, target)
        target_max = max(target_max, target)
        library = str(raw["library"])
        library_counts[library] += 1
        canonical[slot_offset:slot_offset + 8] = struct.pack(
            ">II", raw_words[0], raw_words[1])
        reported = {key: value for key, value in raw.items()
                    if not key.startswith("_")}
        reported.update({
            "state": "resolved",
            "live_words": [_hex(word) for word in live_words],
            "changed_word_count": changed_words,
            "target": _hex(target),
            "target_in_allowed_range": True,
        })
        reported_thunks.append(reported)

    expected_changed_words = profile.thunk_slot_count * 2
    _require(total_changed_words == expected_changed_words,
             "live_thunk_total_changed_word_count_mismatch",
             "CPU-visible thunk table has an unexpected changed-word count",
             expected=expected_changed_words, actual=total_changed_words)
    canonical_digest = _sha256(canonical)
    canonical_diff = _difference_summary(reference_text, bytes(canonical))
    _require(canonical_diff["equal"] and
             canonical_digest == profile.text_sha256,
             "canonical_text_sha256_mismatch",
             "canonicalized CPU-visible .text does not reconstruct Rev1655",
             expected=profile.text_sha256, actual=canonical_digest,
             **canonical_diff)

    live_iat_report: dict[str, Any] | None = None
    resolver_report: dict[str, Any] | None = None
    live_iat_words: list[int] | None = None
    if live_iat is not None:
        live_iat_report, live_iat_words = _analyze_live_iat(
            live_iat, source_iat, raw_thunks, profile)
        for reported, raw in zip(reported_thunks, raw_thunks):
            reported["live_iat_value"] = _hex(
                live_iat_words[int(raw["_iat_index"])])
    if resolver is not None:
        assert live_iat_words is not None
        resolver_report = _analyze_resolver(
            resolver, raw_thunks, live_text, live_iat_words, profile)

    report: dict[str, Any] = {
        "schema": SCHEMA,
        "compatible": True,
        "profile": profile.name,
        "layout": {
            "image_base": _hex(profile.image_base),
            "pe_size": profile.pe_size,
            "header_size": profile.header_size,
            "text_rva": _hex(profile.text_rva),
            "text_size": profile.text_size,
            "text_raw_offset": _hex(profile.text_raw_offset),
            "thunk_rva": _hex(profile.thunk_rva),
            "thunk_text_offset": _hex(profile.thunk_text_offset),
            "thunk_slot_size": profile.thunk_slot_size,
            "thunk_slot_count": profile.thunk_slot_count,
            "iat_rva": _hex(profile.iat_rva),
            "iat_size": profile.iat_size,
        },
        "hashes": {
            "pe_sha256": _sha256(pe),
            "reference_header_sha256": _sha256(reference_header),
            "live_header_sha256": _sha256(live_header),
            "reference_text_sha256": _sha256(reference_text),
            "live_text_sha256": _sha256(live_text),
            "reference_immutable_prefix_sha256": _sha256(reference_prefix),
            "live_immutable_prefix_sha256": _sha256(live_prefix),
            "canonicalized_text_sha256": canonical_digest,
        },
        "header": header_diff,
        "immutable_prefix": {
            "size": prefix_size,
            **prefix_diff,
        },
        "thunks": {
            "raw_marker_structure_valid": True,
            "raw_marker_slot_count": len(raw_thunks),
            "live_lis_addi_slot_count": len(reported_thunks),
            "mtctr_bctr_unchanged_slot_count": mtctr_bctr_unchanged,
            "changed_word_count": total_changed_words,
            "expected_changed_word_count": expected_changed_words,
            "library_counts": library_counts,
            "decoded_target_range": {
                "minimum": _hex(target_min),
                "maximum": _hex(target_max),
            },
            "slots": reported_thunks,
        },
        "iat_mapping": {
            "source_derived": True,
            "function_count": len(source_iat_functions),
            "data_import_count": len(REV1655_DATA_IMPORT_RVAS),
            "separator_count": len(REV1655_IAT_SEPARATOR_RVAS),
            "data_import_rvas": [
                _hex(rva) for rva in REV1655_DATA_IMPORT_RVAS
            ],
            "separator_rvas": [
                _hex(rva) for rva in REV1655_IAT_SEPARATOR_RVAS
            ],
        },
        "canonicalization": {
            "equal_to_reference": True,
            **canonical_diff,
        },
    }
    if live_iat_report is not None:
        report["live_iat"] = live_iat_report
    if resolver_report is not None:
        report["resolver"] = resolver_report
    return report


def _read_artifact(path: Path, kind: str) -> bytes:
    try:
        return path.read_bytes()
    except OSError as exc:
        raise AnalysisError(
            "artifact_read_failed",
            f"could not read {kind} artifact",
            {"path": str(path), "reason": str(exc)},
        ) from exc


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate Aurora Rev1655 CPU-visible image dumps")
    parser.add_argument("--pe", required=True, type=Path,
                        help="reviewed original/Aurora.exe")
    parser.add_argument("--header", required=True, type=Path,
                        help="CPU-visible 0x400-byte header dump")
    parser.add_argument("--text", required=True, type=Path,
                        help="CPU-visible Rev1655 .text dump")
    parser.add_argument("--iat", type=Path,
                        help="optional CPU-visible 0x5B4-byte v4 IAT dump")
    parser.add_argument("--resolver", type=Path,
                        help="optional 160-byte AZRE v4 resolver evidence")
    return parser


def main(
    argv: Sequence[str] | None = None,
    profile: ImageProfile = REV1655_PROFILE,
) -> int:
    args = _parser().parse_args(argv)
    try:
        report = analyze_image(
            _read_artifact(args.pe, "PE"),
            _read_artifact(args.header, "header"),
            _read_artifact(args.text, "text"),
            profile,
            live_iat=(
                _read_artifact(args.iat, "IAT")
                if args.iat is not None else None
            ),
            resolver=(
                _read_artifact(args.resolver, "resolver")
                if args.resolver is not None else None
            ),
        )
    except AnalysisError as exc:
        report = {
            "schema": SCHEMA,
            "compatible": False,
            "profile": profile.name,
            "error": {
                "code": exc.code,
                "message": exc.message,
                "details": exc.details,
            },
        }
        json.dump(report, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 2
    json.dump(report, sys.stdout, indent=2, sort_keys=True)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
