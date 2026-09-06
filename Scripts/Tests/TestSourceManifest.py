"""Build attestation rejects changed inputs, missing tests and edited evidence."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("manifest", Path(__file__).resolve().parents[1] / "Capture-SourceManifest.py")
MANIFEST = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MANIFEST)


class SourceManifestTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="velkorran-manifest-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "Source/Game/Private/Tests/Example.cpp"
        self.source.parent.mkdir(parents=True)
        self.source.write_text('IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest, "ProjectVelkorran.Campaign.Example", 0)\n')

    def test_generated_logs_do_not_change_attested_code(self):
        before = MANIFEST.capture(self.root)
        saved = self.root / "Source/Game/Saved/Log.json"
        saved.parent.mkdir(parents=True)
        saved.write_text('{"log":1}')
        MANIFEST.verify(before, MANIFEST.capture(self.root))

    def test_modified_runtime_code_rejects_prior_manifest(self):
        runtime = self.root / "Source/Game/Private/Runtime.cpp"
        runtime.write_text("void Run() {}\n")
        before = MANIFEST.capture(self.root)
        runtime.write_text("void Run() { Changed(); }\n")
        with self.assertRaisesRegex(ValueError, "Runtime.cpp"):
            MANIFEST.verify(before, MANIFEST.capture(self.root))

    def test_linked_source_directory_cannot_hide_mutating_code(self):
        with tempfile.TemporaryDirectory(prefix="velkorran-external-") as directory:
            external = Path(directory)
            (external / "Feature.cpp").write_text("void Feature() {}")
            for link in (self.root / "Source/External", self.root / "Plugins"):
                with self.subTest(link=link):
                    try:
                        link.symlink_to(external, target_is_directory=True)
                    except OSError as error:
                        self.skipTest(f"Directory symlink unavailable: {error}")
                    try:
                        with self.assertRaisesRegex(ValueError, "link/reparse"):
                            MANIFEST.capture(self.root)
                    finally:
                        link.unlink()

    def test_new_untracked_test_cannot_hide_behind_existing_inventory(self):
        before = MANIFEST.capture(self.root)
        (self.source.parent / "Added.cpp").write_text('IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdded, "ProjectVelkorran.Campaign.Added", 0)\n')
        with self.assertRaisesRegex(ValueError, "Added.cpp"):
            MANIFEST.verify(before, MANIFEST.capture(self.root))

    def test_removed_build_input_is_detected(self):
        target = self.root / "Source/Game.Target.cs"
        target.write_text("class GameTarget {}")
        before = MANIFEST.capture(self.root)
        target.unlink()
        with self.assertRaisesRegex(ValueError, "Game.Target.cs"):
            MANIFEST.verify(before, MANIFEST.capture(self.root))

    def test_modified_manifest_body_is_rejected(self):
        actual = MANIFEST.capture(self.root)
        forged = copy.deepcopy(actual)
        forged["files"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "fingerprint/counts"):
            MANIFEST.verify(forged, actual)

    def test_missing_or_duplicate_registrations_fail_capture(self):
        self.source.write_text("// no native registrations\n")
        with self.assertRaises(ValueError):
            MANIFEST.capture(self.root)
        self.source.write_text('IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOne, "ProjectVelkorran.Campaign.Duplicate", 0)\nIMPLEMENT_SIMPLE_AUTOMATION_TEST(FTwo, "ProjectVelkorran.Campaign.Duplicate", 0)\n')
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            MANIFEST.capture(self.root)


if __name__ == "__main__":
    unittest.main()
