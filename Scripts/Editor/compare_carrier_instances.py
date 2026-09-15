"""Compare saved carrier HISMs against temporary unit-scale static-mesh actors."""
from pathlib import Path
import json,os,runpy,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(sub.get_all_level_actors());assert len(actors)==3140
runpy.run_path(str(root/'Scripts/Editor/check_carrier_kit.py'))['check_carrier_kit'](actors)
visuals=[];temporary=[]
for a in actors:
    if a.get_actor_label().startswith('Aurelion_Carrier_'):
        c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        stable='_Stable' in a.get_actor_label()
        visuals.append((c,c.get_editor_property('visible'),stable))
        if stable:
            t=c.get_instance_transform(0,world_space=True)
            replacement=sub.spawn_actor_from_class(unreal.StaticMeshActor,t.translation,t.rotation.rotator())
            replacement.set_actor_label('TEMP_CarrierUnit_'+a.get_actor_label())
            replacement.set_actor_scale3d(t.scale3d)
            mesh=replacement.static_mesh_component;mesh.set_static_mesh(c.static_mesh)
            mesh.set_collision_profile_name('NoCollision');mesh.set_editor_property('can_ever_affect_navigation',False)
            mesh.set_cast_shadow(c.get_editor_property('cast_shadow'));mesh.set_visibility(False)
            temporary.append(replacement)
assert len(temporary)==4
def instance_mode(name):
    for c,visible,stable in visuals:c.set_visibility(stable and name=='original-hism')
    for a in temporary:a.static_mesh_component.set_visibility(name=='unit-static-mesh')
def restore_instances():
    for c,visible,stable in visuals:c.set_visibility(visible)
    for a in temporary:assert sub.destroy_actor(a)
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[(mode,(6500,10000,2000),(-12,-55),75) for mode in ('original-hism','unit-static-mesh')]\n"
suffix=suffix.replace("if state['phase']==0:","if state['phase']==0:\n            instance_mode(name)")
suffix=suffix.replace('restore_capture_settings();editor.eject_pilot_level_actor();','restore_instances();restore_capture_settings();editor.eject_pilot_level_actor();')
suffix=suffix.replace('==capture_actor_count','==3140')
suffix=suffix.replace('except Exception:\n        restore_capture_settings()','except Exception:\n        restore_instances();restore_capture_settings()')
exec(compile(prefix+views+'state=dict'+suffix,'carrier_instance_capture','exec'),globals())
