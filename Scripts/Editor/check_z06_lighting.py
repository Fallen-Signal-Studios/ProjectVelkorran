"""Verify local key-light calibration and visible upward-facing pier fixtures."""
import json,math
from pathlib import Path
import unreal

def check_z06_lighting(world,actors):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z06Lighting/lighting-fit.json').read_text());labels={a.get_actor_label():a for a in actors};rows=[]
    gate_fit=None
    if 'KIT_Z06_GateHousing' in labels:
        gate_fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/GateLanternKit/lighting-fit.json').read_text())
        fit['target_x']=gate_fit['uplight_target_x'];fit['lumens']=gate_fit['uplight_lumens']
    for row in fit['baseline']:
        c=labels[row['actor']].get_component_by_class(unreal.RectLightComponent)
        assert c.get_world_transform().export_text()==row['transform'] and c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
        lumens=gate_fit['key_01_lumens'] if gate_fit and row['actor']=='ENVL_Z06_Key_01' else fit['key_lumens']
        assert c.get_editor_property('intensity')==lumens and c.get_editor_property('attenuation_radius')==fit['key_radius']
    for side,sign,yaw in [('West',-1,-90),('East',1,90)]:
        for i,y in enumerate(fit['stations']):
            name=f'KIT_Z06_Uplight_{side}_{i:02}';a=labels[name];p=a.get_actor_location();s=a.get_actor_scale3d();c=a.static_mesh_component
            assert (p-unreal.Vector(sign*fit['fixture_x'],y,fit['fixture_z'])).length()<.01
            assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and abs(a.get_actor_rotation().yaw-yaw)<.01
            assert c.static_mesh.get_name()=='SM_Aurelion_KIT_PierUplight' and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
            assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            light=labels[name+'_Light'];p=light.get_actor_location();assert (p-unreal.Vector(sign*fit['light_x'],y,fit['light_z'])).length()<.01
            expected=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(sign*fit['target_x'],y,fit['target_z']));r=light.get_actor_rotation()
            direction=unreal.Vector(sign*fit['target_x'],y,fit['target_z'])-p;direction=direction/direction.length()
            # Yaw is undefined for a vertical light; compare physical aim at the same angular tolerance.
            actual=light.get_actor_forward_vector()
            assert (actual-direction).length()<math.radians(.01) and actual.z>math.sin(math.radians(fit['outer_cone']))
            c=light.get_component_by_class(unreal.SpotLightComponent)
            assert c.get_editor_property('mobility')==unreal.ComponentMobility.MOVABLE and c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
            for prop,key in [('intensity','lumens'),('attenuation_radius','attenuation_radius'),('inner_cone_angle','inner_cone'),('outer_cone_angle','outer_cone'),('source_radius','source_radius')]:assert abs(c.get_editor_property(prop)-fit[key])<.01
            color=c.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b),fit['color']))<.01
            assert c.get_editor_property('cast_shadows') and c.get_editor_property('use_inverse_squared_falloff')
            rows.append(dict(fixture=name,pitch=r.pitch,lumens=fit['lumens']))
    return dict(fixtures=rows,calibrated_keys=3,qualification='Editor placement/settings; live combat visibility, final lighting and GPU cost remain unqualified.')
