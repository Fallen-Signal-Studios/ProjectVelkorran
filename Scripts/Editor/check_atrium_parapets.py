"""Ring guard placements, physical boundaries and retained radial passage controls."""
import json,math
from pathlib import Path
import unreal
def get_fit():return json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/AtriumParapetKit/guard-fit.json').read_text())
def passage_controls(world):
 rows=[]
 for angle in (0,90,180,270):
  a=math.radians(angle);t=(math.cos(a),math.sin(a));n=(-t[1],t[0])
  for offset in (-170,0,170):
   start=[3400*t[i]+offset*n[i] for i in (0,1)]+[100];end=[1200*t[i]+offset*n[i] for i in (0,1)]+[100]
   hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(*start),unreal.Vector(*end),42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
   rows.append(dict(angle=angle,offset=offset,blocker=hit.to_tuple()[9].get_actor_label() if hit and hit.to_tuple()[0] else None))
 return rows
def check_parapets(world,actors):
 labels={a.get_actor_label():a for a in actors};fit=get_fit();sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);probes=[]
 for spec in fit['placements']:
  a=labels['KIT_Atrium_Parapet_'+spec['actor']];c=a.static_mesh_component;p=a.get_actor_location();s=a.get_actor_scale3d()
  assert max(abs(x-y) for x,y in zip((p.x,p.y,p.z),spec['position']))<.01 and abs(a.get_actor_rotation().yaw-spec['yaw'])<.001
  assert max(abs(x-1) for x in (s.x,s.y,s.z))<.001
  assert c.static_mesh.get_name()==spec['mesh'] and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
  assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
  assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
  assert sm.get_convex_collision_count(c.static_mesh)==0 and sm.get_simple_collision_count(c.static_mesh)==0
 for row in fit['baseline']:
  a=labels[row['actor']];c=a.static_mesh_component
  assert c.get_world_transform().export_text()==row['transform'] and c.static_mesh.get_path_name()==row['mesh']
  assert c.get_editor_property('visible')==row['visible'] and c.get_editor_property('hidden_in_game')==row['hidden_in_game']
  assert str(c.get_collision_enabled())==row['collision'] and str(c.get_collision_profile_name())==row['profile'] and a.get_actor_enable_collision()
  ignored=[other for other in actors if other!=a];p=a.get_actor_location();angle=math.radians(row['yaw']);normal=(-math.sin(angle),math.cos(angle))
  for z,expected in ((93,True),(230,False)):
   start=(p.x-normal[0]*150,p.y-normal[1]*150,z);end=(p.x+normal[0]*150,p.y+normal[1]*150,z)
   hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(*start),unreal.Vector(*end),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
   assert bool(hit and hit.to_tuple()[0])==expected,(row['actor'],z)
   probes.append(dict(guard=row['actor'],z=z,blocking=expected))
 return dict(placements=len(fit['placements']),guard_controls=probes,passage_controls=passage_controls(world),qualification='Geometry and retained collision controls; live navigation, combat visibility and GPU acceptance remain separate')
