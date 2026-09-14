"""Read-only inventory and source export of the remaining atrium pylons/spans."""
import json,os,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==2484
selected=[a for a in actors if a.get_actor_label().startswith(('Z05__AtriumPylon_','Z05__UpperSpan_'))]
assert len(selected)==12
rows=[];exported=set()
for a in selected:
 c=a.static_mesh_component;m=c.static_mesh;o,e=a.get_actor_bounds(False);b=m.get_bounds()
 rows.append(dict(actor=a.get_actor_label(),actor_transform=a.get_actor_transform().export_text(),component_transform=c.get_world_transform().export_text(),mesh=m.get_path_name(),origin=[o.x,o.y,o.z],extent=[e.x,e.y,e.z],mesh_origin=[b.origin.x,b.origin.y,b.origin.z],mesh_extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z],visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),actor_collision=a.get_actor_enable_collision(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name())))
 if m.get_path_name() not in exported:
  task=unreal.AssetExportTask();task.object=m;task.filename=str(out/(m.get_name()+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=False;task.exporter=unreal.StaticMeshExporterFBX();assert unreal.Exporter.run_asset_export_task(task);exported.add(m.get_path_name())
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'atrium-pylon-baseline.json').write_text(json.dumps(dict(actor_count=len(actors),actors=rows,qualification='Stopped-editor survey; no geometry or gameplay changes'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('pylon-detail',unreal.Vector(-3650,-1500,175),unreal.Rotator(pitch=12,yaw=65),85)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('upper-span',unreal.Vector(-1800,-1800,1100),unreal.Rotator(pitch=-5,yaw=45),85)").replace('z01-','atrium-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'atrium_pylon_capture','exec'))
