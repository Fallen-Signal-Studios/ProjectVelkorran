"""Read-only bridge canopy, support, lighting and floor-instance fitting survey."""
import json,os,itertools
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors}
names=[f'Aurelion_Radiance_Z05_BridgeCanopy_{a}' for a in (0,90,180,270)]+[f'Aurelion_CeramicCanopyPylon_{i:02}' for i in range(4,20)]+[f'Z05_Bridge_{a}' for a in (0,90,180,270)]
rows=[];exported=set()
for name in names:
 a=labels[name];c=a.static_mesh_component;mesh=c.static_mesh;o,e=a.get_actor_bounds(False)
 if 'BridgeCanopy' in name and mesh.get_path_name() not in exported:
  task=unreal.AssetExportTask();task.object=mesh;task.filename=str(out/(mesh.get_name()+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=False;task.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(task);exported.add(mesh.get_path_name())
 rows.append(dict(actor=name,actor_transform=a.get_actor_transform().export_text(),component_transform=c.get_world_transform().export_text(),mesh=mesh.get_path_name(),origin=[o.x,o.y,o.z],extent=[e.x,e.y,e.z],visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),actor_collision=a.get_actor_enable_collision(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name())))
lights=[]
for a in actors:
 for c in a.get_components_by_class(unreal.LightComponent):
  t=c.get_world_transform();p=t.translation
  if abs(p.x)>4000 or abs(p.y)>4000:continue
  lights.append(dict(actor=a.get_actor_label(),component=c.get_name(),type=c.get_class().get_name(),transform=t.export_text(),intensity=c.get_editor_property('intensity'),color=c.get_editor_property('light_color').export_text(),visible=c.get_editor_property('visible')))
floor=labels['Aurelion_Art_M12_Z05_53_427254'].get_component_by_class(unreal.InstancedStaticMeshComponent);b=floor.static_mesh.get_bounds();o=b.origin;e=b.box_extent
corners=[unreal.Vector(o.x+sx*e.x,o.y+sy*e.y,o.z+sz*e.z) for sx,sy,sz in itertools.product((-1,1),repeat=3)];instances=[]
for i in range(floor.get_instance_count()):
 t=floor.get_instance_transform(i,world_space=True);points=[unreal.MathLibrary.transform_location(t,p) for p in corners]
 instances.append(dict(index=i,transform=t.export_text(),bounds=[[min(getattr(p,k) for p in points) for k in ('x','y','z')],[max(getattr(p,k) for p in points) for k in ('x','y','z')]]))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'canopy-baseline.json').write_text(json.dumps(dict(actors=rows,lights=lights,floor=dict(actor='Aurelion_Art_M12_Z05_53_427254',component=floor.get_name(),mesh=floor.static_mesh.get_path_name(),collision=str(floor.get_collision_enabled()),instances=instances)),indent=2))
