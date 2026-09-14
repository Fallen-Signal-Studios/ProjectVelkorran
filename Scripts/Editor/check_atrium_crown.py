"""Crown placement, superseded proxy retirement and physical ring clearances."""
import json,math,runpy
from pathlib import Path
import unreal

def ring_lanes(world):
 rows=[]
 for i in range(8):
  a=math.radians(22.5+i*45);u=(math.cos(a),math.sin(a));v=(-u[1],u[0])
  for radius in (3050,3440):
   start=unreal.Vector(radius*u[0]-250*v[0],radius*u[1]-250*v[1],100);end=unreal.Vector(radius*u[0]+250*v[0],radius*u[1]+250*v[1],100)
   hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
   rows.append(dict(pier=i,radius=radius,blocker=hit.to_tuple()[9].get_actor_label() if hit and hit.to_tuple()[0] else None))
 return rows

def check_crown(world,actors):
 root=Path(unreal.Paths.project_dir());labels={a.get_actor_label():a for a in actors};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
 baseline=json.loads((root/'Art/Source/Aurelion/AtriumCrownKit/placement-baseline.json').read_text())
 for row in baseline['actors']:
  a=labels[row['actor']];c=a.static_mesh_component
  assert a.get_actor_transform().export_text()==row['actor_transform'] and c.get_world_transform().export_text()==row['component_transform'] and c.static_mesh.get_path_name()==row['mesh']
  assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
  assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
 controls=[]
 for i in range(9):
  name=f'KIT_Atrium_Crown_Pier_{i:02}' if i<8 else 'KIT_Atrium_Crown_Ribs';a=labels[name];c=a.static_mesh_component;m=c.static_mesh;p=a.get_actor_location();s=a.get_actor_scale3d()
  angle=22.5+i*45 if i<8 else 0;r=math.radians(angle);expected=(3230*math.cos(r),3230*math.sin(r),0) if i<8 else (0,0,0)
  assert max(abs(x-y) for x,y in zip((p.x,p.y,p.z),expected))<.001 and abs((a.get_actor_rotation().yaw-angle+180)%360-180)<.001
  assert max(abs(x-1) for x in (s.x,s.y,s.z))<.001 and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
  assert m.get_name()==('SM_Aurelion_KIT_AtriumCrownPier' if i<8 else 'SM_Aurelion_KIT_AtriumOpenCrown')
  assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==(3 if i<8 else 0)
  assert a.get_actor_enable_collision()==(i<8) and c.get_collision_enabled()==(unreal.CollisionEnabled.QUERY_AND_PHYSICS if i<8 else unreal.CollisionEnabled.NO_COLLISION)
  if i<8:
   ignored=[other for other in actors if other!=a]
   for z,blocked in ((100,True),(300,True),(930,False)):
    start=unreal.Vector(p.x-150*math.cos(r),p.y-150*math.sin(r),z);end=unreal.Vector(p.x+150*math.cos(r),p.y+150*math.sin(r),z)
    hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
    assert bool(hit and hit.to_tuple()[0])==blocked,(name,z)
    controls.append(dict(pier=name,height=z,blocking=blocked))
 lanes=ring_lanes(world);assert all(r['blocker'] is None for r in lanes),lanes
 passages=runpy.run_path(str(root/'Scripts/Editor/check_atrium_parapets.py'))['passage_controls'](world)
 assert passages==json.loads((root/'Art/Source/Aurelion/AtriumParapetKit/passage-baseline.json').read_text())
 return dict(placements=9,retired_proxies=12,pier_collision_controls=controls,ring_lanes=lanes,radial_passages=passages,qualification='Editor collision and geometry only; live navigation, combat visibility and GPU cost remain unqualified')
