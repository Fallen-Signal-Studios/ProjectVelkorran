"""Fitted floor placement, old-instance preservation and physical joint samples."""
import json,math,runpy
from pathlib import Path
import unreal

def check_bridge_floors(world,actors):
 root=Path(unreal.Paths.project_dir());labels={a.get_actor_label():a for a in actors};source=root/'Art/Source/Aurelion/AtriumBridgeFloorKit'
 fit=json.loads((source/'floor-fit.json').read_text());baseline=json.loads((root/'Art/Source/Aurelion/AtriumCanopyKit/canopy-baseline.json').read_text());sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
 for angle in (0,90,180,270):
  a=labels[f'KIT_Atrium_BridgeFloor_{angle}'];p=a.get_actor_location();r=math.radians(angle);s=a.get_actor_scale3d();c=a.static_mesh_component;m=c.static_mesh
  assert abs(p.x-2300*math.cos(r))<.001 and abs(p.y-2300*math.sin(r))<.001 and abs(p.z)<.001
  assert abs((a.get_actor_rotation().yaw-angle+180)%360-180)<.001 and max(abs(v-1) for v in (s.x,s.y,s.z))<.001
  assert m.get_name()=='SM_Aurelion_KIT_AtriumBridgeFloor' and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
  assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
  assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
 floor=baseline['floor'];c=next(c for c in labels[floor['actor']].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==floor['component'])
 assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(fit['retained_transforms'])
 # Only the retained physical bridge slabs and ring sectors participate in these floor probes.
 physical_names={f'Z05_Bridge_{angle}' for angle in (0,90,180,270)}|{f'{prefix}{i:02}' for prefix in ('Z05_Outer_Walk_','Z05_Control_Platform_') for i in range(32)}
 physical=[labels[name] for name in sorted(physical_names)]
 assert len(physical)==68
 ignored=[a for a in actors if a not in physical];samples=[]
 for angle in (0,90,180,270):
  r=math.radians(angle)
  for lateral in (-170,0,170):
   skew=abs(lateral)*math.tan(math.radians(5.625))
   for radius in (1800-skew-3,1800-skew+3,2050,2300,2550,2800-skew-3,2800-skew+3):
    x=radius*math.cos(r)-lateral*math.sin(r);y=radius*math.sin(r)+lateral*math.cos(r)
    raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,150),unreal.Vector(x,y,-100),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
    hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
    assert hit and hit.to_tuple()[0],(angle,lateral,radius)
    t=hit.to_tuple();assert t[9] in physical and abs(t[5].z)<.01
    samples.append(dict(angle=angle,lateral=lateral,radius=radius,actor=t[9].get_actor_label(),height=t[5].z))
 canopies=runpy.run_path(str(root/'Scripts/Editor/check_atrium_canopies.py'))['check_canopies'](world,actors)
 return dict(placements=4,removed_floor_instances=24,retained_floor_instances=132,physical_floor_samples=samples,canopies=canopies,qualification='Editor geometry and isolated physical floor probes; live traversal and performance remain unqualified')
