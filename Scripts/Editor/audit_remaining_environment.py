"""Read-only saved-map census of visible meshes outside the custom architecture kit."""
from pathlib import Path
import json,os,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());rows=[];groups={}
for a in actors:
    if a.get_editor_property('hidden'):continue
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m=c.static_mesh
        if not m or not c.get_editor_property('visible') or c.get_editor_property('hidden_in_game'):continue
        path=m.get_path_name()
        if '/Aurelion/Environment/ArchitectureKit/' in path:continue
        transforms=[(i,c.get_instance_transform(i,world_space=True)) for i in range(c.get_instance_count())] if isinstance(c,unreal.InstancedStaticMeshComponent) else [(None,c.get_world_transform())]
        b=m.get_bounds()
        row=dict(actor=a.get_actor_label(),path=a.get_path_name(),class_name=a.get_class().get_name(),component=c.get_path_name(),actor_transform=a.get_actor_transform().export_text(),component_transform=c.get_world_transform().export_text(),mesh=path,mesh_origin=[b.origin.x,b.origin.y,b.origin.z],mesh_extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z],collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),actor_collision=a.get_actor_enable_collision(),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],instances=[dict(index=i,location=[t.translation.x,t.translation.y,t.translation.z],transform=t.export_text(),scale=[t.scale3d.x,t.scale3d.y,t.scale3d.z],quaternion=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]) for i,t in transforms])
        rows.append(row);g=groups.setdefault(path,dict(components=0,instances=0,actors=[]));g['components']+=1;g['instances']+=len(transforms);g['actors'].append(a.get_actor_label())
(out/'remaining-environment.json').write_text(json.dumps(dict(actor_count=len(actors),components=rows,groups=groups,qualification='Saved visible static-mesh candidates; runtime-spawned geometry and hidden/native collision layers are outside this census.'),indent=2))
assert len(actors)==3140 and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
