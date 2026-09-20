"""Read-only central chamber art survey, including bounds and interaction owners."""
from pathlib import Path
import json,os,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13'
rows=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if not c.static_mesh:continue
        origin,extent,radius=unreal.SystemLibrary.get_component_bounds(c)
        if abs(origin.x)>3500+extent.x or abs(origin.y-35000)>3000+extent.y or origin.z+extent.z<-1850 or origin.z-extent.z>0:continue
        row=dict(actor=actor.get_actor_label(),actor_class=actor.get_class().get_path_name(),component=c.get_path_name(),mesh=c.static_mesh.get_path_name(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),transform=c.get_world_transform().export_text(),origin=[origin.x,origin.y,origin.z],extent=[extent.x,extent.y,extent.z])
        if isinstance(c,unreal.InstancedStaticMeshComponent):row['instances']=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
        rows.append(row)
(out/'chamber-fixtures.json').write_text(json.dumps(rows,indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
