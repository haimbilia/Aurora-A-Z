#!/usr/bin/env python3
"""Validate and decode AuroraAZ AZS2 v1 dual-slot scene telemetry."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys
from typing import Any, Sequence
import zlib


MAGIC = b"AZS2"
VERSION = 1
RECORD_SIZE = 256
CRC_OFFSET = 252
REASON_COUNTERS_OFFSET = 112
REASON_COUNT = 13
ALL_SAFETY_FLAGS = 0x1FF

REASONS = (
    "static-not-verified",
    "manager-unavailable",
    "memory-unreadable",
    "cache-changed",
    "cache-cycle",
    "cache-limit",
    "path-invalid",
    "main-not-found",
    "main-duplicate",
    "main-not-acquired",
    "handle-invalid",
    "main-not-focused",
    "main-focused",
)
CONFIGURE_RESULTS = (
    "ok",
    "null",
    "already-configured",
    "bad-bindings",
    "image-unverified",
    "signature-mismatch",
)
SAFETY_FLAGS = {
    0x001: "callback_unavailable",
    0x002: "invalid_value",
    0x004: "probe_decision_mismatch",
    0x008: "reason_allow_mismatch",
    0x010: "gate_not_verified",
    0x020: "status_decision_mismatch",
    0x040: "system_ui_raw_allow",
    0x080: "frame_nonmonotonic",
    0x100: "eligibility_mismatch",
}


class TelemetryDecodeError(ValueError):
    """The bytes are not one valid AZS2 v1 telemetry record."""


def _u16(record: bytes, offset: int) -> int:
    return struct.unpack_from(">H", record, offset)[0]


def _u32(record: bytes, offset: int) -> int:
    return struct.unpack_from(">I", record, offset)[0]


def _boolean(record: bytes, offset: int, name: str) -> bool:
    value = record[offset]
    if value > 1:
        raise TelemetryDecodeError(f"invalid {name}: {value}")
    return bool(value)


def _enum(values: tuple[str, ...], value: int, name: str) -> str:
    if value >= len(values):
        raise TelemetryDecodeError(f"invalid {name}: {value}")
    return values[value]


def _flag_names(flags: int) -> list[str]:
    return [name for bit, name in SAFETY_FLAGS.items() if flags & bit]


def decode_record(data: bytes | bytearray | memoryview) -> dict[str, Any]:
    """Validate exactly one fixed-size big-endian scene record."""

    record = bytes(data)
    if len(record) != RECORD_SIZE:
        raise TelemetryDecodeError(
            f"record must be exactly {RECORD_SIZE} bytes, got {len(record)}"
        )
    if record[:4] != MAGIC:
        raise TelemetryDecodeError("invalid AZS2 magic")
    if _u16(record, 4) != VERSION:
        raise TelemetryDecodeError(f"unsupported AZS2 version: {_u16(record, 4)}")
    if _u16(record, 6) != RECORD_SIZE:
        raise TelemetryDecodeError(f"invalid encoded record size: {_u16(record, 6)}")
    expected_crc = zlib.crc32(record[:CRC_OFFSET]) & 0xFFFFFFFF
    stored_crc = _u32(record, CRC_OFFSET)
    if stored_crc != expected_crc:
        raise TelemetryDecodeError(
            f"CRC32 mismatch: stored 0x{stored_crc:08x}, "
            f"expected 0x{expected_crc:08x}"
        )
    if record[175] or any(record[228:CRC_OFFSET]):
        raise TelemetryDecodeError("reserved bytes must be zero")

    samples = _u32(record, 12)
    raw_allowed = _u32(record, 16)
    raw_denied = _u32(record, 20)
    eligible = _u32(record, 24)
    transitions = _u32(record, 28)
    safety_flags = _u32(record, 32)
    invalid_samples = _u32(record, 36)
    ui_active = _u32(record, 40)
    ui_raw_allowed = _u32(record, 44)
    nonmonotonic = _u32(record, 48)
    callback_unavailable = _u32(record, 52)
    last_sample_flags = _u32(record, 224)
    reason_counts = {
        name: _u32(record, REASON_COUNTERS_OFFSET + index * 4)
        for index, name in enumerate(REASONS)
    }

    if safety_flags & ~ALL_SAFETY_FLAGS:
        raise TelemetryDecodeError("safety_flags contains an unknown bit")
    if last_sample_flags & ~ALL_SAFETY_FLAGS:
        raise TelemetryDecodeError("last_sample_safety_flags contains an unknown bit")
    bounded_counts = (
        raw_allowed,
        raw_denied,
        invalid_samples,
        ui_active,
        nonmonotonic,
        callback_unavailable,
        *reason_counts.values(),
    )
    if any(count > samples for count in bounded_counts):
        raise TelemetryDecodeError("a per-sample counter exceeds samples")
    if eligible > raw_allowed:
        raise TelemetryDecodeError("eligible exceeds raw_allowed")
    if ui_raw_allowed > ui_active or ui_raw_allowed > raw_allowed:
        raise TelemetryDecodeError("ui_raw_allowed violates count bounds")
    if samples != 0xFFFFFFFF:
        if raw_allowed + raw_denied != samples:
            raise TelemetryDecodeError("raw allow/deny counters do not sum to samples")
        if sum(reason_counts.values()) != samples:
            raise TelemetryDecodeError("reason counters do not sum to samples")
        if (samples == 0 and transitions != 0) or (
            samples != 0 and transitions >= samples
        ):
            raise TelemetryDecodeError("transition count violates sample bounds")

    configure_result = _enum(
        CONFIGURE_RESULTS, record[164], "last_configure_result"
    )
    decision_reason = _enum(REASONS, record[165], "last_decision_reason")
    status_reason = _enum(REASONS, record[166], "last_status_reason")
    callback_available = _boolean(record, 167, "last_callback_available")
    raw_probe = _boolean(record, 168, "last_raw_probe")
    decision_allows = _boolean(record, 169, "last_decision_allows")
    last_eligible = _boolean(record, 170, "last_eligible")
    system_ui_active = _boolean(record, 171, "last_system_ui_active")
    configured = _boolean(record, 172, "last_status_configured")
    exact_image_verified = _boolean(record, 173, "last_exact_image_verified")
    signatures_verified = _boolean(record, 174, "last_signatures_verified")
    if last_eligible and not (
        callback_available
        and raw_probe
        and decision_allows
        and decision_reason == "main-focused"
        and not system_ui_active
        and configure_result == "ok"
        and configured
        and exact_image_verified
        and signatures_verified
    ):
        raise TelemetryDecodeError("last_eligible violates fail-closed invariants")

    return {
        "format": "AZS2",
        "version": VERSION,
        "generation": _u32(record, 8),
        "samples": samples,
        "raw_allowed": raw_allowed,
        "raw_denied": raw_denied,
        "capture_eligible": eligible,
        "transitions": transitions,
        "max_scanned_nodes": _u32(record, 68),
        "frames": {
            "first": _u32(record, 56),
            "last": _u32(record, 60),
            "last_transition": _u32(record, 64),
            "nonmonotonic": nonmonotonic,
        },
        "system_ui": {
            "active_samples": ui_active,
            "raw_allowed_samples": ui_raw_allowed,
        },
        "invalid_samples": invalid_samples,
        "callback_unavailable": callback_unavailable,
        "observation_drops": _u32(record, 220),
        "reason_counts": reason_counts,
        "safety": {
            "flags": safety_flags,
            "names": _flag_names(safety_flags),
            "last_flags": last_sample_flags,
            "last_names": _flag_names(last_sample_flags),
        },
        "last": {
            "frame": _u32(record, 60),
            "configure_result": configure_result,
            "decision_reason": decision_reason,
            "status_reason": status_reason,
            "callback_available": callback_available,
            "raw_probe_allowed": raw_probe,
            "decision_allows_capture": decision_allows,
            "capture_eligible": last_eligible,
            "system_ui_active": system_ui_active,
            "configured": configured,
            "exact_image_verified": exact_image_verified,
            "signatures_verified": signatures_verified,
            "cache_head": _u32(record, 72),
            "main_scene_node": _u32(record, 76),
            "main_scene_handle": _u32(record, 80),
            "scanned_nodes": _u32(record, 84),
            "gate_probes": _u32(record, 88),
            "gate_allowed": _u32(record, 92),
            "gate_denied": _u32(record, 96),
            "gate_configure_attempts": _u32(record, 100),
            "gate_configure_successes": _u32(record, 104),
        },
        "gate_failures": {
            "static_validation": _u32(record, 108),
            "manager_unavailable": _u32(record, 176),
            "memory_unreadable": _u32(record, 180),
            "cache_changed": _u32(record, 184),
            "cache_cycle": _u32(record, 188),
            "cache_limit": _u32(record, 192),
            "path_invalid": _u32(record, 196),
            "main_missing": _u32(record, 200),
            "main_duplicate": _u32(record, 204),
            "main_not_acquired": _u32(record, 208),
            "invalid_handle": _u32(record, 212),
            "main_not_focused": _u32(record, 216),
        },
        "crc32": stored_crc,
    }


def generation_is_newer(candidate: int, reference: int) -> bool:
    distance = (candidate - reference) & 0xFFFFFFFF
    return distance != 0 and distance < 0x80000000


def select_newest(
    slot_a: bytes | bytearray | memoryview | None,
    slot_b: bytes | bytearray | memoryview | None,
) -> dict[str, Any]:
    valid: dict[str, dict[str, Any]] = {}
    invalid: dict[str, str] = {}
    for name, data in (("A", slot_a), ("B", slot_b)):
        if data is None:
            continue
        try:
            valid[name] = decode_record(data)
        except TelemetryDecodeError as exc:
            invalid[name] = str(exc)
    if not valid:
        details = "; ".join(
            f"slot {name}: {error}" for name, error in invalid.items()
        )
        suffix = f" ({details})" if details else ""
        raise TelemetryDecodeError(f"no valid AZS2 telemetry slot{suffix}")
    selected = "A" if "A" in valid else "B"
    if "A" in valid and "B" in valid and generation_is_newer(
        valid["B"]["generation"], valid["A"]["generation"]
    ):
        selected = "B"
    result = {"selected_slot": selected, **valid[selected]}
    if invalid:
        result["invalid_slots"] = invalid
    return result


def _read(path: Path, slot: str) -> bytes:
    try:
        return path.read_bytes()
    except OSError as exc:
        raise TelemetryDecodeError(
            f"cannot read slot {slot} {path}: {exc}"
        ) from exc


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slot-a", type=Path, help="AuroraAZ-M2b-scene-A.bin")
    parser.add_argument("--slot-b", type=Path, help="AuroraAZ-M2b-scene-B.bin")
    args = parser.parse_args(argv)
    if args.slot_a is None and args.slot_b is None:
        parser.error("at least one of --slot-a or --slot-b is required")
    try:
        slot_a = _read(args.slot_a, "A") if args.slot_a is not None else None
        slot_b = _read(args.slot_b, "B") if args.slot_b is not None else None
        result = select_newest(slot_a, slot_b)
    except TelemetryDecodeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
