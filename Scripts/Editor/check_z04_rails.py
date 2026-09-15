"""Verify all relay rail runs, corner trimming and native walking support."""
from pathlib import Path
import json,runpy,unreal

def check_z04_rails(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04RailKit'
    old=json.loads((source/'rail-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text())
    labels={a.get_actor_label():a for a in actors};a=labels[old['actor']]
    assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    components=a.get_components_by_class(unreal.InstancedStaticMeshComponent);assert len(components)==7
    geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    actual=[];fitted={}
    for c in components:
        m=c.static_mesh;spec=next(s for s in manifest['modules'] if s['asset']==m.get_name())
        assert c.get_world_transform().export_text()==old['component_transform']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        n=sm.get_nanite_settings(m);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
        b=m.get_bounds();origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        assert max(abs(2*extent[i]-100*spec['nominal_dimensions_m'][i]) for i in range(3))<.02
        for index in range(c.get_instance_count()):
            t=c.get_instance_transform(index,world_space=True);p=t.translation;r=t.rotation.rotator()
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.roll)<.001
            pose=tuple(round(v,2) for v in (p.x,p.y,p.z))
            actual.append((m.get_name(),*pose,round(r.yaw%360,2)));fitted[pose]=geo['bounds'](geo['corners'](t,origin,extent))
    assert sorted(actual)==sorted((r['asset'],*r['location_cm'],r['yaw']%360) for r in manifest['placements']) and len(actual)==10
    used=[]
    for row in manifest['placements']:
        used+=row['original_indices'];points=[]
        for index in row['original_indices']:points+=geo['corners'](geo['rail_transform'](old['instances'][index]),old['mesh_origin'],old['mesh_extent'])
        target=geo['bounds'](points);lo,hi=fitted[tuple(row['location_cm'])]
        long=1 if row['yaw']==90 else 0;across=1-long
        assert abs(lo[across]-target[0][across])<.02 and abs(hi[across]-target[1][across])<.02
        if row['asset'].endswith('SlopeRail'):
            assert abs(hi[long]-lo[long]-1200)<.02 and abs(hi[2]-430)<.1 and abs(lo[2])<.1
            assert abs(lo[long]-(row['location_cm'][long]-600))<.02
            assert max(abs(lo[long]-target[0][long]),abs(hi[long]-target[1][long]))<16
            assert abs(lo[2]-target[0][2])<2.1 and abs(hi[2]-target[1][2])<2.1
        else:
            low_trim=4.96 if row['asset'].endswith(('EastRail','WestRail')) else 0
            high_trim=4.96 if row['asset'].endswith(('EastRail','ReturnRail')) else 0
            assert abs(lo[long]-target[0][long]-low_trim)<.02 and abs(hi[long]-target[1][long]+high_trim)<.02
            assert abs(lo[2]-300)<.02 and abs(hi[2]-410)<.02
    assert sorted(used)==list(range(35))
    # Both routes retain 554cm between rail feet; both balcony entrances remain open.
    assert fitted[(9588,-11200,0)][0][0]-fitted[(9012,-11200,0)][1][0]>=553.98
    assert fitted[(7700,-9612,0)][0][1]-fitted[(7700,-10188,0)][1][1]>=553.98
    assert fitted[(9650,-10600,300)][0][0]-fitted[(8650,-10600,300)][1][0]>=599.98
    assert fitted[(8300,-9554,300)][0][1]-fitted[(8300,-10396,300)][1][1]>=599.98
    assert not json.loads((source/'assembly-coplanar.json').read_text())['overlaps']
    runpy.run_path(str(root/'Scripts/Editor/check_z04_floors.py'))['check_z04_floors'](actors)
    contacts=[]
    for row in manifest['placements']:
        suffix=row['asset'].replace('SM_Aurelion_KIT_Z04','').replace('Rail','')
        w,spans={'Slope':(1200,6),'East':(1084,6),'North':(1400,7),'South':(700,4),'Corner':(100,1),'West':(392,2),'Return':(92,1)}[suffix]
        x,y,z=row['location_cm']
        for i in range(spans+1):
            offset=-w/2+8+(w-16)*i/spans
            px,py=(x,y+offset) if row['yaw']==90 else (x+offset,y)
            expected=150+offset*.25 if suffix=='Slope' else 300
            label=('Z04_SouthAscent_6x12_Rise3m' if row['yaw']==90 else 'Z04_WestDescent_12x6_Rise3m') if suffix=='Slope' else 'Z04_FlankBalcony_14x11_Top3m'
            if suffix=='East':px-=2
            if suffix in ('West','Return'):px+=2
            if suffix=='North':py-=2
            if suffix in ('South','Corner'):py+=2
            body=labels[label].get_component_by_class(unreal.StaticMeshComponent)
            hit=body.line_trace_component(unreal.Vector(px,py,expected+15),unreal.Vector(px,py,expected-30),False,False,False)
            assert hit is not None and abs(hit[0].z-expected)<.02,(label,px,py,expected,hit)
            contacts.append(dict(actor=label,x_cm=px,y_cm=py,height_cm=hit[0].z))
    assert len(contacts)==55 and len(actors)==3140
    return dict(rail_runs=10,ramp_runs=4,balcony_runs=6,replaced_vendor_instances=35,native_foot_contacts=contacts,
                qualification='Saved fit, source joins and native support only; live traversal and packaged performance remain unqualified.')
