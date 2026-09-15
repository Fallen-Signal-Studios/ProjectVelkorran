"""Read-only relay devices, prop batches and native presentation bindings."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());rows=[]
for a in actors:
    name=a.get_actor_label()
    if name not in ('Aurelion_E2_ReceiverWest','Aurelion_E2_ReceiverEast','Aurelion_Art_M12_Z04_44_dd5f3e','Aurelion_Art_M12_Z04_45_8c8aca') and 'SweepScanner' not in a.get_class().get_name():continue
    row=dict(actor=name,path=a.get_path_name(),class_name=a.get_class().get_name(),transform=a.get_actor_transform().export_text(),properties={},components=[])
    for key in ('receiver_id','encounter_objective','scanner_id','mission_id','config','encounter_director'):
        try:row['properties'][key]=str(a.get_editor_property(key))
        except Exception:pass
    for c in a.get_components_by_class(unreal.ActorComponent):
        r=dict(component=c.get_path_name(),name=c.get_name(),class_name=c.get_class().get_name(),properties={})
        if isinstance(c,unreal.SceneComponent):
            r.update(transform=c.get_world_transform().export_text(),relative_transform=c.get_relative_transform().export_text(),parent=c.get_attach_parent().get_path_name() if c.get_attach_parent() else None)
        for key in ('visible','hidden_in_game','box_extent','interaction_distance','interaction_time','text','world_size','custom_primitive_data'):
            try:r['properties'][key]=str(c.get_editor_property(key))
            except Exception:pass
        if isinstance(c,unreal.PrimitiveComponent):r.update(collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()))
        if isinstance(c,unreal.StaticMeshComponent):
            r.update(mesh=c.static_mesh.get_path_name() if c.static_mesh else None,materials=[m.get_path_name() if m else None for m in c.get_materials()])
        if isinstance(c,unreal.InstancedStaticMeshComponent):r['instances']=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
        row['components'].append(r)
    rows.append(row)
assert len(actors)==3140 and len(rows)>=4
(out/'devices.json').write_text(json.dumps(dict(actor_count=len(actors),devices=rows,qualification='Stopped-editor component inventory, not live interaction or sensor acceptance'),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
