"""Give only Verity's weapon overlay a Twin Blade idle/walk/run stance blend."""
import json
import os
import shutil
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/VerityTwinBlades'
report = dict(status='preflight', saved=[], live_qualified=False)
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    overlay = unreal.load_asset('/Game/Characters/Animation/ABP_SovVerityOverlay')
    backup = Path(unreal.Paths.project_dir()).resolve() / 'Content/Characters/Animation/ABP_SovVerityOverlay.uasset'
    shutil.copy2(backup, out / backup.name)
    parent = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Overlays/Weapons/ABP_Biped_Overlay_Melee')
    before = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(parent)
    assert not unreal.EditorAssetLibrary.does_asset_exist(root + '/ABP_VerityTwinMeleeBase')
    copy = unreal.EditorAssetLibrary.duplicate_asset(parent.get_path_name().split('.')[0], root + '/ABP_VerityTwinMeleeBase')
    factory = unreal.BlendSpaceFactoryNew()
    factory.set_editor_property('target_skeleton', copy.get_editor_property('target_skeleton'))
    blend = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BS_VerityTwinStance', root, unreal.BlendSpace, factory)
    clips = [unreal.load_asset(root + '/SovVerity_Twinblades_' + name) for name in ('Idle', 'Walk_F', 'Run_F')]
    result = unreal.SovBlueprintAuthoringLibrary.configure_verity_twin_locomotion(copy, blend, clips)
    assert result.succeeded, result.report
    report['stance_compiler'] = str(result.report)
    result = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references([copy, overlay], [parent], [copy])
    assert result.succeeded, result.report
    report['overlay_compiler'] = str(result.report)
    assert before == unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(parent)
    assert unreal.get_default_object(overlay.generated_class()).get_editor_property('3P_Idle') == clips[0]
    for asset in (blend, copy, overlay):
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        report['saved'].append(asset.get_path_name())
    report['status'] = 'saved; Verity weapon stance only; directional footwork remains in base locomotion'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (out / 'verity-twin-locomotion.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
