"""Fitted ring placements, retained collision and exact HISM preservation."""
import json,math
from pathlib import Path
import unreal
def check_rings(world,actors):
 labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir())
 baseline=json.loads((root/'Art/Source/Aurelion/AtriumRingKit/placement-baseline.json').read_text())
 sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);retained=[];placements=[]
 for group in baseline['meshes']:
  kind='Outer' if '28_36' in group['mesh'] else 'Inner'
  for row in group['users']:
   old=labels[row['actor']];c=next(c for c in old.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()==row['component'])
   assert c.get_world_transform().export_text()==row['transform'] and c.static_mesh.get_path_name().split('.')[0]==group['mesh']
   assert str(c.get_collision_enabled())==row['collision']
   assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
   if row['instances']:
    assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['instances']
    continue
   retained.append(old);new=labels['KIT_Atrium_Ring_'+row['actor']];nc=new.static_mesh_component;mesh=nc.static_mesh
   assert new.get_actor_transform().export_text()==old.get_actor_transform().export_text()
   assert mesh.get_name()=='SM_Aurelion_KIT_Atrium'+kind+'RingSector'
   assert not new.get_actor_enable_collision() and nc.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
   assert nc.get_editor_property('visible') and not nc.get_editor_property('hidden_in_game')
   assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
   assert sm.get_convex_collision_count(mesh)==0 and sm.get_simple_collision_count(mesh)==0
   bounds=mesh.get_bounds();assert abs(bounds.origin.z+bounds.box_extent.z)<.02 and abs(bounds.origin.z-bounds.box_extent.z+240)<.02
   placements.append(new.get_actor_label())
 assert len(placements)==64
 ignored=[a for a in actors if a not in retained];probes=[]
 for ri,ro in ((900,1800),(2800,3600)):
  for sector in range(32):
   angle=math.radians((sector+.5)*11.25)
   for radius in (ri+60,(ri+ro)/2,ro-60):
    x=radius*math.cos(angle);y=radius*math.sin(angle)
    raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,200),unreal.Vector(x,y,-100),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
    hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
    assert hit and hit.to_tuple()[0],(sector,radius)
    z=hit.to_tuple()[5].z;assert abs(z)<.02
    probes.append(dict(sector=sector,radius=radius,z=z))
 return dict(placements=len(placements),retained_floor_probes=probes,retained_instances=64,qualification='Original collision and sample heights preserved; live traversal and packaged performance remain unqualified')
