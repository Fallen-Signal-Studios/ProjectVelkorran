"""Read-only ring mesh reference and isolated seam probes for custom kit fitting."""
import json, os, math
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
meshes=['/Game/Aurelion/Meshes/SM_RingSector_28_36_0p6','/Game/Aurelion/Meshes/SM_RingSector_9_18_0p6']
rows=[]
for path in meshes:
 mesh=unreal.load_asset(path);assert mesh
 task=unreal.AssetExportTask();task.object=mesh;task.filename=str(out/(mesh.get_name()+'.fbx'))
 task.automated=True;task.prompt=False;task.replace_identical=False;task.exporter=unreal.StaticMeshExporterFBX()
 assert unreal.Exporter.run_asset_export_task(task)
 users=[]
 for a in actors:
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if c.static_mesh!=mesh:continue
   users.append(dict(actor=a.get_actor_label(),component=c.get_name(),transform=c.get_world_transform().export_text(),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_enabled()),instances=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())] if isinstance(c,unreal.InstancedStaticMeshComponent) else []))
 rows.append(dict(mesh=path,users=users))
probes=[]
for angle in (224.9,224.99,225,225.01,225.1,314.9,314.99,315,315.01,315.1):
 for radius in (1200,1750,3200,3580):
  x=radius*math.cos(math.radians(angle));y=radius*math.sin(math.radians(angle))
  raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,250),unreal.Vector(x,y,-150),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
  hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
  t=hit.to_tuple() if hit else None
  probes.append(dict(angle=angle,radius=radius,hit=dict(actor=t[9].get_actor_label(),point=t[5].export_text()) if t and t[0] else None))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'atrium-ring-geometry.json').write_text(json.dumps(dict(meshes=rows,seam_probes=probes,scope='Read-only source geometry; ray seam diagnostics are not player fall-through acceptance'),indent=2))
