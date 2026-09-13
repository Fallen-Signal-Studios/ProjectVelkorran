"""Replace the isolated 112-instance ship dressing; retain all physical actors.

Preview is undoable and intentionally unsaved pending viewport review.
"""
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[2]
out=root/'Saved/Validation/Aurelion/Shuttles-20260913'; out.mkdir(parents=True,exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=subsystem.get_all_level_actors(); by_label={a.get_actor_label():a for a in actors}
owned={'ART_DepartureShuttle_'+f for f in ('Dominion','Reformation')} | {'ENVL_DepartureShuttle_'+f for f in ('Dominion','Reformation')}
def physical_snapshot():
    result={}
    for a in subsystem.get_all_level_actors():
        if a.get_actor_label() in owned: continue
        result[a.get_name()]=dict(label=a.get_actor_label(),transform=a.get_actor_transform().export_text(),components={c.get_name():dict(transform=c.get_world_transform().export_text(),collision=str(c.get_collision_enabled()),instances=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())] if isinstance(c,unreal.InstancedStaticMeshComponent) else None) for c in a.get_components_by_class(unreal.PrimitiveComponent)})
    return result
before=physical_snapshot()
baseline=out/'physical-baseline.json'
if baseline.exists():
    previous=json.loads(baseline.read_text())
    # UDS updates these non-colliding editor icons with camera/sky reconstruction.
    key='Ultra_Dynamic_Sky_C_0'
    for component in ('BillboardComponent_5','BillboardComponent_6','BillboardComponent_7'):
        assert before[key]['components'][component]['collision']=='<CollisionEnabled.NO_COLLISION: 0>'
        previous[key]['components'][component]['transform']=before[key]['components'][component]['transform']
    (out/'baseline-differences.json').write_text(json.dumps({k:dict(before=previous.get(k),after=v) for k,v in before.items() if previous.get(k)!=v},indent=2),encoding='utf8')
    assert previous==before,'Original physical geometry changed since baseline'
else: baseline.write_text(json.dumps(before,indent=2),encoding='utf8')
old=by_label['Aurelion_Art_M13_Z12_5_9dba0c']
components=[c for c in old.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()=='HierarchicalInstancedStaticMesh']
assert len(components)==1
old_component=components[0]
assert old_component.get_instance_count()==112
assert old_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
for i in range(112):
    p=old_component.get_instance_transform(i,world_space=True).translation
    assert abs(abs(p.x)-3800)<1100 and abs(p.y-47500)<850 and 40<p.z<560
with unreal.ScopedEditorTransaction('Replace departure shuttle dressing'):
    old_component.set_visibility(False); old_component.set_hidden_in_game(True)
    for faction,x in (('Dominion',-3800),('Reformation',3800)):
        label='ART_DepartureShuttle_'+faction
        mesh=unreal.load_asset('/Game/Aurelion/Environment/Blender/Shuttles/SM_Aurelion_'+faction+'_Shuttle'); assert mesh
        actor=by_label.get(label)
        if actor: assert isinstance(actor,unreal.StaticMeshActor) and actor.static_mesh_component.static_mesh==mesh
        else: actor=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,47500,0))
        actor.set_actor_label(label); actor.set_folder_path('Aurelion/EnvironmentArt/Z12/Shuttles')
        actor.set_actor_location(unreal.Vector(x,47500,0),False,False)
        actor.set_actor_rotation(unreal.Rotator(yaw=180),False)
        actor.set_actor_scale3d(unreal.Vector(1,1,1))
        c=actor.static_mesh_component; c.set_static_mesh(mesh)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); actor.set_actor_enable_collision(False)
        c.set_visibility(True); c.set_hidden_in_game(False)
        label='ENVL_DepartureShuttle_'+faction
        light=by_label.get(label)
        if light: assert isinstance(light,unreal.RectLight)
        else: light=subsystem.spawn_actor_from_class(unreal.RectLight,unreal.Vector(x,46400,800))
        light.set_actor_label(label); light.set_folder_path('Aurelion/00_Lighting')
        light.set_actor_location(unreal.Vector(x,46400,800),False,False)
        light.set_actor_rotation(unreal.Rotator(pitch=-25,yaw=90),False)
        c=light.get_component_by_class(unreal.RectLightComponent); c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS); c.set_intensity(4500)
        c.set_attenuation_radius(2400); c.set_source_width(1600); c.set_source_height(1200)
        c.set_light_color(unreal.LinearColor(1,.89,.73,1) if faction=='Dominion' else unreal.LinearColor(.78,.9,1,1))
assert physical_snapshot()==before,'Existing geometry or collision changed'
(out/'placement.json').write_text(json.dumps(dict(status='PREVIEW_UNSAVED',unchanged_existing_actors=len(before),hidden_visual_instances=112,new_mesh_actors=2,new_lights=2,scope='No runtime or cinematic acceptance'),indent=2),encoding='utf8')
unreal.log('AURELION_SHUTTLES_PREVIEW_UNSAVED')
