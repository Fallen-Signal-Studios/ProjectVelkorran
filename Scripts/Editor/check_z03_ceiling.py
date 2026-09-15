"""Full sensor-room ceiling coverage with native room and scanner staging preserved."""
from pathlib import Path
import json,unreal
def check_z03_ceiling(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z03CeilingKit';old=json.loads((source/'ceiling-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text());labels={a.get_actor_label():a for a in actors}
    a=labels[old['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);m=c.static_mesh
    assert a.get_path_name()==old['path'] and c.get_path_name()==old['component'] and a.get_actor_transform().export_text()==old['actor_transform'] and c.get_world_transform().export_text()==old['component_transform']
    assert m.get_name()=='SM_Aurelion_KIT_Z03Coffer' and c.get_instance_count()==72
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision' and a.get_actor_enable_collision()==old['actor_collision']
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not a.get_editor_property('hidden') and not c.get_editor_property('override_materials')
    expected=sorted(tuple(round(v,3) for v in r['location_cm']) for r in manifest['placements']);actual=[]
    for i in range(72):
        t=c.get_instance_transform(i,world_space=True);assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001
        r=t.rotation.rotator();assert abs(r.yaw)+abs(r.pitch)+abs(r.roll)<.001
        actual.append(tuple(round(v,3) for v in (t.translation.x,t.translation.y,t.translation.z)))
    assert sorted(actual)==expected and len(set(actual))==72
    xs=sorted(set(p[0] for p in actual));ys=sorted(set(p[1] for p in actual));assert len(xs)==6 and len(ys)==12
    assert abs(xs[0]-1100/6-5900)<.01 and abs(xs[-1]+1100/6-8100)<.01 and ys[0]-200==-19800 and ys[-1]+200==-15000
    b=m.get_bounds();assert abs(b.box_extent.x*2-1100/3)<.01 and abs(b.box_extent.y*2-400)<.01 and abs(b.origin.z-b.box_extent.z)<.01 and abs(b.origin.z+b.box_extent.z-55)<.01
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(m)
    assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
    assert sm.get_num_uv_channels(m,0)==2 and sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
    room=json.loads((source/'room-baseline.json').read_text());preserved=[]
    for row in room['named_actors']:
        actor=labels[row['actor']];assert actor.get_actor_transform().export_text()==row['transform'];preserved.append(row['actor'])
    for row in room['components']:
        if not row['actor'].startswith('Z03_'):continue
        actor=labels[row['actor']];component=actor.get_component_by_class(unreal.StaticMeshComponent)
        retired_names={'Z03__ArtPylon_W','Z03__ArtPylon_E','Z03__UpperSpan_01','Z03__UpperSpan_02','Z03__GoldChannel_01','Z03__GoldChannel_02'}
        retired=row['actor'] in retired_names and component.get_editor_property('hidden_in_game') and component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert component.static_mesh.get_path_name()==row['mesh'] and (retired or str(component.get_collision_enabled())==row['collision']) and actor.get_actor_enable_collision()==row['actor_collision']
    assert len(actors)==3140
    return dict(coffers=72,covered_area_m2=1056,minimum_visual_height_cm=600,preserved_room_marks=preserved,qualification='Saved full ceiling coverage and unchanged native room/scanner staging; live scanner traversal and performance remain unqualified.')
