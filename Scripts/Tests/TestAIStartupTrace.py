"""Failure injection for missing, truncated and misordered AI trace evidence."""
import importlib.util
import json
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("ai_trace", Path(__file__).resolve().parents[1] / "Extract-AIStartupTrace.py")
TRACE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRACE)


def row(sequence, event, context="controller", data=None, capture="1", world="PIE_0"):
    return "[log] SOV_AI_STARTUP " + json.dumps({"schema": 1, "capture": capture, "sequence": sequence,
        "world": world, "elapsed_seconds": sequence / 10, "game_seconds": sequence / 10,
        "context": context, "event": event, "data": data or {}})


class AIStartupTraceTests(unittest.TestCase):
    def test_closed_sequence_preserves_raw_order_and_does_not_diagnose(self):
        lines = [row(1, "observer_armed"), row(2, "perception_attached"),
            row(3, "perception_callback", data={"success": True, "sight": True}),
            row(4, "capture_stopped", data={"reason": "world_cleanup"})]
        report = TRACE.extract(lines)
        capture = report["captures"][0]
        self.assertTrue(capture["recording_complete_through_world_cleanup"])
        self.assertEqual(capture["controllers"]["controller"]["first_successful_sight"]["sequence"], 3)
        self.assertEqual(capture["controllers"]["controller"]["perception_before_attachment"], "unknown")
        self.assertEqual(report["diagnosis"], "not established by extraction")

    def test_missing_arm_and_gap_never_claim_complete_capture(self):
        for lines in ([row(2, "perception_attached"), row(3, "capture_stopped", data={"reason": "world_cleanup"})],
                      [row(1, "observer_armed"), row(3, "capture_stopped", data={"reason": "world_cleanup"})]):
            self.assertFalse(TRACE.extract(lines)["captures"][0]["recording_complete_through_world_cleanup"])

    def test_no_callback_does_not_imply_no_prior_perception(self):
        capture = TRACE.extract([row(1, "observer_armed"), row(2, "snapshot")])["captures"][0]
        self.assertIsNone(capture["controllers"]["controller"]["first_callback"])
        self.assertEqual(capture["controllers"]["controller"]["perception_before_attachment"], "unknown")
        self.assertFalse(capture["capture_closed"])

    def test_each_limit_is_incomplete(self):
        for reason in ("time_limit", "event_limit", "disabled"):
            capture = TRACE.extract([row(1, "observer_armed"), row(2, "capture_stopped", data={"reason": reason})])["captures"][0]
            self.assertTrue(capture["capture_closed"])
            self.assertFalse(capture["recording_complete_through_world_cleanup"])

    def test_malformed_or_absent_trace_fails_closed(self):
        for lines in (["normal log"], ["SOV_AI_STARTUP {"], ["SOV_AI_STARTUP {}"]):
            with self.assertRaises(ValueError):
                TRACE.extract(lines)

    def test_invalid_clocks_rejected(self):
        for clock in ("elapsed_seconds", "game_seconds"):
            for value in ("later", -1, float("nan"), float("inf")):
                payload = json.loads(row(1, "observer_armed").split(TRACE.MARKER, 1)[1])
                payload[clock] = value
                with self.assertRaises(ValueError):
                    TRACE.extract([TRACE.MARKER + json.dumps(payload)])

    def test_duplicate_and_reordered_rows_rejected(self):
        for numbers in ((1, 1), (2, 1)):
            with self.assertRaises(ValueError):
                TRACE.extract([row(number, "snapshot") for number in numbers])

    def test_pie_worlds_and_cold_retries_are_kept_separate(self):
        report = TRACE.extract([row(1, "observer_armed", capture="1"),
            row(1, "observer_armed", capture="2", world="PIE_1"),
            row(2, "snapshot", capture="1"), row(2, "snapshot", capture="2", world="PIE_1")])
        self.assertEqual(len(report["captures"]), 2)
        self.assertEqual([capture["event_count"] for capture in report["captures"]], [2, 2])


if __name__ == "__main__":
    unittest.main()
