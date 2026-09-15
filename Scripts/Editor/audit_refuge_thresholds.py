"""Read-only meshes overlapping the measured refuge threshold bands."""
from pathlib import Path
import json,os,runpy,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);helper=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m=c.static_mesh
        if not m:continue
        b=m.get_bounds();transforms=[(i,c.get_instance_transform(i,world_space=True)) for i in range(c.get_instance_count())] if isinstance(c,unreal.InstancedStaticMeshComponent) else [(None,c.get_world_transform())]
        hits=[]
        for i,t in transforms:
            lo,hi=helper['bounds'](helper['corners'](t,[b.origin.x,b.origin.y,b.origin.z],[b.box_extent.x,b.box_extent.y,b.box_extent.z]))
            if lo[2]>-1185 or hi[2]<-1205 or lo[1]>21930 or hi[1]<21870:continue
            if not any(lo[0]<x+width/2 and hi[0]>x-width/2 for x,width in ((-2600,600),(3050,500))):continue
            hits.append(dict(index=i,transform=t.export_text(),location=[t.translation.x,t.translation.y,t.translation.z],scale=[t.scale3d.x,t.scale3d.y,t.scale3d.z],quaternion=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],bounds=[lo,hi]))
        if hits:
            rows.append(dict(actor=a.get_actor_label(),path=a.get_path_name(),actor_transform=a.get_actor_transform().export_text(),component=c.get_path_name(),component_transform=c.get_world_transform().export_text(),mesh=m.get_path_name(),materials=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())],visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),actor_hidden=a.get_editor_property('hidden'),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),actor_collision=a.get_actor_enable_collision(),instances=hits,all_transforms=[t.export_text() for i,t in transforms]))
(out/'threshold-overlaps.json').write_text(json.dumps(rows,indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
