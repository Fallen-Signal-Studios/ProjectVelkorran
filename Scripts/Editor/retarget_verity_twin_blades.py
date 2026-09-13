"""Create project-owned Twin Blade clips with UE's IK retarget pipeline."""
import json
import os
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
report = dict(status='preflight', assets=[], gameplay_qualified=False)
root = '/Game/Characters/Animation/VerityTwinBlades'
pack = '/Game/TwinBladesBundle/TwinBladesAndTwinSword/TwinbladesBase/Animation'
tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    source = unreal.load_asset('/Game/TwinBladesBundle/Demo/Characters/Mannequins/Meshes/SKM_Manny')
    target = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SKM_Quinn')
    assert source and target
    rigs = []
    for name, mesh in [('IK_VerityTwinSource', source), ('IK_VerityTwinTarget', target)]:
        assert not unreal.EditorAssetLibrary.does_asset_exist(root + '/' + name)
        rig = tools.create_asset(name, root, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        controller = unreal.IKRigController.get_controller(rig)
        controller.set_skeletal_mesh(mesh)
        assert controller.apply_auto_generated_retarget_definition(), name
        assert controller.apply_auto_fbik(), name
        rigs.append(rig)
    rtg = tools.create_asset('RTG_VerityTwinBlades', root, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(rtg)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rigs[0])
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rigs[1])
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source)
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target)
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    clips = ['Twinblades_Idle', 'Movement/Twinblades_Walk_F', 'Movement/Twinblades_Run_F']
    clips += ['Attack/Twinblades_Attack_%02d' % i for i in range(1, 5)]
    inputs = [unreal.AssetRegistryHelpers.create_asset_data(unreal.load_asset(pack + '/' + p)) for p in clips]
    generated = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        inputs, source, target, rtg, prefix='SovVerity_', include_referenced_assets=False,
        overwrite_existing_files=False)
    assert len(generated) == len(clips), 'Retarget output count mismatch'
    for data in generated:
        asset = data.get_asset()
        name = asset.get_name()
        destination = root + '/' + name
        assert not unreal.EditorAssetLibrary.does_asset_exist(destination)
        assert unreal.EditorAssetLibrary.rename_loaded_asset(asset, destination)
        assert asset.get_editor_property('skeleton') == target.get_editor_property('skeleton')
        assert asset.get_editor_property('sequence_length') > 0
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        report['assets'].append(asset.get_path_name())
    for asset in rigs + [rtg]:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        report['assets'].append(asset.get_path_name())
    report['status'] = 'retargeted; not yet connected to gameplay'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (out / 'verity-retarget.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
