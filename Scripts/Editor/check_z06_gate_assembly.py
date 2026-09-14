"""Saved gate housing, dedicated optical fixtures and bounded lighting revisions."""
from pathlib import Path
import json,runpy
import unreal

def check_z06_gate_assembly(world,actors):
    root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/GateLanternKit/lighting-fit.json').read_text());labels={a.get_actor_label():a for a in actors}
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    a=labels['KIT_Z06_GateHousing'];c=a.static_mesh_component
    assert (a.get_actor_location()-unreal.Vector(1100,8460,-600)).length()<.01 and abs(a.get_actor_rotation().yaw-90)<.01
    assert (a.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001
    assert c.static_mesh.get_name()=='SM_Aurelion_KIT_Z06GateHousing' and sm.get_convex_collision_count(c.static_mesh)==5
    assert a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS and str(c.get_collision_profile_name())=='BlockAll'
    o,e=a.get_actor_bounds(False);assert abs(o.z+e.z-25)<.02 and abs(o.z-e.z+600)<.02
    rows=[]
    for row in fit['lanterns']:
        name='KIT_Z06_GateLantern_'+row['name'];a=labels[name];p=unreal.Vector(*row['location']);c=a.static_mesh_component;mesh=c.static_mesh
        assert (a.get_actor_location()-p).length()<.01 and (a.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001
        r=a.get_actor_rotation();assert max(abs(r.pitch),abs(r.yaw),abs(r.roll))<.01
        assert mesh.get_name()=='SM_Aurelion_KIT_GateLantern' and not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled') and sm.get_convex_collision_count(mesh)==0
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        light=labels[name+'_Light'];assert (light.get_actor_location()-p-unreal.Vector(0,0,fit['light_offset_z'])).length()<.01
        # Check direction rather than Euler yaw at the -90 degree singularity.
        assert (light.get_actor_forward_vector()-unreal.Vector(0,0,-1)).length()<.001
        lc=light.get_component_by_class(unreal.RectLightComponent);assert lc.get_editor_property('mobility')==unreal.ComponentMobility.MOVABLE and lc.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS and lc.get_editor_property('cast_shadows')
        for prop,value in [('intensity',row['lumens']),('attenuation_radius',fit['radius']),('source_width',fit['source_width']),('source_height',fit['source_height'])]:assert abs(lc.get_editor_property(prop)-value)<.01
        color=lc.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b),fit['color']))<.01
        rows.append(dict(fixture=name,lumens=row['lumens']))
    for side,sign in [('West',-1),('East',1)]:
        for i in range(4):
            a=labels[f'KIT_Z06_Uplight_{side}_{i:02}_Light'];c=a.get_component_by_class(unreal.SpotLightComponent);p=a.get_actor_location()
            direction=unreal.Vector(sign*fit['uplight_target_x'],p.y,50)-p;direction=direction/direction.length()
            assert (a.get_actor_forward_vector()-direction).length()<.001 and c.get_editor_property('intensity')==fit['uplight_lumens']
    assert labels['ENVL_Z06_Key_01'].get_component_by_class(unreal.RectLightComponent).get_editor_property('intensity')==fit['key_01_lumens']
    gate=runpy.run_path(str(root/'Scripts/Editor/check_z06_rescue_gate.py'))['check_z06_rescue_gate'](world,actors)
    lanes=[];housing=labels['KIT_Z06_GateHousing'];ignored=[a for a in actors if a!=housing]
    for x in (925,1100,1275):
        for z in (-505,-420):
            raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x,8200,z),unreal.Vector(x,8700,z),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert not (hit and hit.to_tuple()[0]),(x,z);lanes.append([x,z])
    return dict(lanterns=rows,gate=gate,aperture_capsules=lanes,qualification='Saved geometry, lights and native sweeps; live interaction, final art and GPU performance remain separate.')
