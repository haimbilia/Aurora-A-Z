#!/usr/bin/env python3
"""Validate an Aurora Rev1655 CPU-visible header and .text capture.

Aurora's Xbox 360 loader rewrites the first two words of each import thunk at
the very end of .text.  Everything else is immutable.  This tool verifies the
source PE, the captured immutable bytes, and the exact loader rewrite shape,
then reconstructs the original .text before hashing it.
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


def _validate_pe(pe: bytes, profile: ImageProfile) -> tuple[bytes, bytes, list[int]]:
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
    iat_ids = [
        _u32be(pe, iat_offset + offset) & THUNK_ID_MASK
        for offset in range(0, profile.iat_size, 4)
    ]
    return reference_header, reference_text, iat_ids


def _decode_raw_thunks(
    reference_text: bytes,
    iat_ids: Sequence[int],
    profile: ImageProfile,
) -> list[dict[str, Any]]:
    offset = profile.thunk_text_offset
    _require(offset >= 0 and offset + profile.thunk_size == len(reference_text),
             "raw_thunk_location_mismatch",
             "the reviewed thunk table is not the exact .text suffix",
             text_size=len(reference_text), thunk_offset=_hex(offset),
             thunk_size=profile.thunk_size)
    iat_counts: dict[int, int] = {}
    for thunk_id in iat_ids:
        if thunk_id != 0:
            iat_counts[thunk_id] = iat_counts.get(thunk_id, 0) + 1

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
        _require(iat_counts.get(thunk_id, 0) == 1,
                 "raw_thunk_iat_membership_mismatch",
                 "source PE thunk identifier is not represented exactly once in the IAT",
                 slot=index, thunk_id=_hex(thunk_id, 6),
                 iat_occurrences=iat_counts.get(thunk_id, 0))
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
            "raw_words": [_hex(word) for word in words],
            "_raw_words": words,
        })
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


def analyze_image(
    pe: bytes,
    live_header: bytes,
    live_text: bytes,
    profile: ImageProfile = REV1655_PROFILE,
) -> dict[str, Any]:
    """Analyze three in-memory artifacts and return a JSON-safe report."""
    reference_header, reference_text, iat_ids = _validate_pe(pe, profile)
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

    raw_thunks = _decode_raw_thunks(reference_text, iat_ids, profile)
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

    return {
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
        "canonicalization": {
            "equal_to_reference": True,
            **canonical_diff,
        },
    }


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
