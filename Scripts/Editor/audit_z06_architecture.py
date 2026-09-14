"""Read-only breach-rescue room survey including full instance bounds."""
import json,os,time,itertools
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==2509
named=[a for a in actors if a.get_actor_label().startswith('Z06_')]
floors=[a for a in named if 'floor' in a.get_actor_label().lower() and a.get_component_by_class(unreal.StaticMeshComponent)]
assert floors
floor=max(floors,key=lambda a:a.get_actor_bounds(False)[1].x*a.get_actor_bounds(False)[1].y);origin,extent=floor.get_actor_bounds(False)
lo=(origin.x-extent.x-100,origin.y-extent.y-100,origin.z-extent.z-300);hi=(origin.x+extent.x+100,origin.y+extent.y+100,origin.z+extent.z+2000)
def overlaps(low,high):return all(a<=d and b>=c for a,b,c,d in zip(low,high,lo,hi))
def xyz(v):return [v.x,v.y,v.z]
rows=[]
for a in actors:
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  mesh=c.static_mesh
  if not mesh:continue
  o,e,_=unreal.SystemLibrary.get_component_bounds(c)
  if not overlaps([v-d for v,d in zip(xyz(o),xyz(e))],[v+d for v,d in zip(xyz(o),xyz(e))]):continue
  instances=[]
  if isinstance(c,unreal.InstancedStaticMeshComponent):
   b=mesh.get_bounds();mo=b.origin;me=b.box_extent;corners=[unreal.Vector(mo.x+x*me.x,mo.y+y*me.y,mo.z+z*me.z) for x,y,z in itertools.product((-1,1),repeat=3)]
   for i in range(c.get_instance_count()):
    t=c.get_instance_transform(i,world_space=True);points=[unreal.MathLibrary.transform_location(t,p) for p in corners]
    low=[min(getattr(p,k) for p in points) for k in ('x','y','z')];high=[max(getattr(p,k) for p in points) for k in ('x','y','z')]
    if overlaps(low,high):instances.append(dict(index=i,transform=t.export_text(),bounds=[low,high]))
   if not instances:continue
  rows.append(dict(actor=a.get_actor_label(),actor_class=a.get_class().get_name(),component=c.get_name(),mesh=mesh.get_path_name(),actor_transform=a.get_actor_transform().export_text(),component_transform=c.get_world_transform().export_text(),origin=xyz(o),extent=xyz(e),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),actor_collision=a.get_actor_enable_collision(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),instances=instances,instance_count=c.get_instance_count() if isinstance(c,unreal.InstancedStaticMeshComponent) else None,all_instance_transforms=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())] if isinstance(c,unreal.InstancedStaticMeshComponent) else []))
(out/'z06-architecture.json').write_text(json.dumps(dict(scope='Stopped-editor geometry inventory; no gameplay or visual acceptance',bounds=[lo,hi],floor_actor=floor.get_actor_label(),named_actors=[dict(actor=a.get_actor_label(),class_name=a.get_class().get_name(),transform=a.get_actor_transform().export_text()) for a in named],components=rows),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('room-return',unreal.Vector(origin.x,origin.y+extent.y-250,origin.z+extent.z+250),unreal.Rotator(pitch=6,yaw=-90),90)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'z06_survey_capture','exec'))
