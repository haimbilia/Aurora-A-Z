#!/usr/bin/env python3
"""Validate and decode AuroraAZ AZI2 v2 dual-slot input telemetry."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys
import zlib
from typing import Any, Sequence


MAGIC = b"AZI2"
VERSION = 2
RECORD_SIZE = 160
CRC_OFFSET = 156
RESERVED_START = 128

ALL_CONTROLS_MASK = 0x7F
ALL_SAFETY_FLAGS = 0x0F

CONTROL_NAMES = (
    "a",
    "rb",
    "r3",
    "dpad_left",
    "dpad_right",
    "lstick_left",
    "lstick_right",
)
EVENT_NAMES = ("press", "repeat", "release")

RUNTIME_STATES = {
    0: "stopped",
    1: "starting",
    2: "running",
    3: "stopping",
    4: "closed",
}
COMMANDS = {
    0: "none",
    1: "enter",
    2: "previous",
    3: "next",
    4: "apply",
}
LAST_CONTROLS = {
    0: "unknown",
    1: "a",
    2: "rb",
    3: "r3",
    4: "dpad_left",
    5: "dpad_right",
    6: "lstick_left",
    7: "lstick_right",
}
LAST_EVENTS = {0: "invalid", 1: "press", 2: "repeat", 3: "release"}
REQUESTED_STAGES = {0: "off", 1: "observe", 2: "consume"}
EFFECTIVE_STAGES = {0: "observe_only", 1: "consume_verified"}


class TelemetryDecodeError(ValueError):
    """The bytes are not one valid AZI2 v2 telemetry record."""


def _u16(record: bytes, offset: int) -> int:
    return struct.unpack_from(">H", record, offset)[0]


def _u32(record: bytes, offset: int) -> int:
    return struct.unpack_from(">I", record, offset)[0]


def _crc32_ieee(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def _require_enum(name: str, value: int, values: dict[int, str]) -> str:
    try:
        return values[value]
    except KeyError as exc:
        raise TelemetryDecodeError(f"invalid {name}: {value}") from exc


def _require_boolean(name: str, value: int) -> bool:
    if value > 1:
        raise TelemetryDecodeError(f"invalid {name}: {value}")
    return bool(value)


def decode_record(data: bytes | bytearray | memoryview) -> dict[str, Any]:
    """Validate exactly one 160-byte big-endian record and decode it."""

    record = bytes(data)
    if len(record) != RECORD_SIZE:
        raise TelemetryDecodeError(
            f"record must be exactly {RECORD_SIZE} bytes, got {len(record)}"
        )
    if record[:4] != MAGIC:
        raise TelemetryDecodeError("invalid AZI2 magic")
    if _u16(record, 4) != VERSION:
        raise TelemetryDecodeError(f"unsupported AZI2 version: {_u16(record, 4)}")
    if _u16(record, 6) != RECORD_SIZE:
        raise TelemetryDecodeError(f"invalid encoded record size: {_u16(record, 6)}")

    expected_crc = _crc32_ieee(record[:CRC_OFFSET])
    stored_crc = _u32(record, CRC_OFFSET)
    if stored_crc != expected_crc:
        raise TelemetryDecodeError(
            f"CRC32 mismatch: stored 0x{stored_crc:08x}, expected 0x{expected_crc:08x}"
        )
    if any(record[RESERVED_START:CRC_OFFSET]):
        raise TelemetryDecodeError("reserved bytes 128..155 must be zero")

    seen_mask = _u32(record, 16)
    press_mask = _u32(record, 20)
    repeat_mask = _u32(record, 24)
    release_mask = _u32(record, 28)
    consumed_mask = _u32(record, 32)
    filter_queued_mask = _u32(record, 36)
    safety_flags = _u32(record, 40)
    event_masks = press_mask | repeat_mask | release_mask
    combined_masks = seen_mask | event_masks | consumed_mask | filter_queued_mask

    if combined_masks & ~ALL_CONTROLS_MASK:
        raise TelemetryDecodeError("control mask contains an unknown bit")
    if event_masks & ~seen_mask:
        raise TelemetryDecodeError("event masks must be subsets of seen_mask")
    if consumed_mask & ~seen_mask:
        raise TelemetryDecodeError("consumed_mask must be a subset of seen_mask")
    if filter_queued_mask & ~seen_mask:
        raise TelemetryDecodeError("filter_queued_mask must be a subset of seen_mask")
    if safety_flags & ~ALL_SAFETY_FLAGS:
        raise TelemetryDecodeError("safety_flags contains an unknown bit")
    if bool(consumed_mask) != bool(safety_flags & 0x01):
        raise TelemetryDecodeError("consumed safety flag disagrees with consumed_mask")
    if bool(filter_queued_mask) != bool(safety_flags & 0x02):
        raise TelemetryDecodeError(
            "filter-queued safety flag disagrees with filter_queued_mask"
        )

    runtime_state = _require_enum(
        "runtime_state", _u32(record, 94), RUNTIME_STATES
    )
    worker_entered = _require_boolean("worker_entered", record[98])
    last_command = _require_enum("last_command", record[99], COMMANDS)
    last_control = _require_enum("last_control", record[118], LAST_CONTROLS)
    last_event = _require_enum("last_event", record[119], LAST_EVENTS)
    last_requested_stage = _require_enum(
        "last_requested_stage", record[120], REQUESTED_STAGES
    )
    last_effective_stage = _require_enum(
        "last_effective_stage", record[121], EFFECTIVE_STAGES
    )
    last_consumed = _require_boolean("last_consumed", record[122])
    last_filter_queued = _require_boolean("last_filter_queued", record[123])
    last_would_handle = _require_boolean("last_would_handle", record[124])
    last_coverflow_active = _require_boolean(
        "last_coverflow_active", record[125]
    )

    controls: dict[str, dict[str, int | bool]] = {}
    for control_slot, control_name in enumerate(CONTROL_NAMES):
        bit = 1 << control_slot
        counter_offset = 48 + control_slot * len(EVENT_NAMES) * 2
        counts = {
            event_name: _u16(record, counter_offset + event_slot * 2)
            for event_slot, event_name in enumerate(EVENT_NAMES)
        }
        controls[control_name] = {
            "seen": bool(seen_mask & bit),
            **counts,
            "consumed": bool(consumed_mask & bit),
            "filter_queued": bool(filter_queued_mask & bit),
        }

    return {
        "format": "AZI2",
        "version": VERSION,
        "generation": _u32(record, 8),
        "relevant_observations": _u32(record, 12),
        "invalid_event_count": _u32(record, 44),
        "observation_drops": _u32(record, 90),
        "runtime_state": runtime_state,
        "worker_entered": worker_entered,
        "controls": controls,
        "safety": {
            "flags": safety_flags,
            "consumed_mask": consumed_mask,
            "filter_queued_mask": filter_queued_mask,
            "consumed": bool(safety_flags & 0x01),
            "filter_queued": bool(safety_flags & 0x02),
            "requested_not_observe": bool(safety_flags & 0x04),
            "effective_not_observe": bool(safety_flags & 0x08),
            "observe_only_ok": safety_flags == 0,
        },
        "last": {
            "command": last_command,
            "serial": _u32(record, 100),
            "input_frame": _u32(record, 104),
            "caller_return_address": _u32(record, 108),
            "virtual_key": _u16(record, 112),
            "flags": _u16(record, 114),
            "unicode": _u16(record, 116),
            "control": last_control,
            "event": last_event,
            "requested_stage": last_requested_stage,
            "effective_stage": last_effective_stage,
            "consumed": last_consumed,
            "filter_queued": last_filter_queued,
            "would_handle": last_would_handle,
            "coverflow_active": last_coverflow_active,
            "user_index": record[126],
            "hid_code": record[127],
        },
        "crc32": stored_crc,
    }


def generation_is_newer(candidate: int, reference: int) -> bool:
    """RFC-1982-style uint32 ordering used by the resident selector."""

    distance = (candidate - reference) & 0xFFFFFFFF
    return distance != 0 and distance < 0x80000000


def select_newest(
    slot_a: bytes | bytearray | memoryview | None,
    slot_b: bytes | bytearray | memoryview | None,
) -> dict[str, Any]:
    """Select the newest valid slot; equal/ambiguous generations prefer A."""

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
        details = "; ".join(f"slot {name}: {error}" for name, error in invalid.items())
        suffix = f" ({details})" if details else ""
        raise TelemetryDecodeError(f"no valid AZI2 telemetry slot{suffix}")

    selected = "A" if "A" in valid else "B"
    if "A" in valid and "B" in valid and generation_is_newer(
        valid["B"]["generation"], valid["A"]["generation"]
    ):
        selected = "B"

    output = {"selected_slot": selected, **valid[selected]}
    if invalid:
        output["invalid_slots"] = invalid
    return output


def _read(path: Path, slot_name: str) -> bytes:
    try:
        return path.read_bytes()
    except OSError as exc:
        raise TelemetryDecodeError(f"cannot read slot {slot_name} {path}: {exc}") from exc


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slot-a", type=Path, help="AuroraAZ-M2a-input-A.bin")
    parser.add_argument("--slot-b", type=Path, help="AuroraAZ-M2a-input-B.bin")
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
