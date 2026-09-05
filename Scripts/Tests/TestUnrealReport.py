"""Negative tests for the actual report gate, using synthetic reports, not UE results."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest
import sys

sys.dont_write_bytecode = True

SPEC = importlib.util.spec_from_file_location("report_gate", Path(__file__).resolve().parents[1] / "Check-UnrealReport.py")
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


class ReportGateTests(unittest.TestCase):
    def setUp(self):
        self.expected = {"ProjectVelkorran.Campaign.Load", "ProjectVelkorran.World.Door"}
        self.report = {"succeeded": 2, "succeededWithWarnings": 0, "failed": 0, "notRun": 0, "inProcess": 0,
                       "tests": [{"fullTestPath": name, "state": "Success", "errors": 0, "entries": []}
                                 for name in sorted(self.expected)]}

    def check(self, report=None, expected=None, prefix="ProjectVelkorran"):
        return GATE.verify(self.report if report is None else report,
                           self.expected if expected is None else expected, prefix)

    def test_complete_success(self):
        self.assertEqual(self.check()["source_tests"], 2)

    def test_missing_source_test_rejects_stale_binary_report(self):
        self.report["tests"].pop(); self.report["succeeded"] = 1
        with self.assertRaisesRegex(GATE.ReportError, "missing"):
            self.check()

    def test_counter_tampering(self):
        for field in ("succeeded", "succeededWithWarnings", "failed", "notRun", "inProcess"):
            for value in (-1, None, "0", True, 0.5):
                with self.subTest(field=field, value=value), self.assertRaises(GATE.ReportError):
                    report = copy.deepcopy(self.report); report[field] = value; self.check(report)

    def test_incomplete_or_failure(self):
        for field in ("failed", "notRun", "inProcess"):
            report = copy.deepcopy(self.report); report[field] = 1
            with self.subTest(field=field), self.assertRaises(GATE.ReportError):
                self.check(report)

    def test_missing_counter(self):
        del self.report["failed"]
        with self.assertRaises(GATE.ReportError):
            self.check()

    def test_bad_total(self):
        self.report["succeeded"] = 3
        with self.assertRaisesRegex(GATE.ReportError, "totals"):
            self.check()

    def test_duplicate_rows(self):
        self.report["tests"][1] = copy.deepcopy(self.report["tests"][0])
        with self.assertRaisesRegex(GATE.ReportError, "Duplicate"):
            self.check()

    def test_non_success_state(self):
        for value in (None, "Fail", "NotRun", "InProcess", "Skipped"):
            report = copy.deepcopy(self.report); report["tests"][0]["state"] = value
            with self.subTest(state=value), self.assertRaises(GATE.ReportError):
                self.check(report)

    def test_hidden_error_event(self):
        self.report["tests"][0]["entries"] = [{"event": {"type": "Error", "message": "Actual engine error"}}]
        with self.assertRaisesRegex(GATE.ReportError, "hidden"):
            self.check()

    def test_missing_error_count(self):
        del self.report["tests"][0]["errors"]
        with self.assertRaises(GATE.ReportError):
            self.check()

    def test_nonzero_error_count(self):
        self.report["tests"][0]["errors"] = 1
        with self.assertRaises(GATE.ReportError):
            self.check()

    def test_empty_report(self):
        self.report["tests"] = []
        with self.assertRaises(GATE.ReportError):
            self.check()

    def test_filter_namespace_boundary(self):
        self.assertFalse(GATE.selected("ProjectVelkorran.CampaignExtra.Test", "ProjectVelkorran.Campaign"))
        self.assertTrue(GATE.selected("ProjectVelkorran.Campaign.Test", "projectvelkorran.campaign"))

    def test_focused_run(self):
        self.report["tests"] = self.report["tests"][:1]; self.report["succeeded"] = 1
        self.assertEqual(self.check(expected={"ProjectVelkorran.Campaign.Load"}, prefix="ProjectVelkorran.Campaign")["source_tests"], 1)

    def test_warning_success(self):
        self.report["succeeded"] = 1; self.report["succeededWithWarnings"] = 1
        self.report["tests"][0]["entries"] = [{"event": {"type": "Warning"}}]
        self.assertEqual(self.check()["warnings"], 1)

    def test_warning_cannot_be_hidden_by_successful_aggregate(self):
        self.report["tests"][0]["entries"] = [{"event": {"type": "Warning"}}]
        self.assertEqual(self.check()["warnings"], 1)

    def test_malformed_warning_counter_rejected(self):
        self.report["tests"][0]["warnings"] = -1
        with self.assertRaises(GATE.ReportError):
            self.check()

    def test_source_inventory_includes_plugin_and_ignores_comments(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root / "Source").mkdir(); (root / "Plugins").mkdir()
            (root / "Source/A.cpp").write_text('IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA, "ProjectVelkorran.A", Flags)\n'
                '// IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIgnore, "ProjectVelkorran.Ignore", Flags)\n'
                '/* IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIgnore2, "ProjectVelkorran.Ignore2", Flags) */', encoding="utf-8")
            (root / "Plugins/B.cpp").write_text('IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n FB, "ProjectVelkorran.B", Flags)', encoding="utf-8")
            self.assertEqual(GATE.source_tests(root, "ProjectVelkorran"), {"ProjectVelkorran.A", "ProjectVelkorran.B"})
            (root / "Plugins/C.cpp").write_text('IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC, "projectvelkorran.a", Flags)', encoding="utf-8")
            with self.assertRaisesRegex(GATE.ReportError, "Duplicate"):
                GATE.source_tests(root, "ProjectVelkorran")

    def test_no_inventory_rejected(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(GATE.ReportError):
            GATE.source_tests(Path(directory), "ProjectVelkorran")


if __name__ == "__main__":
    unittest.main()
