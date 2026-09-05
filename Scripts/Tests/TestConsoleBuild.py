#!/usr/bin/env python3
"""Descriptor failure cases plus the actual Mass source preprocessor matrix."""

import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("console_build", ROOT / "Scripts/Check-ConsoleBuild.py")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class ConsoleBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.project = self.root / "Example.uproject"
        self.project.write_text(json.dumps({"Plugins": [{"Name": "NarrativePro", "Enabled": True}]}))
        self.plugin = self.root / "Plugins/NarrativePro/NarrativePro.uplugin"
        self.plugin.parent.mkdir(parents=True)
        self.data = {"Modules": [{"Name": name, "Type": "Runtime"}
                                 for name in sorted(CHECK.REQUIRED_NARRATIVE_MODULES)]}
        self.save()

    def tearDown(self):
        self.temp.cleanup()

    def save(self):
        self.plugin.write_text(json.dumps(self.data))

    def findings(self, platform="FixtureConsole"):
        return CHECK.audit(self.project, platform, [])

    def test_portable_descriptor_does_not_claim_compilation(self):
        findings = self.findings()
        self.assertFalse(any(item["severity"] == "blocker" for item in findings))
        self.assertTrue(any(item["code"] == "licensed-build-required" for item in findings))

    def test_required_runtime_module_excluded(self):
        self.data["Modules"][0]["PlatformAllowList"] = ["Win64"]
        self.save()
        self.assertIn("required-module-filter", [item["code"] for item in self.findings()])

    def test_unspecified_platform_does_not_guess_vendor_support(self):
        self.data["Modules"][0]["PlatformAllowList"] = ["Win64"]
        self.save()
        self.assertIn("required-module-platform-review", [item["code"] for item in self.findings(None)])

    def test_explicit_empty_platforms_fail_closed(self):
        self.data["Modules"][0]["HasExplicitPlatforms"] = True
        self.save()
        self.assertIn("required-module-filter", [item["code"] for item in self.findings()])

    def test_editor_dependency_is_not_a_console_game_dependency(self):
        self.data["Plugins"] = [{"Name": "EditorOnlyMissing", "Enabled": True, "TargetAllowList": ["Editor"]}]
        self.save()
        self.assertFalse(any(item["code"] == "plugin-unresolved" for item in self.findings()))

    def test_missing_runtime_dependency_blocks(self):
        self.data["Plugins"] = [{"Name": "ZenDyn", "Enabled": True}]
        self.save()
        self.assertTrue(any(item["code"] == "plugin-unresolved" and item["severity"] == "blocker"
                            for item in self.findings()))

    def test_project_descriptor_supported_platforms_checked(self):
        self.data["SupportedTargetPlatforms"] = ["Win64"]
        self.save()
        self.assertIn("plugin-platform-filter", [item["code"] for item in self.findings()])

    def test_template_descriptor_does_not_resolve_missing_plugin(self):
        nested = self.plugin.parent / "Resources/PluginTemplates/ZenDyn/ZenDyn.uplugin"
        nested.parent.mkdir(parents=True)
        nested.write_text("{}")
        self.assertNotIn("ZenDyn", CHECK.discover_plugins(self.root / "Plugins"))

    def test_deny_list_wins(self):
        self.assertFalse(CHECK.allowed({"PlatformAllowList": ["FixtureConsole"],
                                        "PlatformDenyList": ["FixtureConsole"]}, "Platform", "FixtureConsole"))

    def test_production_mass_debug_reference_has_a_declaration_in_all_configurations(self):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            self.skipTest("A C++ preprocessor is required for the production source matrix")
        source = (ROOT / "Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/AI/Mass/IncomingCollisionProcessor.cpp").read_text(encoding="utf-8-sig")
        # Run the real file's conditional compilation, omitting only unavailable UE headers.
        source = re.sub(r'^\s*#include[^\n]*', '', source, flags=re.MULTILINE)
        for mass_debug in (0, 1):
            for visual_log in (0, 1):
                with self.subTest(mass_debug=mass_debug, visual_log=visual_log):
                    result = subprocess.run([compiler, "-E", "-x", "c++", "-P",
                                             f"-DWITH_MASSGAMEPLAY_DEBUG={mass_debug}",
                                             f"-DENABLE_VISUAL_LOG={visual_log}", "-"],
                                            input=source, capture_output=True, text=True, check=True)
                    code = result.stdout
                    reference = "MassDebuggerSubsystem.GetSelectedEntity()" in code
                    declaration = "const UMassDebuggerSubsystem& MassDebuggerSubsystem" in code
                    self.assertFalse(reference and not declaration)
                    self.assertEqual(reference, bool(mass_debug and visual_log))


if __name__ == "__main__":
    unittest.main()
