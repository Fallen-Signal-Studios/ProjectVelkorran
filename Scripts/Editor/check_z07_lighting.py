"""Gallery uplight geometry, optical aim and light settings; no live acceptance."""
import json
from pathlib import Path
import unreal
def check_z07_lighting(world,actors):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z07Lighting/lighting-fit.json').read_text())
    labels={a.get_actor_label():a for a in actors};rows=[]
    assert sum(n.startswith('KIT_Z07_Uplight_') for n in labels)==12
    for i in range(2):
        c=labels[f'ENVL_Z07_Key_{i:02}'].get_component_by_class(unreal.RectLightComponent)
        assert c.get_editor_property('cast_shadows')
        assert c.get_editor_property('source_width')==fit['key_source_width'] and c.get_editor_property('source_height')==fit['key_source_height']
    for side,sign,yaw in [('West',-1,-90),('East',1,90)]:
        for i,(x,y) in enumerate(fit['stations']):
            name=f'KIT_Z07_Uplight_{side}_{i:02}';a=labels[name];c=a.static_mesh_component
            assert (a.get_actor_location()-unreal.Vector(sign*x,y,fit['fixture_z'])).length()<.001
            assert (a.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001 and abs(a.get_actor_rotation().yaw-yaw)<.001
            assert c.static_mesh.get_name()=='SM_Aurelion_KIT_PierUplight'
            assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
            assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            light=labels[name+'_Light'];p=light.get_actor_location()
            assert (p-unreal.Vector(sign*(x-fit['optical_offset']),y,fit['light_z'])).length()<.001
            assert (light.get_actor_forward_vector()-unreal.Vector(0,0,1)).length()<.001
            c=light.get_component_by_class(unreal.SpotLightComponent)
            assert c.get_editor_property('mobility')==unreal.ComponentMobility.MOVABLE and c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
            for prop,key in [('intensity','lumens'),('attenuation_radius','attenuation_radius'),('inner_cone_angle','inner_cone'),('outer_cone_angle','outer_cone'),('source_radius','source_radius')]:
                assert abs(c.get_editor_property(prop)-fit[key])<.001
            color=c.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b),fit['color']))<.01
            assert c.get_editor_property('cast_shadows') and c.get_editor_property('use_inverse_squared_falloff') and c.get_editor_property('visible')
            assert c.get_editor_property('indirect_lighting_intensity')==1 and c.get_editor_property('volumetric_scattering_intensity')==0
            rows.append(dict(fixture=name,lumens=fit['lumens'],optical_source_z=p.z,aim='vertical up'))
    return dict(fixtures=rows,qualification='Saved settings and visual fixture fit only; live visibility, final art and packaged GPU performance remain unqualified.')
