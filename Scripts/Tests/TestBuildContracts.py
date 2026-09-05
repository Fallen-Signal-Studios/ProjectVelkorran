#!/usr/bin/env python3
"""Source contracts and real host C++ unity repros; these are not UHT/UBT tests."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def unique_source(name):
    paths = list((ROOT / 'Source').rglob(name))
    if len(paths) != 1:
        raise AssertionError(f'Expected one source file for {name}: {paths}')
    return paths[0].read_text(encoding='utf-8-sig')


def function(source, return_type, name):
    match = re.search(r'\b' + return_type + r'\s+' + name + r'\([^\n]+\)\s*\{', source)
    if not match:
        raise AssertionError('Production function not found: ' + name)
    end = match.end()
    depth = 1
    while depth and end < len(source):
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    if depth:
        raise AssertionError('Unbalanced production function: ' + name)
    return source[match.start():end]


class BuildContracts(unittest.TestCase):
    def test_generated_headers_are_last_includes(self):
        violations = []
        for directory in ('Source', 'Plugins'):
            for path in (ROOT / directory).rglob('*.h'):
                text = path.read_text(encoding='utf-8-sig')
                includes = re.findall(r'^\s*#\s*include\s*[<"]([^">]+)[">]', text, re.M)
                generated = [i for i, name in enumerate(includes) if name.endswith('.generated.h')]
                if generated and generated[-1] != len(includes) - 1:
                    violations.append(str(path.relative_to(ROOT)))
        self.assertEqual(violations, [])

    def test_reflected_fixtures_are_editor_module_only(self):
        descriptor = json.loads((ROOT / 'ProjectVelkorran.uproject').read_text())
        modules = {module['Name']: module for module in descriptor['Modules']}
        self.assertEqual(modules.get('ProjectVelkorranTests', {}).get('Type'), 'Editor')
        self.assertIn('ProjectVelkorranTests', (ROOT / 'Source/ProjectVelkorranEditor.Target.cs').read_text())
        for directory in (ROOT / 'Source/ProjectVelkorran', ROOT / 'Plugins'):
            for path in directory.rglob('*.h'):
                if 'Tests' in path.parts:
                    self.assertNotRegex(path.read_text(), r'\bUCLASS\s*\(', str(path))

    def test_payload_helpers_do_not_create_unity_overloads(self):
        for file in ('SovSelenePayload.cpp', 'SovProtectionInterceptReceipt.cpp'):
            self.assertNotRegex(unique_source(file), r'\bAlive\s*\(')
        for file in ('SovTarrikPayloadRuntimeTests.cpp', 'SovSelenePayloadRuntimeTests.cpp'):
            self.assertNotRegex(unique_source(file), r'\b(?:Activate|Shield)\s*(?:<|\()')

    def test_exact_current_helpers_compile_together_and_preserve_semantics(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('A host C++ compiler is required for the narrow unity repro')
        shield_a = function(unique_source('SovTarrikPayloadRuntimeTests.cpp'), 'float', 'TarrikPayloadShield')
        shield_b = function(unique_source('SovSelenePayloadRuntimeTests.cpp'), 'float', 'SelenePayloadShield')
        selene = function(unique_source('SovSelenePayload.cpp'), 'bool', 'IsLivingSelenePayloadParticipant')
        protection = function(unique_source('SovProtectionInterceptReceipt.cpp'), 'bool', 'IsLivingProtectionParticipant')
        stubs = '''
#include <cassert>
constexpr float KINDA_SMALL_NUMBER = .0001f;
struct UNarrativeAttributeSetBase { static int GetShieldAttribute() { return 1; } static int GetHealthAttribute() { return 0; } };
struct UAbilitySystemComponent {
 bool bHasSet = false;
 template<class T> const T* GetSet() const { static T Value; return bHasSet ? &Value : nullptr; }
 bool HasMatchingGameplayTag(int) const { return false; }
 float GetNumericAttribute(int) const { return 100.f; }
};
struct ASovAxiomRuntimeTestCharacter { UAbilitySystemComponent ASC; UAbilitySystemComponent* GetNarrativeAbilitySystemComponent() { return &ASC; } };
template<class T> bool IsValid(const T* Value) { return Value != nullptr; }
struct FNarrativeGameplayTags { int State_IsDead=0; static const FNarrativeGameplayTags& Get(){ static FNarrativeGameplayTags V; return V; } };
struct FSovGameplayTags { int State_Fatal=1; static const FSovGameplayTags& Get(){ static FSovGameplayTags V; return V; } };
'''
        main = '''
int main() {
 UAbilitySystemComponent Source; const UAbilitySystemComponent* ConstSource = &Source;
 assert(IsLivingSelenePayloadParticipant(&Source));
 assert(IsLivingSelenePayloadParticipant(ConstSource));
#if INCLUDE_PROTECTION
 assert(!IsLivingProtectionParticipant(&Source));
 Source.bHasSet=true; assert(IsLivingProtectionParticipant(&Source));
#endif
 ASovAxiomRuntimeTestCharacter Character;
 assert(TarrikPayloadShield(&Character)==100.f); assert(SelenePayloadShield(&Character)==100.f);
}
'''
        source = stubs + '\nnamespace {\n' + shield_a + '\n}\nnamespace {\n' + shield_b + '\n}\n'
        source += '\n#if INCLUDE_PROTECTION\nnamespace {\n' + protection + '\n}\n#endif\nnamespace {\n' + selene + '\n}\n' + main
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'CurrentUnityHelpers.cpp'
            path.write_text(source)
            for included in (0, 1):
                executable = Path(directory) / ('unity' + str(included))
                built = subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', f'-DINCLUDE_PROTECTION={included}', str(path), '-o', str(executable)], capture_output=True, text=True)
                self.assertEqual(built.returncode, 0, built.stderr)
                self.assertEqual(subprocess.run([str(executable)], capture_output=True).returncode, 0)


if __name__ == '__main__':
    unittest.main()
