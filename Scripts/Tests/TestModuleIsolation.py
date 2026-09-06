"""Failure injection for the source/module and supplied build-evidence checker."""
import copy
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("isolation", ROOT / "Scripts/Check-TestModuleIsolation.py")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class ModuleIsolationTests(unittest.TestCase):
    def setUp(self):
        self.receipt = {"TargetName": "ProjectVelkorran", "Platform": "Win64", "Configuration": "Shipping",
                        "BuildProducts": [{"Type": "Executable", "Path": "ProjectVelkorran.exe"}]}
        self.manifest = {"TargetName": "ProjectVelkorran", "Modules": [
            {"Name": "ProjectVelkorran"}, {"Name": "NarrativeArsenal"}]}
        self.stage = "ProjectVelkorran/Content/Paks/ProjectVelkorran-Win64.pak\t2026-09-06\n"

    def verify(self):
        return CHECK.verify_packaged_metadata(self.receipt, self.manifest, self.stage,
            {"USovFixtureProbe"}, "Win64", "Shipping")

    def test_actual_source_is_isolated(self):
        report = CHECK.audit_source(ROOT)
        self.assertGreater(len(report["fixture_classes"]), 100)
        self.assertGreater(report["native_registrations"], 294)

    def test_valid_metadata_does_not_claim_runtime_validation(self):
        self.assertEqual(self.verify()["runtime_validation"], "not performed")

    def test_editor_receipt_rejected(self):
        self.receipt["TargetName"] = "ProjectVelkorranEditor"
        with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_wrong_configuration_and_platform_rejected(self):
        for field, value in (("Configuration", "Development"), ("Platform", "Mac")):
            saved = copy.deepcopy(self.receipt)
            self.receipt[field] = value
            with self.assertRaises(CHECK.IsolationError): self.verify()
            self.receipt = saved

    def test_empty_build_products_rejected(self):
        self.receipt["BuildProducts"] = []
        with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_fixture_module_in_uht_rejected(self):
        self.manifest["Modules"].append({"Name": CHECK.MODULE})
        with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_fixture_header_in_runtime_uht_rejected(self):
        self.manifest["Modules"][0]["PrivateHeaders"] = ["Source/ProjectVelkorran/Private/Tests/SovFixture.h"]
        with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_windows_fixture_header_rejected_after_json_escaping(self):
        self.manifest["Modules"][0]["PrivateHeaders"] = [r"C:\Project\Source\ProjectVelkorran\Private\Tests\SovFixture.h"]
        with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_fixture_class_in_staged_evidence_rejected(self):
        self.stage += "/Script/ProjectVelkorran.USovFixtureProbe\n"
        with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_editor_module_binary_in_stage_rejected(self):
        self.stage += "Binaries/Win64/UnrealEditor-ProjectVelkorranTests.dll\n"
        with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_reflected_name_without_native_prefix_rejected(self):
        self.stage += "/Script/ProjectVelkorran.SovFixtureProbe\n"
        with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_empty_or_unrelated_stage_rejected(self):
        for value in ("", "readme.txt\n"):
            self.stage = value
            with self.assertRaises(CHECK.IsolationError): self.verify()

    def test_wrong_or_incomplete_uht_manifest_rejected(self):
        self.manifest["TargetName"] = "ProjectVelkorranEditor"
        with self.assertRaises(CHECK.IsolationError): self.verify()
        self.manifest["TargetName"] = "ProjectVelkorran"
        self.manifest["Modules"] = [{"Name": "Unrelated"}]
        with self.assertRaises(CHECK.IsolationError): self.verify()


if __name__ == "__main__":
    unittest.main()
