"""Fresh-load asset checks, followed by the complete enemy definition inventory."""
import json, os, runpy
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
report = {'blood':{}, 'roles':{}}
for color in ('Red','Black'):
    for kind in ('Hit','Slash','Burst','Low'):
        name = 'NS_Aurelion_Blood' + color + kind
        asset = unreal.load_asset('/Game/Aurelion/VFX/Blood/' + name)
        assert asset and unreal.SovCombatFeedbackAuthoringLibrary.finish_feedback_compilation(asset)
        info = unreal.SovCombatFeedbackAuthoringLibrary.inspect_feedback_system(asset)
        count = 0
        for emitter in info.emitters:
            if not emitter.enabled:
                continue
            for path in emitter.renderer_objects:
                renderer = unreal.load_object(None, path)
                material = renderer.get_editor_property('material')
                if color == 'Black':
                    assert material.get_path_name().startswith('/Game/Aurelion/VFX/Blood/Materials/')
                    for parameter in ('MainColor','SecondaryColor'):
                        value = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material, parameter)
                        assert max(value.r,value.g,value.b) <= .00401 and abs(value.r-value.g) < .00001 and abs(value.g-value.b) < .00001
                count += 1
        assert count > 0
        report['blood'][name] = {'ready':info.ready,'renderers':count}
for role in ('Linkbound','WallRunner','Weaver','Elite','SecurityDrone','ContaminatedDrone','Enforcer'):
    bp = unreal.load_asset('/Game/Aurelion/Enemies/BP_Aurelion' + role)
    cdo = unreal.get_default_object(bp.generated_class())
    blood = cdo.get_component_by_class(unreal.SovBloodFeedbackComponent)
    report['roles'][role] = {'blood':blood.get_editor_property('enabled'), 'black':blood.get_editor_property('black_blood')}
    dismemberment = cdo.get_component_by_class(unreal.SovDismembermentComponent)
    if dismemberment:
        puddle = dismemberment.get_editor_property('death_blood_puddle_material')
        profile = dismemberment.get_editor_property('dismemberment_profile')
        report['roles'][role]['existing_puddle'] = puddle.get_path_name() if puddle else None
        report['roles'][role]['existing_sever_profile'] = profile.get_path_name() if profile else None
    assert report['roles'][role]['black'] == (role in ('Linkbound','WallRunner','Weaver','Elite'))
    if role == 'WallRunner':
        montage = cdo.get_wall_traversal().get_editor_property('wall_run_montage')
        assert montage
        report['wall_montage'] = montage.get_path_name()
    if role == 'Weaver':
        montage = cdo.get_editor_property('support_cast_montage')
        assert montage
        report['support_montage'] = montage.get_path_name()
report['filled_casts'] = {}
for name, path, prop in (
    ('HoundBite','/NarrativePro/Pro/Core/Abilities/GameplayAbilities/DominionHound/GA_DominionHoundBite','attack_montage'),
    ('HandlerCommand','/Game/Characters/DominionHandler/GA_DominionHandler_CommandHound','command_montage')):
    bp = unreal.load_asset(path)
    montage = unreal.get_default_object(bp.generated_class()).get_editor_property(prop)
    clip = unreal.load_asset('/Game/Aurelion/Enemies/Animation/AS_'+name)
    assert montage and clip and montage.get_editor_property('skeleton') == clip.get_editor_property('skeleton')
    assert not unreal.AnimationLibrary.get_animation_notify_events(montage)
    assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
    assert not clip.get_editor_property('enable_root_motion') and clip.get_editor_property('force_root_lock')
    report['filled_casts'][name] = montage.get_path_name()
report['legacy_drone_referencers'] = [str(n) for n in unreal.AssetRegistryHelpers.get_asset_registry().get_referencers(
    '/Game/SciFi_Drone_1/Narrative/NPC_ReformationDrone', unreal.AssetRegistryDependencyOptions())]
(out/'blood-casts-review.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/audit_all_enemy_casts.py'))
