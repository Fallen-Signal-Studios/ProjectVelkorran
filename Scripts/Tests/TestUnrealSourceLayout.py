"""Host checks for known unity/UHT source-layout hazards, not an Unreal build."""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
PRIVATE = ROOT / "Source/ProjectVelkorran/Private"


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
            path = ROOT / "Source/ProjectVelkorranTests/Private/Tests" / f"Sov{hero}PayloadRuntimeTests.cpp"
            text = path.read_text()
            namespace = re.search(r"namespace (Sov\w+PayloadTests)\s*\{", text)
            self.assertIsNotNone(namespace, f"Helper namespace missing in {path}")
            source += f"namespace {namespace[1]} {{\n"
            source += extract_helper(path, "Activate") + "\n"
            source += extract_helper(path, "Shield") + "\n}\n"
            registrations = text.split("IMPLEMENT_SIMPLE_AUTOMATION_TEST", 1)[1]
            self.assertNotRegex(registrations, r"(?<![:\w])(?:Activate\s*<|Shield\s*\()")
        self.compile_source(source)

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
