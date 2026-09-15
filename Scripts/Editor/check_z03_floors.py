"""Full sensor floor, platform and bridge fit with native floor contacts."""
from pathlib import Path
import json,runpy,unreal

def check_z03_floors(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z03FloorKit'
    old=json.loads((source/'floor-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text());labels={a.get_actor_label():a for a in actors}
    a=labels[old['actor']];assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    components=list(a.get_components_by_class(unreal.InstancedStaticMeshComponent));assert len(components)==4
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));actual=[];footprints={}
    for c in components:
        m=c.static_mesh;spec=next(s for s in manifest['modules'] if s['asset']==m.get_name())
        assert c.get_world_transform().export_text()==old['component_transform'] and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        n=sm.get_nanite_settings(m);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
        slots={str(s.material_slot_name):s.material_interface for s in m.get_editor_property('static_materials')}
        stone=slots['M_Aurelion_IvoryStone']
        assert stone.get_path_name()=='/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_Z03FloorStone.M_AurelionKit_Z03FloorStone'
        ml=unreal.MaterialEditingLibrary
        assert abs(ml.get_material_property_input_node(stone,unreal.MaterialProperty.MP_BASE_COLOR).get_editor_property('const_alpha')-.65)<.0001
        assert abs(ml.get_material_property_input_node(stone,unreal.MaterialProperty.MP_NORMAL).get_editor_property('const_alpha')-.22)<.0001
        b=m.get_bounds();origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        assert max(abs(2*extent[i]-100*spec['nominal_dimensions_m'][i]) for i in range(3))<.02
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);p=t.translation;r=t.rotation.rotator()
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.roll)<.001
            pose=(round(p.x,3),round(p.y,3),round(p.z,3));actual.append((m.get_name(),*pose,round(r.yaw%360,3)))
            footprints[pose]=geo['bounds'](geo['corners'](t,origin,extent))
    assert sorted(actual)==sorted((r['asset'],*[round(v,3) for v in r['location_cm']],r['yaw']%360) for r in manifest['placements']) and len(actual)==85
    # Independent room grid and bridge envelopes against the original saved vendor geometry.
    room=[r for r in actual if r[0].endswith('Z03Paving')];assert len(room)==72
    xs=sorted(set(r[1] for r in room));ys=sorted(set(r[2] for r in room))
    assert len(xs)==6 and len(ys)==12 and abs(xs[0]-1100/6-5900)<.01 and abs(xs[-1]+1100/6-8100)<.01 and ys[0]-200==-19800 and ys[-1]+200==-15000
    for index in range(81,91):
        row=old['instances'][index];target=geo['bounds'](geo['corners'](geo['rail_transform'](row),old['mesh_origin'],old['mesh_extent']))
        pose=(round((target[0][0]+target[1][0])/2,3),round((target[0][1]+target[1][1])/2,3),0.)
        fitted=footprints[pose]
        assert max(abs(fitted[s][i]-target[s][i]) for s in (0,1) for i in (0,1))<.02 and abs(fitted[1][2])<.02
    runpy.run_path(str(root/'Scripts/Editor/check_z03_ceiling.py'))['check_z03_ceiling'](actors)
    contacts=[]
    def contact(label,x,y,z):
        body=labels[label].get_component_by_class(unreal.StaticMeshComponent)
        hit=body.line_trace_component(unreal.Vector(x,y,z+20),unreal.Vector(x,y,z-60),False,False,False)
        assert hit is not None and abs(hit[0].z-z)<.02,(label,x,y,z,hit)
        contacts.append(dict(actor=label,x_cm=x,y_cm=y,height_cm=hit[0].z))
    for r in room:contact('Z03_Floor',r[1],r[2],0)
    for label,center,rise in [('Z03_Service_Up',-18500,1),('Z03_Service_Down',-16100,-1),('Z03_Service_Deck',-17300,0)]:
        for x in (7500,7700):
            for offset in (-500,-300,-100,100,300,500):contact(label,x,center+offset,150+offset*.25*rise if rise else 300)
    assert len(contacts)==108 and len(actors)==3140
    return dict(room_bays=72,bridge_bays=10,platform_sections=3,replaced_vendor_instances=91,native_contacts=contacts,
        qualification='Full saved floor fit and retained native support planes; live route, final art and packaged performance remain unqualified.')
