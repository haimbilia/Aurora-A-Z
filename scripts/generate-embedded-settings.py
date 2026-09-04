#!/usr/bin/env python3
"""Emit the compiled Aurora A-Z settings XUR as a C byte array."""

from __future__ import annotations

import argparse
from pathlib import Path


def emit_c(data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        lines.append(
            "    " + ", ".join(f"0x{value:02x}u" for value in chunk) + ","
        )
    return (
        "#include <stdint.h>\n\n"
        "const uint8_t g_auroraaz_embedded_settings_xur[] = {\n"
        + "\n".join(lines)
        + "\n};\n"
        + "const uint32_t g_auroraaz_embedded_settings_xur_size = "
        + f"{len(data)}u;\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = args.source.read_bytes()
    # XUIHelper's compiled XUR v5 container uses the XUIB magic. Keep this
    # strict so XML or a failed conversion is never embedded as binary XUI.
    if not data.startswith(b"XUIB"):
        raise SystemExit("settings resource is not a compiled XUR")
    if not data:
        raise SystemExit("settings resource is empty")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(emit_c(data), encoding="ascii", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
