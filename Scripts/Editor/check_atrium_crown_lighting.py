"""Verify fitted crown fixtures and their local upward lighting settings."""
import json,math
from pathlib import Path
import unreal
def check_crown_lighting(world,actors):
 fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/UplightKit/atrium-crown-lighting.json').read_text());labels={a.get_actor_label():a for a in actors};rows=[]
 for i in range(fit['count']):
  angle=fit['start_angle']+i*fit['angle_step'];r=math.radians(angle);u=(math.cos(r),math.sin(r));name=f'KIT_Atrium_CrownLight_{i:02}'
  a=labels[name];p=a.get_actor_location();s=a.get_actor_scale3d();c=a.static_mesh_component
  assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),(fit['fixture_radius']*u[0],fit['fixture_radius']*u[1],fit['fixture_z'])))<.01
  assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and abs((a.get_actor_rotation().yaw-angle-90+180)%360-180)<.01
  assert c.static_mesh.get_name()=='SM_Aurelion_KIT_PierUplight' and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
  assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
  light=labels[name+'_Light'];p=light.get_actor_location();expected_p=unreal.Vector(fit['light_radius']*u[0],fit['light_radius']*u[1],fit['light_z'])
  assert (p-expected_p).length()<.01
  expected=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(fit['target_radius']*u[0],fit['target_radius']*u[1],fit['target_z']));actual=light.get_actor_rotation()
  assert abs(actual.pitch-expected.pitch)<.01 and abs((actual.yaw-expected.yaw+180)%360-180)<.01
  assert actual.pitch>fit['outer_cone'], 'The direct-light cone must stay above the horizon'
  c=light.get_component_by_class(unreal.SpotLightComponent)
  assert c.get_editor_property('mobility')==unreal.ComponentMobility.MOVABLE and c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
  for prop,key in (('intensity','lumens'),('attenuation_radius','attenuation_radius'),('inner_cone_angle','inner_cone'),('outer_cone_angle','outer_cone'),('source_radius','source_radius')):assert abs(c.get_editor_property(prop)-fit[key])<.01
  color=c.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b),fit['color']))<.01
  assert c.get_editor_property('cast_shadows') and c.get_editor_property('use_inverse_squared_falloff')
  rows.append(dict(fixture=name,pitch=actual.pitch,lumens=fit['lumens']))
 return dict(fixtures=rows,light_count=8,qualification='Placement/settings only; final lighting, live combat visibility and GPU cost remain unqualified')
