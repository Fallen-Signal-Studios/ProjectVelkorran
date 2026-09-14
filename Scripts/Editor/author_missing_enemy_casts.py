"""Fill audited Hound bite and Handler command montage gaps without changing payload timers."""
import json, os, shutil
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Aurelion/Enemies/Animation/'
report = {}
cases = [
    ('HoundBite', '/Game/FantasyBeast04/Animations/A_FantasyBeast04_attack01',
     '/NarrativePro/Pro/Core/Abilities/GameplayAbilities/DominionHound/GA_DominionHoundBite', 'attack_montage',
     '/Game/FantasyBeast04/Animations/Appearance_DominionHound', 'DefaultSlot', .8),
    ('HandlerCommand', '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Generic/Lyra/MM_Rifle_Equip',
     '/Game/Characters/DominionHandler/GA_DominionHandler_CommandHound', 'command_montage',
     '/Game/Characters/DominionHandler/Appearance_DominionHandler', 'FullBody', .8),
]
try:
    for name, source_path, ability_path, prop, appearance_path, slot, duration in cases:
        source = unreal.load_asset(source_path)
        appearance = unreal.load_asset(appearance_path)
        mesh = appearance.get_editor_property('character_attributes').get_editor_property('base_mesh')
        bp = unreal.load_asset(ability_path)
        cdo = unreal.get_default_object(bp.generated_class())
        existing = cdo.get_editor_property(prop)
        if existing:
            assert existing.get_path_name() == root+'AM_'+name+'.AM_'+name
            report[name] = {'montage':existing.get_path_name(),'already_authored':True}
            continue
        retargeted = None
        if source.get_editor_property('skeleton') != mesh.get_editor_property('skeleton'):
            assert name == 'HandlerCommand'
            source_mesh = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SKM_Quinn')
            tools = unreal.AssetToolsHelpers.get_asset_tools()
            rigs = []
            for suffix, rig_mesh in (('Source',source_mesh),('Target',mesh)):
                rig = tools.create_asset('IK_HandlerCast'+suffix,root.rstrip('/'),unreal.IKRigDefinition,unreal.IKRigDefinitionFactory())
                controller = unreal.IKRigController.get_controller(rig)
                controller.set_skeletal_mesh(rig_mesh)
                controller.apply_auto_generated_retarget_definition()
                controller.apply_auto_fbik()
                rigs.append(rig)
            retargeter = tools.create_asset('RTG_HandlerCast',root.rstrip('/'),unreal.IKRetargeter,unreal.IKRetargetFactory())
            controller = unreal.IKRetargeterController.get_controller(retargeter)
            for side, rig, rig_mesh in ((unreal.RetargetSourceOrTarget.SOURCE,rigs[0],source_mesh),(unreal.RetargetSourceOrTarget.TARGET,rigs[1],mesh)):
                controller.set_ik_rig(side,rig)
                controller.set_preview_mesh(side,rig_mesh)
            controller.add_default_ops()
            controller.auto_map_chains(unreal.AutoMapChainType.EXACT,True)
            generated = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                [unreal.EditorAssetLibrary.find_asset_data(source_path)],source_mesh,mesh,retargeter,
                prefix='SovHandlerCast_',include_referenced_assets=False,overwrite_existing_files=False)
            assert len(generated) == 1
            retargeted = generated[0].get_asset()
            assert unreal.EditorAssetLibrary.rename_loaded_asset(retargeted,root+'AS_'+name)
            for asset in rigs+[retargeter]:
                assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
        package = ability_path + '.uasset'
        if package.startswith('/Game/'):
            disk = Path(unreal.Paths.project_dir())/'Content'/package[len('/Game/'):]
        else:
            disk = Path(unreal.Paths.project_dir())/'Plugins/Narrativeed3f9374a6eV6/Content'/package[len('/NarrativePro/'):]
        shutil.copy2(disk, out/(name+'-before.uasset'))
        if retargeted:
            clip = retargeted
        else:
            assert not unreal.EditorAssetLibrary.does_asset_exist(root+'AS_'+name)
            clip = unreal.EditorAssetLibrary.duplicate_asset(source_path, root+'AS_'+name)
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton', mesh.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation', clip)
        montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AM_'+name,root.rstrip('/'),unreal.AnimMontage,factory)
        result = unreal.SovBlueprintAuthoringLibrary.configure_enemy_cast_montage(montage,clip,0.,clip.get_play_length(),clip.get_play_length()/duration,slot)
        assert result.succeeded, result.report
        for asset in (clip,montage):
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
        cdo.set_editor_property(prop,montage)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.get_default_object(bp.generated_class()).get_editor_property(prop) == montage
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
        report[name] = {'montage':montage.get_path_name(),'source':source_path,'slot':slot,'duration':duration,'skeleton':mesh.get_editor_property('skeleton').get_path_name()}
except Exception:
    import traceback
    report['error'] = traceback.format_exc()
finally:
    (out/'missing-enemy-casts.json').write_text(json.dumps(report,indent=2))
