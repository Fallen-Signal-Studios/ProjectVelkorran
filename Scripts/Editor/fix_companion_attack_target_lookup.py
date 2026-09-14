"""Bridge only the legacy attack-goal failure to the validated command target."""
import json
import os
from pathlib import Path
import re
import shutil
import traceback
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
report = dict(status='running', live_damage_qualified=False)
bp = unreal.load_asset('/NarrativePro/Pro/Core/Abilities/GameplayAbilities/Attacks/GA_CombatAbilityBase')
assert bp
asset = Path(unreal.Paths.project_dir()).resolve() / (
    'Plugins/Narrativeed3f9374a6eV6/Content/Pro/Core/Abilities/GameplayAbilities/Attacks/GA_CombatAbilityBase.uasset')
backup = out / 'GA_CombatAbilityBase-before.uasset'
assert asset.is_file() and not backup.exists()
shutil.copy2(asset, backup)


def export(name):
    path = out / (name+'.t3d')
    assert not path.exists()
    task = unreal.AssetExportTask()
    for key, value in dict(object=bp, exporter=unreal.ObjectExporterT3D(), filename=str(path),
                           automated=True, prompt=False, selected=False, replace_identical=False).items():
        task.set_editor_property(key, value)
    assert unreal.Exporter.run_asset_export_task(task)
    data = path.read_bytes()
    return data.decode('utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig').replace('\r\n', '\n')


try:
    before = export('target-before')
    result = unreal.SovBlueprintAuthoringLibrary.add_companion_attack_target_fallback(bp)
    report['compiler'] = str(result.report)
    assert result.succeeded, report['compiler']
    after = export('target-after')
    pattern = r'^   Begin Object Name="([^"]+)"[^\n]*\n.*?^   End Object'
    def sections(text):
        return {m.group(1): m.group(0) for m in re.finditer(pattern, text, re.M | re.S)}
    old, new = sections(before), sections(after)
    assert old.keys() == new.keys(), 'Unexpected graph inventory change'
    changed = [key for key in old if old[key] != new[key]]
    report['changed_sections'] = changed
    assert changed == ['GetBotAttackTarget'], 'Changes escaped the target function'
    assert 'SovCompanionCommandTarget' in new['GetBotAttackTarget']
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    report['status'] = 'saved'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (out/'companion-target-authoring.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
