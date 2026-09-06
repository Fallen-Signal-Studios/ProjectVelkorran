"""Exercise runner control flow with test doubles. These tests never execute Unreal."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location("campaign_runner", Path(__file__).resolve().parents[1] / "Validate-Campaign.py")
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class CampaignQualificationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="campaign runner with spaces ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.project = self.root / "Project With Spaces/ProjectVelkorran.uproject"
        self.project.parent.mkdir()
        self.engine = self.root / "UE 5.7"
        self.output = self.root / "Evidence"
        self.manifest = self.root / "missions.json"
        self.write(self.project, json.dumps({"EngineAssociation": "5.7", "Plugins": [{"Name": "NarrativePro", "Enabled": True}]}))
        self.write(self.engine / "Engine/Build/Build.version", '{"MajorVersion":5,"MinorVersion":7,"PatchVersion":4}')
        self.plugin = self.project.parent / "Plugins/Narrative/NarrativePro.uplugin"
        self.write(self.plugin, '{"FileVersion":3}')
        self.content = self.project.parent / "Content/UnitTestOnly.uasset"
        self.write(self.content, "test-double data, not a valid Unreal asset")
        self.write(self.plugin.parent / "Content/UnitTestOnly.uasset", "test-double data")
        self.source = self.project.parent / "Source/Tests/Test.cpp"
        self.write(self.source, 'IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest, "ProjectVelkorran.Campaign.One", 0)')
        self.write(self.manifest, '["/Game/Missions/DA_Test.DA_Test"]')
        for target in ("Mac", "Win64"):
            for path in RUNNER.platform_tools(self.engine, target).values():
                self.write(path, "not executable; test double only")
        self.calls = []

    def write(self, path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def args(self, *extra):
        return RUNNER.parser().parse_args(["--engine-root", str(self.engine), "--project", str(self.project),
            "--platform", "Mac", "--mission-manifest", str(self.manifest), "--output", str(self.output), *extra])

    def report(self, log, tests=("ProjectVelkorran.Campaign.One",)):
        self.write(log.parent / "AutomationReport/index.json", json.dumps({"succeeded": len(tests),
            "succeededWithWarnings": 0, "failed": 0, "notRun": 0, "inProcess": 0,
            "tests": [{"fullTestPath": name, "state": "Success", "errors": 0} for name in tests]}))

    def fake_engine(self, argv, cwd, log, timeout):
        self.calls.append((argv, cwd, log, timeout))
        self.write(log, "UNIT TEST DOUBLE: ENGINE UNEXECUTED\n")
        if log.stem == "native-automation":
            self.report(log)
        if log.stem == "campaign-preflight":
            self.write(log, "UNIT TEST DOUBLE: ENGINE UNEXECUTED\nNative mission preflight: 1 assets, 0 errors.\n")
        return {"exit_code": 0, "timed_out": False}

    def assert_unqualified(self, summary):
        for field in ("builds_passed", "native_tests_passed", "campaign_preflight_passed", "package_produced",
                      "shipping_candidate_qualified", "packaged_playthrough_verified", "engine_processes_started"):
            self.assertFalse(summary[field], field)

    def test_complete_fake_run_retains_evidence_but_never_claims_engine_execution(self):
        code, directory, summary = RUNNER.run(self.args(), self.fake_engine, "Darwin")
        self.assertEqual(code, 0)
        self.assertEqual(summary["status"], "test_double_completed")
        self.assertEqual(summary["execution"], "test_double_engine_unexecuted")
        self.assert_unqualified(summary)
        self.assertEqual(len(self.calls), 4)
        self.assertEqual(summary["source_integrity"], "unchanged")
        self.assertEqual(summary["native_test_count"], 1)
        self.assertEqual(RUNNER.read_json(directory / "summary.json"), summary)
        self.assertTrue((directory / "source-after.json").is_file())
        self.assertTrue((directory / "content-after.json").is_file())
        self.assertEqual(summary["stages"][2]["coverage"]["source_tests"], 1)

    def test_mac_command_arguments_preserve_spaces_and_nonunity_for_both_targets(self):
        args = self.args("--non-unity", "--package", "--map", "/Game/Maps/M12", "--map", "/Game/Maps/M13",
                         "--additional-asset", "/Game/Dialogue/DA_Voice.DA_Voice")
        stages = RUNNER.stage_plan(args, self.output, RUNNER.supplied_assets(args))
        self.assertEqual(stages[0]["argv"][:2], ["/bin/bash", str(self.engine / "Engine/Build/BatchFiles/Mac/Build.sh")])
        self.assertIn("-DisableUnity", stages[0]["argv"])
        self.assertIn("-DisableUnity", stages[1]["argv"])
        self.assertIn(f"-Project={self.project}", stages[0]["argv"])
        self.assertEqual(stages[2]["argv"][1], str(self.project))
        self.assertIn("-ShippingValidation", stages[3]["argv"])
        self.assertIn("-AdditionalAssets=/Game/Dialogue/DA_Voice.DA_Voice", stages[3]["argv"])
        self.assertIn("-map=/Game/Maps/M12+/Game/Maps/M13", stages[4]["argv"])
        self.assertIn("-clientconfig=Development", stages[4]["argv"])

    def test_windows_batch_quoting_and_editor_paths(self):
        args = self.args("--platform", "Win64")
        stage = RUNNER.stage_plan(args, self.output, RUNNER.supplied_assets(args))[0]
        command = RUNNER.process_command(stage["argv"], True)
        self.assertIn('/d /v:off /s /c ""', command)
        self.assertIn(f'"-Project={self.project}"', command)
        self.assertIn('"C:/UE(test)/Build.bat"', RUNNER.process_command(["C:/UE(test)/Build.bat", "Target"], True))
        self.assertTrue(RUNNER.platform_tools(self.engine, "Win64")["editor"].name.endswith("-Cmd.exe"))
        for unsafe in ("C:/percent%name", "C:/bang!name", "C:/amp&name", 'C:/quote"name'):
            with self.subTest(path=unsafe), self.assertRaisesRegex(ValueError, "metacharacters"):
                RUNNER.process_command(["Build.bat", unsafe], True)

    def test_dry_run_executes_nothing_and_grants_no_acceptance(self):
        code, _, summary = RUNNER.run(self.args("--dry-run"), self.fake_engine, "Darwin")
        self.assertEqual(code, 0)
        self.assertEqual(summary["status"], "planned_only")
        self.assertEqual(self.calls, [])
        self.assert_unqualified(summary)
        self.assertTrue(all(stage["status"] == "not_run" for stage in summary["stages"]))

    def test_wrong_host_or_engine_version_retains_failed_preflight(self):
        for host, version in (("Linux", (5, 7)), ("Darwin", (5, 6))):
            with self.subTest(host=host, version=version):
                self.write(self.engine / "Engine/Build/Build.version", json.dumps({"MajorVersion": version[0], "MinorVersion": version[1]}))
                code, directory, summary = RUNNER.run(self.args(), self.fake_engine, host)
                self.assertEqual(code, 2)
                self.assertTrue(summary["preflight"]["errors"])
                self.assertTrue((directory / "summary.json").exists())
                self.assertEqual(self.calls, [])

    def test_missing_plugin_and_content_fail_before_build(self):
        self.plugin.unlink()
        self.content.unlink()
        code, _, summary = RUNNER.run(self.args(), self.fake_engine, "Darwin")
        self.assertEqual(code, 2)
        self.assertIn("Enabled plugin is unavailable", summary["error"])
        self.assertIn("Restore actual Project content", summary["error"])
        self.assertEqual(self.calls, [])

    def test_platform_excluded_plugin_is_not_required(self):
        descriptor = RUNNER.read_json(self.project)
        descriptor["Plugins"].append({"Name": "WindowsOnly", "Enabled": True, "PlatformAllowList": ["Win64"]})
        self.write(self.project, json.dumps(descriptor))
        result = RUNNER.preflight(self.args(), "Darwin")
        self.assertEqual(result["errors"], [])
        result = RUNNER.preflight(self.args("--platform", "Win64"), "Windows")
        self.assertIn("WindowsOnly", "; ".join(result["errors"]))

    def test_build_only_does_not_require_content_or_run_other_gates(self):
        self.content.unlink()
        code, _, summary = RUNNER.run(self.args("--build-only"), self.fake_engine, "Darwin")
        self.assertEqual(code, 0)
        self.assertEqual([stage["name"] for stage in summary["stages"]], ["editor-build", "game-build"])
        self.assert_unqualified(summary)

    def test_package_requires_explicit_maps_and_real_object_path_syntax(self):
        for extra in (("--package",), ("--package", "--build-only", "--map", "/Game/Maps/M12"),
                      ("--map", "/Game/Maps/M12.M12")):
            with self.subTest(extra=extra):
                code, _, summary = RUNNER.run(self.args(*extra), self.fake_engine, "Darwin")
                self.assertEqual(code, 2)
                self.assertEqual(self.calls, [])
                self.assert_unqualified(summary)
        self.write(self.manifest, '["/Game/DA_Test.WrongObject"]')
        self.assertEqual(RUNNER.run(self.args(), self.fake_engine, "Darwin")[0], 2)

    def test_failed_build_halts_later_stages_and_retains_exit(self):
        def fail(argv, cwd, log, timeout):
            self.fake_engine(argv, cwd, log, timeout)
            return {"exit_code": 6, "timed_out": False}
        code, _, summary = RUNNER.run(self.args(), fail, "Darwin")
        self.assertEqual(code, 2)
        self.assertEqual(summary["stages"][0]["exit_code"], 6)
        self.assertEqual([stage["status"] for stage in summary["stages"]], ["failed", "not_run", "not_run", "not_run"])
        self.assertEqual(len(self.calls), 1)

    def test_timeout_retains_partial_stages_and_never_claims_success(self):
        def timeout(argv, cwd, log, limit):
            result = self.fake_engine(argv, cwd, log, limit)
            return {"exit_code": 124, "timed_out": True} if log.stem == "native-automation" else result
        code, _, summary = RUNNER.run(self.args(), timeout, "Darwin")
        self.assertEqual(code, 2)
        self.assertEqual([stage["status"] for stage in summary["stages"]], ["passed", "passed", "failed", "not_run"])
        self.assertTrue(summary["stages"][2]["timed_out"])
        self.assert_unqualified(summary)

    def test_old_successful_report_is_never_reused(self):
        self.report(self.output / "old-run/native-automation.log")
        def no_report(argv, cwd, log, timeout):
            return {"exit_code": 0, "timed_out": False}
        code, directory, summary = RUNNER.run(self.args(), no_report, "Darwin")
        self.assertEqual(code, 2)
        self.assertNotEqual(directory.name, "old-run")
        self.assertEqual(summary["stages"][2]["status"], "failed")

    def test_zero_exit_with_partial_report_fails_inventory_gate(self):
        def partial(argv, cwd, log, timeout):
            result = self.fake_engine(argv, cwd, log, timeout)
            if log.stem == "native-automation":
                self.report(log, ("ProjectVelkorran.Campaign.Other",))
            return result
        code, _, summary = RUNNER.run(self.args(), partial, "Darwin")
        self.assertEqual(code, 2)
        self.assertIn("Native tests missing", summary["error"])
        self.assertEqual(summary["stages"][3]["status"], "not_run")

    def test_zero_exit_without_commandlet_completion_marker_fails(self):
        def incomplete(argv, cwd, log, timeout):
            result = self.fake_engine(argv, cwd, log, timeout)
            if log.stem == "campaign-preflight":
                self.write(log, "Engine started but commandlet did not finish")
            return result
        code, _, summary = RUNNER.run(self.args(), incomplete, "Darwin")
        self.assertEqual(code, 2)
        self.assertIn("did not record complete", summary["error"])

    def test_source_and_content_drift_invalidate_an_otherwise_complete_run(self):
        for path in (self.source, self.content):
            with self.subTest(path=path):
                original = path.read_text()
                def drift(argv, cwd, log, timeout):
                    result = self.fake_engine(argv, cwd, log, timeout)
                    if log.stem == "campaign-preflight":
                        path.write_text(original + "\n// changed\n")
                    return result
                code, _, summary = RUNNER.run(self.args(), drift, "Darwin")
                self.assertEqual(code, 2)
                self.assertEqual(summary["source_integrity"], "failed")
                self.assert_unqualified(summary)
                path.write_text(original)

    def test_successful_uat_exit_without_archive_fails(self):
        code, _, summary = RUNNER.run(self.args("--package", "--map", "/Game/Maps/M12"), self.fake_engine, "Darwin")
        self.assertEqual(code, 2)
        self.assertIn("no archived package", summary["error"])

    def test_fake_archive_is_hashed_but_never_qualifies_engine_or_playthrough(self):
        def package(argv, cwd, log, timeout):
            result = self.fake_engine(argv, cwd, log, timeout)
            if log.stem == "package":
                self.write(log.parent / "Archive/Mac/ProjectVelkorran.app/Contents/MacOS/ProjectVelkorran", "fake executable")
                self.write(log.parent / "Archive/Mac/ProjectVelkorran/Content/Paks/Fake.pak", "fake container")
            return result
        code, directory, summary = RUNNER.run(self.args("--package", "--map", "/Game/Maps/M12"), package, "Darwin")
        self.assertEqual(code, 0)
        self.assertEqual(len(RUNNER.read_json(directory / "archive-inventory.json")), 2)
        self.assertIn("unverified", summary["stages"][-1]["archive"]["fixture_exclusion"])
        self.assert_unqualified(summary)

    def test_editor_test_module_in_archive_is_rejected(self):
        self.write(self.output / "Archive/Mac/ProjectVelkorranTests.dylib", "unexpected test module")
        with self.assertRaisesRegex(ValueError, "Editor test module"):
            RUNNER.package_inventory(self.args(), self.output)

    def test_mac_framework_links_are_attested_but_external_links_fail(self):
        archive = self.output / "Archive"
        executable = archive / "ProjectVelkorran.app/Contents/MacOS/ProjectVelkorran"
        framework = archive / "ProjectVelkorran.app/Contents/Frameworks/Example.framework"
        self.write(executable, "fake executable")
        self.write(archive / "Content/Paks/Fake.pak", "fake container")
        self.write(framework / "Versions/A/Example", "fake framework")
        link = framework / "Versions/Current"
        try:
            link.symlink_to("A", target_is_directory=True)
        except OSError as error:
            self.skipTest(f"Symlinks unavailable: {error}")
        RUNNER.package_inventory(self.args(), self.output)
        records = RUNNER.read_json(self.output / "archive-inventory.json")
        self.assertEqual(next(row for row in records if row["path"] == str(link))["symlink_target"], "A")
        link.unlink()
        link.symlink_to(self.engine, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "escapes the archive"):
            RUNNER.package_inventory(self.args(), self.output)

    def test_linked_archive_root_cannot_reuse_an_old_package(self):
        old = self.root / "OldArchive"
        self.write(old / "ProjectVelkorran.exe", "old fake executable")
        self.write(old / "Content/Fake.pak", "old fake container")
        self.output.mkdir()
        try:
            (self.output / "Archive").symlink_to(old, target_is_directory=True)
        except OSError as error:
            self.skipTest(f"Symlinks unavailable: {error}")
        with self.assertRaisesRegex(ValueError, "root cannot be linked/reparse"):
            RUNNER.package_inventory(self.args("--platform", "Win64"), self.output)

    def test_directory_symlink_named_as_executable_cannot_satisfy_package_gate(self):
        archive = self.output / "Archive"
        directory = archive / "NotAnExecutable"
        directory.mkdir(parents=True)
        self.write(archive / "Content/Fake.pak", "fake container")
        try:
            (archive / "ProjectVelkorran.exe").symlink_to(directory, target_is_directory=True)
        except OSError as error:
            self.skipTest(f"Symlinks unavailable: {error}")
        with self.assertRaisesRegex(ValueError, "lacks the expected game executable"):
            RUNNER.package_inventory(self.args("--platform", "Win64"), self.output)

    def test_iostore_requires_nonempty_matching_data_for_each_table(self):
        archive = self.output / "Archive"
        self.write(archive / "ProjectVelkorran.exe", "fake executable")
        self.write(archive / "Content/Fake.utoc", "fake table")
        self.write(archive / "Content/Other.ucas", "wrong data file")
        for data in (None, ""):
            with self.subTest(data=data):
                if data is not None:
                    self.write(archive / "Content/Fake.ucas", data)
                with self.assertRaisesRegex(ValueError, "IoStore containers require"):
                    RUNNER.package_inventory(self.args("--platform", "Win64"), self.output)
        self.write(archive / "Content/Fake.ucas", "matching fake data")
        RUNNER.package_inventory(self.args("--platform", "Win64"), self.output)
        self.write(archive / "Content/Orphan.utoc", "second fake table")
        self.write(archive / "Content/Fallback.pak", "fake pak does not repair orphan IoStore data")
        with self.assertRaisesRegex(ValueError, "IoStore containers require"):
            RUNNER.package_inventory(self.args("--platform", "Win64"), self.output)

    def test_empty_executable_or_pak_cannot_satisfy_package_gate(self):
        archive = self.output / "Archive"
        for executable, container in (("", "fake container"), ("fake executable", "")):
            with self.subTest(executable=executable, container=container):
                self.write(archive / "ProjectVelkorran.exe", executable)
                self.write(archive / "Content/Fake.pak", container)
                with self.assertRaisesRegex(ValueError, "lacks the expected game executable"):
                    RUNNER.package_inventory(self.args("--platform", "Win64"), self.output)

    def test_process_launch_error_is_retained_without_following_stages(self):
        def fail_launch(argv, cwd, log, timeout):
            raise OSError("fixture launch unavailable")
        code, _, summary = RUNNER.run(self.args(), fail_launch, "Darwin")
        self.assertEqual(code, 2)
        self.assertEqual(summary["stages"][0]["status"], "failed")
        self.assertIn("fixture launch unavailable", summary["error"])
        self.assertTrue(all(stage["status"] == "not_run" for stage in summary["stages"][1:]))


if __name__ == "__main__":
    unittest.main()
