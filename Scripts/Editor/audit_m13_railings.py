"""Read-only M13 chamber guardrail mesh and instance measurements."""
from pathlib import Path
import json,os,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
a=next(a for a in actors if a.get_actor_label()=='Aurelion_Art_M13_Z10_12_de411e')
c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);mesh=c.static_mesh;b=mesh.get_bounds()
def xyz(v):return [v.x,v.y,v.z]
rows=[]
for i in range(c.get_instance_count()):
    t=c.get_instance_transform(i,world_space=True)
    rows.append(dict(index=i,location=xyz(t.translation),transform=t.export_text(),scale=xyz(t.scale3d),quaternion=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]))
data=dict(actor=a.get_actor_label(),path=a.get_path_name(),class_name=a.get_class().get_name(),component=c.get_path_name(),actor_transform=a.get_actor_transform().export_text(),component_transform=c.get_world_transform().export_text(),mesh=mesh.get_path_name(),mesh_origin=xyz(b.origin),mesh_extent=xyz(b.box_extent),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),actor_collision=a.get_actor_enable_collision(),materials=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())],instances=rows)
(out/'rail-baseline.json').write_text(json.dumps(data,indent=2))
assert len(actors)==1315 and len(rows)==192 and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
