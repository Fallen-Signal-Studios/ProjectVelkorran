"""Read the two chamber floor art instances and export their primitive for fitting."""
from pathlib import Path
import json
import os
import unreal

root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
owner=next(a for a in actors if a.get_actor_label()=='Aurelion_Art_M13_Z10_10_d7ffd0')
c=owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c and c.get_instance_count()==2 and c.static_mesh.get_path_name()=='/Engine/BasicShapes/Cylinder.Cylinder'
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
rows=[]
for index in range(2):
    t=c.get_instance_transform(index,world_space=True)
    rows.append(dict(index=index,transform=t.export_text(),location=[t.translation.x,t.translation.y,t.translation.z],scale=[t.scale3d.x,t.scale3d.y,t.scale3d.z]))
report=dict(actor=owner.get_actor_label(),path=owner.get_path_name(),component=c.get_path_name(),
    actor_transform=owner.get_actor_transform().export_text(),component_transform=c.get_world_transform().export_text(),
    mesh=c.static_mesh.get_path_name(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),
    instances=rows,actor_count=len(actors),materials=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())])
(out/'chamber-floor-baseline.json').write_text(json.dumps(report,indent=2))
task=unreal.AssetExportTask();task.object=c.static_mesh;task.filename=str(out/'Cylinder.fbx')
task.automated=True;task.prompt=False;task.replace_identical=False;task.exporter=unreal.StaticMeshExporterFBX()
assert unreal.Exporter.run_asset_export_task(task)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
