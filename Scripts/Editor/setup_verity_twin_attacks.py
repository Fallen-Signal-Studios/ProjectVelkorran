"""Wire retargeted Twin Blade attacks through the existing Verity melee ability."""
import json
import os
import shutil
import sys
import traceback
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/VerityTwinBlades'
report = dict(status='preflight', saved=[], montages=[], live_damage_qualified=False)

def duplicate(source, target):
    assert not unreal.EditorAssetLibrary.does_asset_exist(target), target
    result = unreal.EditorAssetLibrary.duplicate_asset(source, target)
    assert result, target
    return result

try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    project = Path(unreal.Paths.project_dir()).resolve()
    for rel in ('Content/Items/Weapons/WI_Verity.uasset', 'Content/Characters/Animation/ABP_SovVerityOverlay.uasset'):
        shutil.copy2(project / rel, out / Path(rel).name)
    source = unreal.load_asset('/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Melee/Abilities/GA_Attack_Melee_Verity')
    source_state = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(source)
    ability = duplicate(source.get_path_name().split('.')[0], root + '/GA_SovVerityTwinAttack')
    cdo = unreal.get_default_object(ability.generated_class())
    old_combo = list(cdo.get_editor_property('DefaultComboAnimations'))
    assert len(old_combo) == 1
    combo = duplicate(old_combo[0].get_path_name().split('.')[0], root + '/Combo_VerityTwinBlades')
    rows = list(combo.get_editor_property('character_anims'))
    assert len(rows) == 4, 'Expected four original combo entries'
    template = '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/3P/AM_Sword_3P_1H_Attack_1'
    # Pack-authored slash/trail intervals, retained as one native damage window per attack.
    windows = [(0.147672, 0.653118), (0.412897, 0.608858),
               (0.386700, 0.667903), (0.270228, 0.537798)]
    assets = []
    for i, row in enumerate(rows, 1):
        clip = unreal.load_asset(root + '/SovVerity_Twinblades_Attack_%02d' % i)
        assert clip
        montage = duplicate(template, root + '/AM_VerityTwin_%02d' % i)
        result = unreal.SovBlueprintAuthoringLibrary.configure_verity_twin_montage(montage, clip, *windows[i-1])
        assert result.succeeded, result.report
        row.set_editor_property('montage3p', montage)
        report['montages'].append(dict(path=montage.get_path_name(), contract=str(result.report)))
        assets.append(montage)
    combo.set_editor_property('character_anims', rows)
    cdo.set_editor_property('DefaultComboAnimations', [combo])
    item = unreal.load_asset('/Game/Items/Weapons/WI_Verity')
    result = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references([ability, item], [source], [ability])
    assert result.succeeded, result.report
    report['compiler'] = str(result.report)
    assert list(unreal.get_default_object(ability.generated_class()).get_editor_property('DefaultComboAnimations')) == [combo]
    assert ability.generated_class() in unreal.get_default_object(item.generated_class()).get_editor_property('weapon_abilities')
    from configure_aurelion_companion_equipment import replace_curated_primary
    companion_assets, report['companion_allowlists'] = replace_curated_primary(
        'Selene', source.generated_class(), ability.generated_class())
    assert unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(source) == source_state
    overlay = unreal.load_asset('/Game/Characters/Animation/ABP_SovVerityOverlay')
    unreal.get_default_object(overlay.generated_class()).set_editor_property('3P_Idle',
        unreal.load_asset(root + '/SovVerity_Twinblades_Idle'))
    unreal.BlueprintEditorLibrary.compile_blueprint(overlay)
    for asset in assets + [combo, ability, item, overlay] + companion_assets:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        report['saved'].append(asset.get_path_name())
    report['status'] = 'saved; live animation and damage review pending'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (out / 'verity-twin-attacks.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
