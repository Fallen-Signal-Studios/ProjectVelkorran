"""Canopy placement and preservation of the original route, floor instances and lights."""
import json,math,runpy
from pathlib import Path
import unreal
def check_canopies(world,actors):
 root=Path(unreal.Paths.project_dir());labels={a.get_actor_label():a for a in actors};baseline=json.loads((root/'Art/Source/Aurelion/AtriumCanopyKit/canopy-baseline.json').read_text());sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
 for angle in (0,90,180,270):
  for kind in ('Frame','Vault'):
   a=labels[f'KIT_Atrium_Canopy_{angle}_{kind}'];p=a.get_actor_location();s=a.get_actor_scale3d();r=math.radians(angle)
   assert abs(p.x-2300*math.cos(r))<.01 and abs(p.y-2300*math.sin(r))<.01 and abs(p.z)<.01
   assert abs((a.get_actor_rotation().yaw-angle-90+180)%360-180)<.001 and max(abs(x-1) for x in (s.x,s.y,s.z))<.001
   c=a.static_mesh_component;mesh=c.static_mesh
   assert mesh.get_name()=='SM_Aurelion_KIT_AtriumCanopy'+kind and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
   assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
   assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled') and sm.get_convex_collision_count(mesh)==0 and sm.get_simple_collision_count(mesh)==0
 for row in baseline['actors']:
  a=labels[row['actor']];c=a.static_mesh_component
  assert a.get_actor_transform().export_text()==row['actor_transform'] and c.get_world_transform().export_text()==row['component_transform'] and c.static_mesh.get_path_name()==row['mesh']
  assert a.get_actor_enable_collision()==row['actor_collision'] and str(c.get_collision_enabled())==row['collision'] and str(c.get_collision_profile_name())==row['profile']
  replaced='BridgeCanopy' in row['actor'] or 'CeramicCanopyPylon' in row['actor']
  assert c.get_editor_property('visible')==(False if replaced else row['visible']) and c.get_editor_property('hidden_in_game')==(True if replaced else row['hidden'])
 light_measurements=[]
 for row in baseline['lights']:
  c=next(c for c in labels[row['actor']].get_components_by_class(unreal.LightComponent) if c.get_name()==row['component'])
  assert c.get_world_transform().export_text()==row['transform'] and c.get_editor_property('intensity')==row['intensity'] and c.get_editor_property('light_color').export_text()==row['color'] and c.get_editor_property('visible')==row['visible']
  light_measurements.append(dict(actor=row['actor'],units=str(c.get_editor_property('intensity_units')),intensity=c.get_editor_property('intensity'),source_width=c.get_editor_property('source_width'),source_height=c.get_editor_property('source_height'),attenuation_radius=c.get_editor_property('attenuation_radius')))
 floor=baseline['floor'];c=next(c for c in labels[floor['actor']].get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==floor['component'])
 assert c.static_mesh.get_path_name()==floor['mesh'] and str(c.get_collision_enabled())==floor['collision']
 assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==[r['transform'] for r in floor['instances']]
 passages=runpy.run_path(str(root/'Scripts/Editor/check_atrium_parapets.py'))['passage_controls'](world)
 assert passages==json.loads((root/'Art/Source/Aurelion/AtriumParapetKit/passage-baseline.json').read_text())
 return dict(placements=8,replaced_visuals=20,retained_lights=len(baseline['lights']),light_measurements=light_measurements,retained_floor_instances=c.get_instance_count(),passage_controls=passages,qualification='Editor geometry and retained state only; live traversal, final lighting and performance remain unqualified')
