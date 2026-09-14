"""Read-only atrium transition and intersecting instance survey."""
import json,os,time,itertools,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors}
regions=[((-4000,-4000,-1000),(4000,4000,8000))]
def overlaps(lo,hi):return any(all(b>=c and a<=d for a,b,c,d in zip(lo,hi,rlo,rhi)) for rlo,rhi in regions)
rows=[]
for a in actors:
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  mesh=c.static_mesh
  if not mesh:continue
  origin,extent,radius=unreal.SystemLibrary.get_component_bounds(c)
  if not overlaps([v-e for v,e in zip((origin.x,origin.y,origin.z),(extent.x,extent.y,extent.z))],[v+e for v,e in zip((origin.x,origin.y,origin.z),(extent.x,extent.y,extent.z))]):continue
  instances=[]
  if isinstance(c,unreal.InstancedStaticMeshComponent):
   b=mesh.get_bounds();o=b.origin;e=b.box_extent
   corners=[unreal.Vector(o.x+sx*e.x,o.y+sy*e.y,o.z+sz*e.z) for sx,sy,sz in itertools.product((-1,1),repeat=3)]
   for i in range(c.get_instance_count()):
    t=c.get_instance_transform(i,world_space=True);points=[unreal.MathLibrary.transform_location(t,p) for p in corners]
    lo=[min(getattr(p,k) for p in points) for k in ('x','y','z')];hi=[max(getattr(p,k) for p in points) for k in ('x','y','z')]
    if overlaps(lo,hi):instances.append(dict(index=i,transform=t.export_text(),bounds=[lo,hi]))
   if not instances:continue
  rows.append(dict(actor=a.get_actor_label(),component=c.get_name(),mesh=mesh.get_path_name(),transform=c.get_world_transform().export_text(),visible=c.get_editor_property('visible'),hidden_in_game=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),origin=origin.export_text(),extent=extent.export_text(),instances=instances))
def trace(start,end,complex_trace):
 raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*start),unreal.Vector(*end),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex_trace,[],unreal.DrawDebugTrace.NONE,True)
 hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
 if not hit or not hit.to_tuple()[0]:return None
 t=hit.to_tuple();return dict(actor=t[9].get_actor_label(),position=t[5].export_text(),normal=t[6].export_text())
probes=[]
for x in (-7100,-6500,-5900,-5300,-4700,-4100,-3500,-2900,-2300,-1700):
 for y in (-2200,-2000,-1800):
  start=(x,y,250);end=(x,y,-100)
  probes.append(dict(kind='floor',start=start,end=end,simple=trace(start,end,False),complex=trace(start,end,True)))
for angle in range(0,360,5):
 for radius in (880,920,1200,1750,1820,2300,2780,2820,3200,3580,3620,3680):
  radians=math.radians(angle);x=radius*math.cos(radians);y=radius*math.sin(radians)
  start=(x,y,250);end=(x,y,-150)
  probes.append(dict(kind='ring_floor',angle=angle,radius=radius,start=start,end=end,simple=trace(start,end,False),complex=trace(start,end,True)))
bridges=[]
for name in ('bridge2','bridge3','bridge4','bridge5'):
 a=by_label[name];bridges.append(dict(actor=name,transform=a.get_actor_transform().export_text(),mesh=a.static_mesh_component.static_mesh.get_path_name()))
(out/'atrium-ring-survey.json').write_text(json.dumps(dict(scope='Authoring survey including instance bounds, not just instance origins; global enclosures may intersect the region. No live route qualification.',bridges=bridges,components=rows,probes=probes),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('atrium-entry',unreal.Vector(-3200,0,175),unreal.Rotator(yaw=0),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('atrium-overview',unreal.Vector(-5900,-5900,4500),unreal.Rotator(pitch=-32,yaw=45),90)").replace('z01-','atrium-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'north_connector_capture','exec'))
