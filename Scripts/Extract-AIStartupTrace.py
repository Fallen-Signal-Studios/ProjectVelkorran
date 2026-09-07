#!/usr/bin/env python3
"""Extract bounded native startup observations. Never infer a repaired or absent AI event."""
import argparse
import json
import math
from pathlib import Path

MARKER = "SOV_AI_STARTUP "


def extract(lines):
    captures = {}
    for line_number, line in enumerate(lines, 1):
        if MARKER not in line:
            continue
        payload = line.split(MARKER, 1)[1].strip()
        try:
            row = json.loads(payload)
        except json.JSONDecodeError as error:
            raise ValueError(f"Malformed trace at line {line_number}: {error}") from error
        required = {"schema", "capture", "sequence", "world", "elapsed_seconds", "game_seconds", "context", "event", "data"}
        if not isinstance(row, dict) or not required <= row.keys() or type(row["schema"]) is not int or row["schema"] != 1:
            raise ValueError(f"Unsupported or incomplete trace at line {line_number}")
        if (not isinstance(row["capture"], str) or not isinstance(row["world"], str)
                or type(row["sequence"]) is not int or row["sequence"] < 1
                or not isinstance(row["context"], str) or not isinstance(row["event"], str)
                or not isinstance(row["data"], dict)):
            raise ValueError(f"Invalid trace field types at line {line_number}")
        for clock in ("elapsed_seconds", "game_seconds"):
            if type(row[clock]) not in (int, float) or not math.isfinite(row[clock]) or row[clock] < 0:
                raise ValueError(f"Invalid trace clock at line {line_number}")
        key = (row["capture"], row["world"])
        captures.setdefault(key, []).append(row)
    if not captures:
        raise ValueError("No native startup trace found. Arm before a fresh PIE world; a late switch captures nothing.")
    result = []
    for (capture_id, world), rows in captures.items():
        sequences = [row["sequence"] for row in rows]
        if sequences != sorted(set(sequences)):
            raise ValueError(f"Duplicated or reordered sequence in capture {capture_id}; supply one editor log.")
        armed = rows[0]["event"] == "observer_armed" and rows[0]["sequence"] == 1
        continuous = armed and sequences == list(range(1, sequences[-1] + 1))
        stopped = rows[-1]["event"] == "capture_stopped"
        reason = rows[-1]["data"].get("reason") if stopped else None
        controllers = {}
        for row in rows:
            if row["event"] not in {"perception_attached", "perception_callback", "snapshot", "controller_begin_play_enter"}:
                continue
            item = controllers.setdefault(row["context"], {"attachment": None, "first_callback": None,
                "first_successful_sight": None, "first_snapshot": None, "latest_snapshot": None,
                "perception_before_attachment": "unknown", "callback_count": 0})
            event = row["event"]
            if event == "perception_attached" and item["attachment"] is None:
                item["attachment"] = row
            elif event == "perception_callback":
                item["callback_count"] += 1
                if item["first_callback"] is None:
                    item["first_callback"] = row
                if row["data"].get("success") and row["data"].get("sight") and item["first_successful_sight"] is None:
                    item["first_successful_sight"] = row
            elif event == "snapshot":
                if item["first_snapshot"] is None:
                    item["first_snapshot"] = row
                item["latest_snapshot"] = row
        result.append({"capture": capture_id, "world": world, "armed_before_world_initialization": armed,
            "sequence_continuous_from_arm": continuous, "capture_closed": stopped, "stop_reason": reason,
            "recording_complete_through_world_cleanup": armed and continuous and stopped and reason == "world_cleanup",
            "controllers": controllers, "event_count": len(rows), "events": rows})
    return {"schema": 1, "diagnosis": "not established by extraction",
        "limitations": ["No record before native perception attachment establishes whether a prior engine event occurred.",
            "Snapshots are sampled observations, not readiness or callback publication events.",
            "Publication hooks mark entry immediately before the existing delegate broadcast, not Blueprint listener completion.",
            "Callbacks run in native threat-listener order; they do not prove the Blueprint attack generator received the relay.",
            "A time/event limit, disabled trace, missing rows, or missing world cleanup leaves an incomplete recording."],
        "captures": result}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        report = extract(args.log.read_text(encoding="utf-8-sig", errors="replace").splitlines())
    except (OSError, ValueError) as error:
        parser.exit(2, f"{error}\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"Extracted {sum(c['event_count'] for c in report['captures'])} events in {len(report['captures'])} captures; no AI diagnosis asserted.")


if __name__ == "__main__":
    main()
