"""Read-only placement and preserved-geometry checks, including after reload."""
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[2]
out=root/'Saved/Validation/Aurelion/Shuttles-20260913'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
by_name={a.get_name():a for a in actors}; by_label={a.get_actor_label():a for a in actors}
baseline=json.loads((out/'physical-baseline.json').read_text())
for name,row in baseline.items():
    a=by_name[name]
    assert a.get_actor_label()==row['label'] and a.get_actor_transform().export_text()==row['transform'],name
    components={c.get_name():c for c in a.get_components_by_class(unreal.PrimitiveComponent)}
    assert set(components)==set(row['components']),name
    for cname,before in row['components'].items():
        c=components[cname]
        if name=='Ultra_Dynamic_Sky_C_0' and cname in ('BillboardComponent_5','BillboardComponent_6','BillboardComponent_7'):
            assert isinstance(c,unreal.BillboardComponent) and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        else:
            assert c.get_world_transform().export_text()==before['transform'],(name,cname)
        assert str(c.get_collision_enabled())==before['collision'],(name,cname)
        if before['instances'] is not None:
            assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==before['instances'],(name,cname)
old=by_label['Aurelion_Art_M13_Z12_5_9dba0c']
for c in old.get_components_by_class(unreal.InstancedStaticMeshComponent):
    if c.get_name()=='HierarchicalInstancedStaticMesh':
        assert c.get_instance_count()==112 and not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    else:
        assert c.get_instance_count()==60 and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
for faction,x in (('Dominion',-3800),('Reformation',3800)):
    a=by_label['ART_DepartureShuttle_'+faction]; p=a.get_actor_location()
    assert abs(p.x-x)<.1 and abs(p.y-47500)<.1 and abs(p.z)<.1
    s=a.get_actor_scale3d(); assert s==unreal.Vector(1,1,1)
    assert abs(abs(a.get_actor_rotation().yaw)-180)<.1
    c=a.static_mesh_component
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert c.static_mesh.get_name()=='SM_Aurelion_'+faction+'_Shuttle'
    assert c.get_num_materials()==5 and all(c.get_material(i) for i in range(5))
    origin,extent=a.get_actor_bounds(False)
    assert abs(origin.x-x)+extent.x<1400 and abs(origin.y-47500)+extent.y<900
    assert origin.z-extent.z>-.2 and origin.z+extent.z<550
    light=by_label['ENVL_DepartureShuttle_'+faction]; p=light.get_actor_location()
    assert abs(p.x-x)<.1 and abs(p.y-46400)<.1 and abs(p.z-800)<.1
    assert abs(light.get_actor_rotation().pitch+25)<.1
    c=light.get_component_by_class(unreal.RectLightComponent)
    assert c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
    for key,value in (('intensity',4500),('attenuation_radius',2400),('source_width',1600),('source_height',1200)):
        assert abs(c.get_editor_property(key)-value)<.1
report=dict(status='PASS',preserved_existing_actors=len(baseline),ship_envelopes_inside_docks=True,materials_resolved=True,scope='Editor content checks; earned M13, cinematics and packaged GPU performance remain pending')
(out/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('AURELION_SHUTTLES_VERIFIED')
