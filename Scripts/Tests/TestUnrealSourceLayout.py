"""Host checks for known unity/UHT source-layout hazards, not an Unreal build."""

from pathlib import Path
import json
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
PRIVATE = ROOT / "Source/ProjectVelkorran/Private"
TEST_PRIVATE = ROOT / "Source/ProjectVelkorranTests/Private"


def extract_helper(path, name):
    """Extract the complete source body of one of the audited free helpers."""
    source = path.read_text()
    declaration = re.search(
        r"^(?:\s*template<class T> )?\s*(?:T\*|float|bool)\s+"
        + re.escape(name) + r"\(", source, re.MULTILINE)
    if not declaration:
        raise AssertionError(f"Missing helper {name} in {path}")
    end = source.index("{", declaration.end()) + 1
    depth = 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[declaration.start():end].strip()


class UnrealSourceLayoutTests(unittest.TestCase):
    def test_native_registrations_have_unique_classes_and_names(self):
        classes, names = {}, {}
        for path in TEST_PRIVATE.rglob("*.cpp"):
            for cls, name in re.findall(
                    r'^\s*IMPLEMENT_(?:SIMPLE|COMPLEX)_AUTOMATION_TEST\s*\(\s*(\w+)\s*,\s*"([^"]+)"',
                    path.read_text(), re.MULTILINE):
                self.assertNotIn(cls, classes, f"{cls}: {classes.get(cls)} and {path}")
                self.assertNotIn(name, names, f"{name}: {names.get(name)} and {path}")
                classes[cls], names[name] = path, path
        self.assertGreater(len(names), 0)

    def test_editor_fixture_module_has_one_implementation(self):
        implementations = []
        for path in TEST_PRIVATE.rglob("*.cpp"):
            if re.search(r"IMPLEMENT_MODULE\s*\([^,]+,\s*ProjectVelkorranTests\s*\)",
                         path.read_text()):
                implementations.append(path)
        self.assertEqual(len(implementations), 1, implementations)

    def compile_source(self, source, run=False):
        compiler = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("A host C++ compiler is required for the unity-language check")
        with tempfile.TemporaryDirectory(prefix="velkorran-unity-") as directory:
            path = Path(directory) / "Check.cpp"
            path.write_text(source)
            executable = Path(directory) / "Check"
            command = [compiler, "-std=c++17", str(path)]
            command += ["-o", str(executable)] if run else ["-fsyntax-only"]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            if run:
                result = subprocess.run([str(executable)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_generated_header_is_last_include(self):
        count = 0
        for root in (ROOT / "Source", ROOT / "Plugins"):
            for path in root.rglob("*.h"):
                includes = re.findall(r'^\s*#include\s*[<"]([^">]+)[">]',
                                      path.read_text(errors="replace"), re.MULTILINE)
                if any(name.endswith(".generated.h") for name in includes):
                    count += 1
                    self.assertTrue(includes[-1].endswith(".generated.h"), str(path))
        self.assertGreater(count, 0, "The reflected-header scan must inspect actual headers")

    def test_payload_test_helpers_share_a_translation_unit(self):
        source = """
#define TEXT(x) x
constexpr int INDEX_NONE = -1;
struct UWeaponItem {};
struct FGameplayAbilitySpecHandle {};
struct FGameplayAbilitySpec {
 FGameplayAbilitySpec(int, int, int, UWeaponItem*);
 bool InputPressed; void* GetPrimaryInstance() const;
};
struct UNarrativeAttributeSetBase { static int GetShieldAttribute(); };
struct FStubASC {
 float GetNumericAttribute(int); FGameplayAbilitySpecHandle GiveAbility(FGameplayAbilitySpec);
 bool TryActivateAbility(FGameplayAbilitySpecHandle);
 FGameplayAbilitySpec* FindAbilitySpecFromHandle(FGameplayAbilitySpecHandle);
};
struct ASovAxiomRuntimeTestCharacter {
 FStubASC* GetNarrativeAbilitySystemComponent(); UWeaponItem* SetTestWeapon();
};
struct FAutomationTestBase { void AddError(const char*); bool TestTrue(const char*, bool); };
template<class T> T* Cast(void* p) { return static_cast<T*>(p); }
"""
        for hero in ("Tarrik", "Selene"):
            path = TEST_PRIVATE / "Tests" / f"Sov{hero}PayloadRuntimeTests.cpp"
            text = path.read_text()
            namespace = re.search(r"namespace (Sov\w+PayloadTests)\s*\{", text)
            self.assertIsNotNone(namespace, f"Helper namespace missing in {path}")
            source += f"namespace {namespace[1]} {{\n"
            source += extract_helper(path, "Activate") + "\n"
            source += extract_helper(path, "Shield") + "\n}\n"
            registrations = text.split("IMPLEMENT_SIMPLE_AUTOMATION_TEST", 1)[1]
            self.assertNotRegex(registrations, r"(?<![:\w])(?:Activate\s*<|Shield\s*\()")
        self.compile_source(source)

    def test_reflected_fixtures_are_excluded_from_game_modules(self):
        project = json.loads((ROOT / "ProjectVelkorran.uproject").read_text())
        module = next(item for item in project["Modules"] if item["Name"] == "ProjectVelkorranTests")
        self.assertEqual(module["Type"], "Editor")
        self.assertEqual(module["LoadingPhase"], "PostEngineInit")
        fixture_count = 0
        for source_root in (ROOT / "Source", ROOT / "Plugins"):
            for path in source_root.rglob("*.h"):
                if "Tests" not in path.parts:
                    continue
                if re.search(r"\bUCLASS\s*\(", path.read_text()):
                    fixture_count += 1
                    self.assertIn(TEST_PRIVATE, path.parents, str(path))
        self.assertGreater(fixture_count, 20, "Inspect the actual reflected test fixtures")
        for path in (ROOT / "Source").glob("*.Target.cs"):
            text = path.read_text()
            if "TargetType.Editor" not in text:
                self.assertNotIn('"ProjectVelkorranTests"', text, str(path))
        for path in (ROOT / "Source").rglob("*.Build.cs"):
            if path.parent.name != "ProjectVelkorranTests":
                self.assertNotIn('"ProjectVelkorranTests"', path.read_text(), str(path))

    def test_fixture_script_paths_follow_their_reflected_module(self):
        fixtures = set()
        for path in (TEST_PRIVATE / "Tests").glob("*.h"):
            fixtures.update(re.findall(r"class\s+[UA]([A-Za-z0-9_]+)\s*:", path.read_text()))
        self.assertIn("SovLifecycleTestGameMode", fixtures)
        for path in (TEST_PRIVATE / "Tests").glob("*.cpp"):
            for module, name in re.findall(r"/Script/([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)", path.read_text()):
                if name in fixtures:
                    self.assertEqual(module, "ProjectVelkorranTests", str(path))

    def test_living_policies_do_not_change_with_unity_composition(self):
        prefix = """
#include <cassert>
constexpr float KINDA_SMALL_NUMBER = 0.0001f;
struct UNarrativeAttributeSetBase { static int GetHealthAttribute() { return 0; } };
struct UAbilitySystemComponent {
 bool HasSet = false, Dead = false, Fatal = false; float Health = 100.f;
 template<class T> const T* GetSet() const { static T value; return HasSet ? &value : nullptr; }
 bool HasMatchingGameplayTag(int tag) const { return tag == 1 ? Dead : Fatal; }
 float GetNumericAttribute(int) const { return Health; }
};
template<class T> bool IsValid(T* p) { return p != nullptr; }
struct FNarrativeGameplayTags {
 int State_IsDead = 1; static FNarrativeGameplayTags Get() { return {}; }
};
struct FSovGameplayTags { int State_Fatal = 2; static FSovGameplayTags Get() { return {}; } };
"""
        helpers = [
            ("SovProtectionInterceptReceipt.cpp", "IsLivingProtectionParticipant", "false"),
            ("SovSelenePayload.cpp", "IsLivingSelenePayloadParticipant", "true"),
        ]
        # Each policy intentionally retains its own missing-attribute-set contract.
        # Test both include orders as well as separate compilation.
        for selection in ([helpers[0]], [helpers[1]], helpers, list(reversed(helpers))):
            with self.subTest(helpers=[helper[1] for helper in selection]):
                source = prefix
                for filename, name, _ in selection:
                    source += "namespace {\n" + extract_helper(PRIVATE / "Combat" / filename, name) + "\n}\n"
                source += "int main() { UAbilitySystemComponent asc;\n"
                for _, name, missing_set_result in selection:
                    source += f"""
asc = {{}};
assert(!{name}(nullptr));
assert({name}(&asc) == {missing_set_result});
asc.HasSet = true; assert({name}(&asc));
asc.Dead = true; assert(!{name}(&asc));
asc.Dead = false; asc.Fatal = true; assert(!{name}(&asc));
asc.Fatal = false; asc.Health = 0.f; assert(!{name}(&asc));
"""
                self.compile_source(source + "}\n", run=True)


if __name__ == "__main__":
    unittest.main()
