#!/usr/bin/env python3
"""Resize the repository RGBA PNG and emit an embedded C byte array."""

from __future__ import annotations

import argparse
import binascii
from pathlib import Path
import struct
import zlib


PNG = b"\x89PNG\r\n\x1a\n"


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def decode_rgba(data: bytes) -> tuple[int, int, bytes]:
    if not data.startswith(PNG):
        raise ValueError("icon is not a PNG")
    pos, compressed, header = 8, bytearray(), None
    while pos + 12 <= len(data):
        size = struct.unpack_from(">I", data, pos)[0]
        kind = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + size]
        pos += 12 + size
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
    if header is None:
        raise ValueError("PNG has no IHDR")
    width, height, depth, color, compression, filtering, interlace = header
    if (depth, color, compression, filtering, interlace) != (8, 6, 0, 0, 0):
        raise ValueError("icon must be non-interlaced 8-bit RGBA")
    raw = zlib.decompress(bytes(compressed))
    stride, source, prior = width * 4, bytearray(), bytearray(width * 4)
    offset = 0
    for _ in range(height):
        mode = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += stride + 1
        for x in range(stride):
            left = row[x - 4] if x >= 4 else 0
            up = prior[x]
            upper_left = prior[x - 4] if x >= 4 else 0
            if mode == 1:
                row[x] = (row[x] + left) & 0xFF
            elif mode == 2:
                row[x] = (row[x] + up) & 0xFF
            elif mode == 3:
                row[x] = (row[x] + ((left + up) // 2)) & 0xFF
            elif mode == 4:
                row[x] = (row[x] + paeth(left, up, upper_left)) & 0xFF
            elif mode != 0:
                raise ValueError(f"unsupported PNG filter {mode}")
        source.extend(row)
        prior = row
    return width, height, bytes(source)


def chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", binascii.crc32(body) & 0xFFFFFFFF)


def resize_png(data: bytes, side: int) -> bytes:
    width, height, pixels = decode_rgba(data)
    rows = bytearray()
    for y in range(side):
        source_y = min(height - 1, (y * height) // side)
        rows.append(0)
        for x in range(side):
            source_x = min(width - 1, (x * width) // side)
            start = (source_y * width + source_x) * 4
            rows.extend(pixels[start:start + 4])
    ihdr = struct.pack(">IIBBBBB", side, side, 8, 6, 0, 0, 0)
    return PNG + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")


def emit_c(png: bytes) -> str:
    lines = []
    for offset in range(0, len(png), 12):
        lines.append("    " + ", ".join(f"0x{x:02x}u" for x in png[offset:offset + 12]) + ",")
    return (
        "#include <stdint.h>\n\n"
        "const uint8_t g_auroraaz_embedded_icon_png[] = {\n"
        + "\n".join(lines)
        + "\n};\n"
        + f"const uint32_t g_auroraaz_embedded_icon_png_size = {len(png)}u;\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--size", type=int, default=64)
    args = parser.parse_args()
    if not 16 <= args.size <= 256:
        raise SystemExit("size must be between 16 and 256")
    output = resize_png(args.source.read_bytes(), args.size)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(emit_c(output), encoding="ascii", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
