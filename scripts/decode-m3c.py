#!/usr/bin/env python3
"""Decode AuroraAZ AZC3 v1 browse/settings telemetry."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct


FIELDS = (
    "version", "record_size", "operation_mode", "browse_bind_result",
    "browse_step_result", "consumer_queued", "consumer_no_match",
    "consumer_rejected", "input_queued", "input_applied",
    "input_rejected", "input_pending", "input_in_flight", "apply_reason",
    "apply_target", "apply_current", "apply_count", "label_result",
    "settings_hook_calls", "settings_requests_taken", "settings_pending",
    "settings_resolve_result", "settings_call_result",
    "settings_completions", "settings_dialog_active", "runtime_state",
    "shutdown_requests",
    "icon_cache_result", "icon_apply_result",
)

MODES = {1: "filter", 2: "browse"}
BROWSE_RESULTS = {
    0: "jump-queued", 1: "idle", 2: "deferred", 3: "no-match",
    4: "input-busy", 5: "bad-request", 6: "bad-bindings",
    7: "bad-list", 8: "race", 9: "publish-failed", 10: "cancelled",
}
APPLY_REASONS = {
    0: "none", 1: "bad-publication", 2: "bad-gcm", 3: "stock-gate",
    4: "bad-layout", 5: "bad-helper", 6: "already-selected",
    7: "move-rejected", 8: "moved",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("record", type=Path)
    args = parser.parse_args()
    data = args.record.read_bytes()
    if len(data) != 120 or data[:4] != b"AZC3":
        raise SystemExit("not an AZC3 v1 120-byte record")
    values = struct.unpack(">29I", data[4:])
    decoded = dict(zip(FIELDS, values))
    if decoded["version"] != 1 or decoded["record_size"] != 120:
        raise SystemExit("unsupported AZC3 record")
    decoded["operation_mode_name"] = MODES.get(decoded["operation_mode"], "unknown")
    decoded["browse_bind_name"] = BROWSE_RESULTS.get(decoded["browse_bind_result"], "unknown")
    decoded["browse_step_name"] = BROWSE_RESULTS.get(decoded["browse_step_result"], "unknown")
    decoded["apply_reason_name"] = APPLY_REASONS.get(decoded["apply_reason"], "unknown")
    print(json.dumps(decoded, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
