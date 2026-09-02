#!/usr/bin/env python3
"""Verify the Rev1655 scene-gate evidence against the checked-in binaries."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import struct
import sys


EXPECTED_EXE_SHA256 = (
    "5bb5baf8df4ccb197241b34935eb400f36c8c20648cc074e2c30fa80add37e3c"
)
EXPECTED_XEX_SHA256 = (
    "583bcd442d8017d6fcb2645b93cda987f4c0a43a688b652d7364ccaedaeefa9f"
)

VALIDATION_SPANS = (
    (0x82212194, bytes.fromhex("3D6082BC386BFFF8"), "manager singleton"),
    (
        0x82225540,
        bytes.fromhex(
            "83FF0028480000287FA3EB78809F00004873AB69"
            "2C03000040820010817F00082F0B0000419A0024"
            "83FF000C2B1F0000"
        ),
        "scene cache node layout",
    ),
    (0x82225588, bytes.fromhex("3960000183BF0004"), "acquired/handle fields"),
    (
        0x8280F448,
        bytes.fromhex(
            "546B043E2B030000419A00603D4082BB556B043E"
            "394AF8B8812A02207F0B4840"
        ),
        "XuiHandleIsValid entry",
    ),
    (
        0x82821978,
        bytes.fromhex(
            "7D8802A6481463519421FF903D6082BB7C7D1B78"
            "83EB0F28"
        ),
        "XuiElementHasFocus entry",
    ),
    (
        0x82821998,
        bytes.fromhex(
            "4BFEE0E17F1F1840419A001C38A000007FE4FB78"
            "7FA3EB784BFFDE012C0300004182000C"
        ),
        "focus ancestor test",
    ),
    (0x821208B8, "Aurora_Main.xur\0".encode("utf-16-be"), "main scene literal"),
)


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


class PeImage:
    def __init__(self, path: pathlib.Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        if self.data[:2] != b"MZ":
            raise ValueError(f"{path}: missing MZ header")
        pe_offset = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise ValueError(f"{path}: missing PE signature")

        coff = pe_offset + 4
        section_count = struct.unpack_from("<H", self.data, coff + 2)[0]
        optional_size = struct.unpack_from("<H", self.data, coff + 16)[0]
        optional = coff + 20
        magic = struct.unpack_from("<H", self.data, optional)[0]
        if magic != 0x10B:
            raise ValueError(f"{path}: expected PE32, found magic 0x{magic:04X}")
        self.image_base = struct.unpack_from("<I", self.data, optional + 28)[0]

        self.sections: list[tuple[int, int, int, int, str]] = []
        section_table = optional + optional_size
        for index in range(section_count):
            offset = section_table + index * 40
            name = self.data[offset : offset + 8].split(b"\0", 1)[0].decode(
                "ascii", "replace"
            )
            virtual_size, rva, raw_size, raw_offset = struct.unpack_from(
                "<IIII", self.data, offset + 8
            )
            self.sections.append(
                (self.image_base + rva, virtual_size, raw_offset, raw_size, name)
            )

    def read_va(self, address: int, size: int) -> bytes:
        for start, virtual_size, raw_offset, raw_size, name in self.sections:
            mapped_size = max(virtual_size, raw_size)
            relative = address - start
            if relative < 0 or relative + size > mapped_size:
                continue
            if relative + size > raw_size:
                raise ValueError(
                    f"0x{address:08X}+{size} reaches uninitialized data in {name}"
                )
            return self.data[raw_offset + relative : raw_offset + relative + size]
        raise ValueError(f"VA 0x{address:08X}+{size} is not mapped")


def parse_args() -> argparse.Namespace:
    repo = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=pathlib.Path, default=repo / "original/Aurora.exe")
    parser.add_argument("--xex", type=pathlib.Path, default=repo / "original/Aurora.xex")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    failures: list[str] = []

    for path, expected in (
        (args.exe, EXPECTED_EXE_SHA256),
        (args.xex, EXPECTED_XEX_SHA256),
    ):
        actual = sha256(path)
        state = "PASS" if actual == expected else "FAIL"
        print(f"{state} SHA-256 {path}: {actual.upper()}")
        if actual != expected:
            failures.append(f"SHA-256 mismatch: {path}")

    image = PeImage(args.exe)
    print(f"INFO PE image base: 0x{image.image_base:08X}")
    for address, expected, label in VALIDATION_SPANS:
        actual = image.read_va(address, len(expected))
        state = "PASS" if actual == expected else "FAIL"
        print(f"{state} 0x{address:08X}+{len(expected):02d} {label}")
        if actual != expected:
            failures.append(
                f"span mismatch at 0x{address:08X}: "
                f"expected {expected.hex().upper()}, got {actual.hex().upper()}"
            )

    if failures:
        for failure in failures:
            print(f"ERROR {failure}", file=sys.stderr)
        return 1
    print(f"scene-gate evidence verified: {len(VALIDATION_SPANS)} spans")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
