"""Full sensor ramp, landing and bridge rails with native guards preserved."""
from pathlib import Path
import json,runpy,unreal

def check_z03_rails(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z03RailKit'
    old=json.loads((source/'rail-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text())
    a=next(a for a in actors if a.get_actor_label()==old['actor'])
    assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    components=list(a.get_components_by_class(unreal.InstancedStaticMeshComponent));assert len(components)==3
    geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);actual=[];fitted={}
    for c in components:
        m=c.static_mesh;spec=next(s for s in manifest['modules'] if s['asset']==m.get_name())
        assert c.get_world_transform().export_text()==old['component_transform']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        n=sm.get_nanite_settings(m);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
        b=m.get_bounds();origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        assert max(abs(2*extent[i]-100*spec['nominal_dimensions_m'][i]) for i in range(3))<.02
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);r=t.rotation.rotator();p=t.translation
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.roll)<.001
            pose=(round(p.x,2),round(p.y,2),round(p.z,2));actual.append((m.get_name(),*pose,round(r.yaw%360,2)))
            fitted[pose]=geo['bounds'](geo['corners'](t,origin,extent))
    assert sorted(actual)==sorted((r['asset'],*r['location_cm'],r['yaw']%360) for r in manifest['placements'])
    assert len(actual)==16 and len(fitted)==16
    used=[]
    for row in manifest['placements']:
        used+=row['original_indices'];points=[]
        for index in row['original_indices']:
            points+=geo['corners'](geo['rail_transform'](old['instances'][index]),old['mesh_origin'],old['mesh_extent'])
        target=geo['bounds'](points);lo,hi=fitted[tuple(row['location_cm'])]
        # All route widths retained. Sloped end bounds deliberately trim tilted legacy overhangs.
        assert max(abs(lo[0]-target[0][0]),abs(hi[0]-target[1][0]))<.02
        if row['asset'].endswith('SlopeRail'):
            # The rounded grip end sits 0.073cm below the ideal sheared-prism corner.
            assert abs(hi[1]-lo[1]-1200)<.02 and abs(hi[2]-430)<.1,(lo,hi)
            assert abs(lo[1]-(row['location_cm'][1]-600))<.02
            assert max(abs(lo[1]-target[0][1]),abs(hi[1]-target[1][1]))<16
            assert abs(lo[2]-target[0][2])<.1 and abs(hi[2]-target[1][2])<2.0
        else:
            assert max(abs(v-w) for xs,ys in zip((lo,hi),target) for v,w in zip(xs,ys))<.02
    assert sorted(used)==list(range(50))
    # Guard spacing leaves >= 354cm on the service route and >= 554cm on the bridge.
    assert fitted[(7788,-18500,0)][0][0]-fitted[(7412,-18500,0)][1][0]>=353.98
    assert fitted[(7288,-14790,0)][0][0]-fitted[(6712,-14790,0)][1][0]>=553.98
    runpy.run_path(str(root/'Scripts/Editor/check_z03_ceiling.py'))['check_z03_ceiling'](actors)
    assert len(actors)==3140
    return dict(rail_runs=16,replaced_vendor_instances=50,sloped_runs=4,landing_runs=2,bridge_sections=10,
                qualification='Full saved rail coverage and unchanged native room/guards; live traversal, all joints and packaged performance remain unqualified.')
