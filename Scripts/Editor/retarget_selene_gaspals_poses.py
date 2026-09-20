"""Retarget the GASP feminine/reference poses into owned Selene content."""
import json
import os
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/SeleneGASPALS'
report = dict(status='preflight', assets=[], gameplay_bound=False)
tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    source = unreal.load_asset('/GASPALS/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
    target = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SKM_Quinn')
    assert source and target
    paths = ['/GASPALS/OverlaySystem/Overlays/Bases/Feminine/Pose_Feminine_' + n
             for n in ('Stand_Idle', 'Stand_Move', 'Crouch')]
    paths += ['/GASPALS/OverlaySystem/BasePoses/Pose_Neutral_' + n
              for n in ('Stand_Idle', 'Stand_Move', 'Crouch_Idle', 'Crouch_Move')]
    clips = [unreal.load_asset(p) for p in paths]
    assert all(clips)
    for clip in clips:
        assert not unreal.EditorAssetLibrary.does_asset_exist(root + '/SovSelene_' + clip.get_name())
    rigs = []
    for name, mesh in [('IK_SeleneGASPSource', source), ('IK_SeleneGASPTarget', target)]:
        assert not unreal.EditorAssetLibrary.does_asset_exist(root + '/' + name)
        rig = tools.create_asset(name, root, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        control = unreal.IKRigController.get_controller(rig)
        control.set_skeletal_mesh(mesh)
        assert control.apply_auto_generated_retarget_definition(), name
        assert control.apply_auto_fbik(), name
        rigs.append(rig)
    rtg = tools.create_asset('RTG_SeleneGASPALS', root, unreal.IKRetargeter, unreal.IKRetargetFactory())
    control = unreal.IKRetargeterController.get_controller(rtg)
    control.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rigs[0])
    control.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rigs[1])
    control.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source)
    control.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target)
    control.add_default_ops()
    control.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    inputs = [unreal.AssetRegistryHelpers.create_asset_data(c) for c in clips]
    generated = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        inputs, source, target, rtg, prefix='SovSelene_',
        include_referenced_assets=False, overwrite_existing_files=False)
    assert len(generated) == len(clips)
    for data in generated:
        asset = data.get_asset()
        generated_path = asset.get_path_name()
        destination = root + '/' + asset.get_name()
        assert unreal.EditorAssetLibrary.rename_loaded_asset(asset, destination)
        assert asset.get_editor_property('skeleton') == target.get_editor_property('skeleton')
        assert asset.get_editor_property('sequence_length') > 0
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        report['assets'].append(dict(path=asset.get_path_name(), generated_path=generated_path,
            length=asset.get_editor_property('sequence_length'),
            additive_type=str(asset.get_editor_property('additive_anim_type'))))
    for asset in rigs + [rtg]:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        report['assets'].append(dict(path=asset.get_path_name()))
    report['status'] = 'retargeted_requires_pose_review_and_graph_integration'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (out / 'selene-pose-retarget.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
