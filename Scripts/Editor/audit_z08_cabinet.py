"""Read-only retained Crucible cabinet mesh and instance measurements."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
a=next(a for a in actors if a.get_actor_label()=='Aurelion_Art_M12_Z08_49_b7d737')
c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);mesh=c.static_mesh;b=mesh.get_bounds()
def xyz(v):return [v.x,v.y,v.z]
rows=[]
for i in range(c.get_instance_count()):
    t=c.get_instance_transform(i,world_space=True)
    rows.append(dict(index=i,transform=t.export_text(),location=xyz(t.translation),scale=xyz(t.scale3d),quaternion=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]))
data=dict(actor=a.get_actor_label(),actor_transform=a.get_actor_transform().export_text(),component=c.get_path_name(),component_transform=c.get_world_transform().export_text(),mesh=mesh.get_path_name(),mesh_origin=xyz(b.origin),mesh_extent=xyz(b.box_extent),actor_collision=a.get_actor_enable_collision(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),instances=rows)
(out/'cabinet-baseline.json').write_text(json.dumps(data,indent=2))
assert len(actors)==3140 and len(rows)==1 and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
