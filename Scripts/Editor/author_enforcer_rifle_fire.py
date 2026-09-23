"""Retarget an owned rifle-fire clip and bind shot-driven Enforcer recoil."""
import json
import hashlib
import os
import traceback
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
folder = '/Game/Aurelion/Enemies/Animation'
source_path = '/Game/Sci_Fi_Characters_Pack/AnimDemoScene/Animations/Fire_Rifle_Ironsights'
source_mesh_path = '/Game/Sci_Fi_Characters_Pack/AnimDemoScene/Mesh/SK_Mannequin'
target_mesh_path = '/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SKM_Manny'
retargeter_path = '/Game/SciFiSoldier/Meshes/RTG_UE4UE5'
clip_path = folder + '/AS_AurelionEnforcer_RifleFire'
montage_path = folder + '/AM_AurelionEnforcer_RifleFire'
bp_path = '/Game/Aurelion/Enemies/BP_AurelionEnforcer'
report = dict(status='running', source=source_path, clip=clip_path, montage=montage_path,
              blueprint=bp_path)

try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    lib = unreal.EditorAssetLibrary
    map_file = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
    map_hash = hashlib.sha256(map_file.read_bytes()).hexdigest()
    source = unreal.load_asset(source_path)
    source_mesh = unreal.load_asset(source_mesh_path)
    target_mesh = unreal.load_asset(target_mesh_path)
    retargeter = unreal.load_asset(retargeter_path)
    bp = unreal.load_asset(bp_path)
    assert all((source, source_mesh, target_mesh, retargeter, bp))
    assert source.get_editor_property('skeleton') == source_mesh.get_editor_property('skeleton')
    assert not unreal.AnimationLibrary.get_animation_notify_events(source)
    assert not source.get_editor_property('enable_root_motion')
    cdo = unreal.get_default_object(bp.generated_class())
    if lib.does_asset_exist(montage_path):
        clip = unreal.load_asset(clip_path)
        montage = unreal.load_asset(montage_path)
        report['already_authored'] = True
    else:
        assert cdo.get_editor_property('weapon_fire_montage') is None, 'Existing Enforcer montage must be reviewed first'
        if lib.does_asset_exist(clip_path):
            # A failed montage-slot attempt may leave our validated retargeted clip.
            clip = unreal.load_asset(clip_path)
            report['resumed_verified_clip'] = True
        else:
            generated = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                [lib.find_asset_data(source_path)], source_mesh, target_mesh, retargeter,
                prefix='SovEnforcerFire_', include_referenced_assets=False,
                overwrite_existing_files=False)
            assert len(generated) == 1, 'Retarget should create exactly one clip'
            clip = generated[0].get_asset()
            assert clip and clip.get_editor_property('skeleton') == target_mesh.get_editor_property('skeleton')
            assert lib.rename_loaded_asset(clip, clip_path)
        assert clip and clip.get_editor_property('skeleton') == target_mesh.get_editor_property('skeleton')
        clip.set_editor_property('enable_root_motion', False)
        clip.set_editor_property('force_root_lock', True)
        assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton', clip.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation', clip)
        montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'AM_AurelionEnforcer_RifleFire', folder, unreal.AnimMontage, factory)
        assert montage
        result = unreal.SovBlueprintAuthoringLibrary.configure_enemy_cast_montage(
            montage, clip, 0., clip.get_play_length(), 1., 'UpperBody')
        assert result.succeeded, result.report
        for asset in (clip, montage):
            assert lib.save_loaded_asset(asset, only_if_is_dirty=False)
        cdo.set_editor_property('weapon_fire_montage', montage)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.get_default_object(bp.generated_class()).get_editor_property('weapon_fire_montage') == montage
        assert lib.save_loaded_asset(bp, only_if_is_dirty=False)
    assert clip and montage and clip.get_editor_property('skeleton') == target_mesh.get_editor_property('skeleton')
    assert montage.get_editor_property('skeleton') == clip.get_editor_property('skeleton')
    assert unreal.get_default_object(bp.generated_class()).get_editor_property('weapon_fire_montage') == montage
    assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
    assert not clip.get_editor_property('enable_root_motion') and clip.get_editor_property('force_root_lock')
    assert hashlib.sha256(map_file.read_bytes()).hexdigest() == map_hash, 'Authoring changed the map file'
    report.update(status='passed', clip_seconds=clip.get_play_length(),
                  target_skeleton=clip.get_editor_property('skeleton').get_path_name(),
                  notify_count=len(unreal.AnimationLibrary.get_animation_notify_events(clip)),
                  root_motion=clip.get_editor_property('enable_root_motion'),
                  blueprint_montage=montage.get_path_name(),
                  dirty_maps_in_editor=[str(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()])
except Exception:
    report.update(status='failed', error=traceback.format_exc())
finally:
    (out / 'enforcer-rifle-fire-authoring.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
