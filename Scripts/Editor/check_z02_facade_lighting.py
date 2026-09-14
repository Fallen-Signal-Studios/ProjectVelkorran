"""Fitted facade fixture and light settings; no photometric/performance acceptance."""
import json
from pathlib import Path
import unreal
def check_lighting(world,actors):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/UplightKit/z02-facade-lighting.json').read_text());labels={a.get_actor_label():a for a in actors};rows=[]
    for row in fit['fixtures']:
        name='KIT_Z02_FacadeLight_'+row['name'];a=labels[name];p=a.get_actor_location();s=a.get_actor_scale3d();x,y,z=row['position']
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),(x,y,z)))<.01 and all(abs(v-1)<.001 for v in (s.x,s.y,s.z))
        assert abs(abs(a.get_actor_rotation().yaw)-180)<.01
        c=a.static_mesh_component;assert c.static_mesh.get_name()=='SM_Aurelion_KIT_PierUplight'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not a.get_actor_enable_collision()
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        light=labels[name+'_Light'];p=light.get_actor_location();assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),(x,y-27.5,z+49)))<.01
        expected=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(x,y+28.5,row['target_z']));actual=light.get_actor_rotation()
        assert abs(expected.pitch-actual.pitch)<.01 and abs((expected.yaw-actual.yaw+180)%360-180)<.01
        c=light.get_component_by_class(unreal.RectLightComponent)
        assert c.get_editor_property('mobility')==unreal.ComponentMobility.MOVABLE and c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
        assert abs(c.get_editor_property('intensity')-row['lumens'])<.01 and abs(c.get_editor_property('attenuation_radius')-row['radius'])<.01
        assert c.get_editor_property('source_width')==fit['source_width'] and c.get_editor_property('source_height')==fit['source_height']
        color=c.get_light_color()
        assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b),fit['color']))<.01
        rows.append(name)
    return dict(fixtures=rows,light_count=len(rows),qualification='Placement and settings checks only; final lighting, combat visibility and GPU cost unqualified.')
