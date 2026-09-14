"""Create owned blood variants and a Parasites wall gait; preserve vendor assets."""
import json, os
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Aurelion/VFX/Blood/'
report = {'systems': {}, 'materials': {}, 'saved': []}
lib = unreal.EditorAssetLibrary
def duplicate(source, path):
    if lib.does_asset_exist(path):
        assert path.startswith(root) or path == '/Game/Aurelion/Enemies/Animation/AS_EclipseWallRun'
        return unreal.load_asset(path)
    asset = lib.duplicate_asset(source.get_path_name(), path)
    assert asset, path
    return asset
def save(asset):
    assert lib.save_loaded_asset(asset, only_if_is_dirty=False)
    report['saved'].append(asset.get_path_name())
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    materials = {}
    for color in ('Red','Black'):
        for kind, path in {'Hit':'Hit/Niagara/NS_BulletHit_High', 'Slash':'Slash/Niagara/NS_Slash_High',
                'Burst':'Burst/Niagara/NS_BloodBurst_High', 'Low':'Hit/Niagara/NS_BulletHit_Low'}.items():
            asset = duplicate(unreal.load_asset('/Game/RealisticBlood/' + path), root + 'NS_Aurelion_Blood' + color + kind)
            info = unreal.SovCombatFeedbackAuthoringLibrary.inspect_feedback_system(asset)
            disabled = []
            required = []
            for emitter in info.emitters:
                if str(emitter.name) == 'Smoke':
                    disabled.append(str(emitter.name))
                    continue
                required.append(str(emitter.name))
                for renderer_path in emitter.renderer_objects:
                    renderer = unreal.load_object(None, renderer_path)
                    assert renderer, renderer_path
                    material = renderer.get_editor_property('material')
                    if color == 'Black':
                        key = material.get_path_name()
                        if key not in materials:
                            owned = duplicate(material, root + 'Materials/MI_Eclipse_' + material.get_name())
                            names = {str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(owned)}
                            assert {'MainColor','SecondaryColor','SpecularTint'} <= names, (key, names)
                            for name, value in {'MainColor':unreal.LinearColor(.004,.004,.004,0),
                                    'SecondaryColor':unreal.LinearColor(.001,.001,.001,0),
                                    'SpecularTint':unreal.LinearColor(.32,.32,.32,0)}.items():
                                unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(owned, name, value)
                                actual = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(owned, name)
                                assert abs(actual.r-value.r) < .00001 and abs(actual.g-value.g) < .00001 and abs(actual.b-value.b) < .00001
                            unreal.MaterialEditingLibrary.update_material_instance(owned)
                            save(owned)
                            materials[key] = owned
                            report['materials'][key] = owned.get_path_name()
                        renderer.set_editor_property('material', materials[key])
            result = unreal.SovCombatFeedbackAuthoringLibrary.prepare_owned_feedback_system(asset, disabled, required, {}, {})
            assert result == '', result
            assert unreal.SovCombatFeedbackAuthoringLibrary.finish_feedback_compilation(asset)
            save(asset)
            report['systems'][color + kind] = unreal.SovCombatFeedbackAuthoringLibrary.inspect_feedback_system(asset).export_text()
    source = unreal.load_asset('/Game/Parasites_Pack/Animations/Parasite_Spider/Anim_Spider_Run')
    clip = duplicate(source, '/Game/Aurelion/Enemies/Animation/AS_EclipseWallRun')
    clip.set_editor_property('enable_root_motion', False)
    clip.set_editor_property('force_root_lock', True)
    assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton', clip.get_editor_property('skeleton'))
    factory.set_editor_property('source_animation', clip)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AM_EclipseWallRun', '/Game/Aurelion/Enemies/Animation', unreal.AnimMontage, factory)
    assert montage
    save(clip); save(montage)
    bp = unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionWallRunner')
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.get_wall_traversal().set_editor_property('wall_run_montage', montage)
    assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(bp)
    save(bp)
    report['wall_gait'] = montage.get_path_name()
    source = unreal.load_asset('/Game/Parasites_Pack/Animations/Parasites_Alfa/Animations_with_stand/Anim_Alfa_Stand_Attack_Fire')
    assert source and not unreal.AnimationLibrary.get_animation_notify_events(source)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton', source.get_editor_property('skeleton'))
    factory.set_editor_property('source_animation', source)
    support = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AM_EclipseWeaver_SupportCast', '/Game/Aurelion/Enemies/Animation', unreal.AnimMontage, factory)
    assert support
    bp = unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionWeaver')
    unreal.get_default_object(bp.generated_class()).set_editor_property('support_cast_montage', support)
    assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(bp)
    save(support); save(bp)
    report['support_cast'] = support.get_path_name()
    report['status'] = 'authored_requires_live_review'
except Exception:
    import traceback
    report['error'] = traceback.format_exc()
    report['status'] = 'failed'
finally:
    (out/'blood-wall-authoring.json').write_text(json.dumps(report, indent=2))
