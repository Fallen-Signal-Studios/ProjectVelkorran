"""Full relay roof coverage, preserved source lights and added upward wash."""
from pathlib import Path
import json,runpy,unreal

def check_z04_ceiling(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04CeilingKit';old=json.loads((source/'ceiling-baseline.json').read_text());labels={a.get_actor_label():a for a in actors}
    a=labels[old['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);mesh=c.static_mesh
    assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['actor_transform'] and c.get_world_transform().export_text()==old['component_transform']
    assert mesh.get_name()=='SM_Aurelion_KIT_Z04Coffer' and c.get_instance_count()==144 and not c.get_editor_property('override_materials') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert str(c.get_collision_profile_name())=='NoCollision' and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    expected=sorted((round(3900+(i+.5)*6200/16,3),round(-12900+(j+.5)*3800/9,3),700) for i in range(16) for j in range(9));actual=[]
    for i in range(144):
        t=c.get_instance_transform(i,world_space=True);p=t.translation;r=t.rotation.rotator();actual.append((round(p.x,3),round(p.y,3),round(p.z,3)))
        assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.yaw)+abs(r.roll)<.001
    assert sorted(actual)==expected
    b=mesh.get_bounds();assert abs(b.box_extent.x*2-387.5)<.01 and abs(b.box_extent.y*2-3800/9)<.01 and abs(b.origin.z-b.box_extent.z)<.01 and abs(b.origin.z+b.box_extent.z-55)<.01
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(mesh)
    assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0 and sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
    for row in json.loads((source/'light-baseline.json').read_text())['lights']:
        actor=labels[row['actor']];light=next(c for c in actor.get_components_by_class(unreal.LightComponent) if c.get_path_name()==row['component'])
        assert light.get_world_transform().export_text()==row['transform']
        for k,v in row['properties'].items():assert str(light.get_editor_property(k))==v,(row['actor'],k)
        color=light.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b,color.a),row['color']))<.0001
    for index,(x,y) in enumerate(((5000,-11900),(9000,-11900),(5000,-10100),(9000,-10100)),1):
        a=labels['ENVL_Z04_CeilingBounce_'+str(index)];lights=list(a.get_components_by_class(unreal.RectLightComponent));assert len(lights)==2
        light=next(c for c in lights if c.get_name()=='CofferWash');p=light.get_world_location();f=light.get_forward_vector()
        assert (p-unreal.Vector(x,y,350)).length()<.001 and (f-unreal.Vector(0,0,1)).length()<.001
        assert light.intensity==500 and light.attenuation_radius==3500 and light.source_width==light.source_height==800 and light.cast_shadows and light.volumetric_scattering_intensity==0
        assert light.intensity_units==unreal.LightUnits.LUMENS and light.mobility==unreal.ComponentMobility.MOVABLE
        color=light.get_editor_property('light_color');assert (color.r,color.g,color.b,color.a)==(226,233,255,255)
    runpy.run_path(str(root/'Scripts/Editor/check_z04_walls.py'))['check_z04_walls'](actors)
    lights=list(labels['ENVL_Z04_Key_01'].get_components_by_class(unreal.RectLightComponent));assert len(lights)==3
    for index,y in enumerate((-11900,-10100),1):
        light=next(c for c in lights if c.get_name()=='CenterWash'+str(index))
        assert (light.get_world_location()-unreal.Vector(7000,y,350)).length()<.001 and (light.get_forward_vector()-unreal.Vector(0,0,1)).length()<.001
        assert light.intensity==250 and light.attenuation_radius==3500 and light.source_width==light.source_height==800 and light.cast_shadows and light.volumetric_scattering_intensity==0
        assert light.intensity_units==unreal.LightUnits.LUMENS and light.mobility==unreal.ComponentMobility.MOVABLE
        color=light.get_editor_property('light_color');assert (color.r,color.g,color.b,color.a)==(226,233,255,255)
    return dict(coffers=144,covered_area_m2=2356,minimum_visual_height_cm=700,roof_top_cm=755,upward_washes=6,side_wash_lumens_each=500,center_wash_lumens_each=250,qualification='Full saved roof coverage and added ceiling illumination; live combat readability and packaged performance remain unqualified.')
