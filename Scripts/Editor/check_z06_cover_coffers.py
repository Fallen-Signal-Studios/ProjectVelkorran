"""Four fitted instanced cover visuals and retained collision/side-clearance controls."""
from pathlib import Path
import json,itertools
import unreal
def check_z06_cover_coffers(world,actors):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/CoverCofferKit/cover-baseline.json').read_text());labels={a.get_actor_label():a for a in actors}
    art=fit['art'];a=labels[art['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert a.get_actor_transform().export_text()==art['actor_transform'] and c.get_world_transform().export_text()==art['component_transform']
    assert c.get_instance_count()==4 and c.static_mesh.get_name()=='SM_Aurelion_KIT_Z06CoverCoffer' and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);m=c.static_mesh;b=m.get_bounds()
    assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_simple_collision_count(m)==0
    rows=[]
    for i,old in enumerate(art['instances']):
        t=c.get_instance_transform(i,world_space=True);r=t.rotation.rotator();assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.yaw)+abs(r.roll)<.001
        physical=min(fit['physical'],key=lambda p:(p['origin'][0]-t.translation.x)**2+(p['origin'][1]-t.translation.y)**2);a=labels[physical['actor']];pc=a.static_mesh_component
        assert a.get_actor_transform().export_text()==physical['actor_transform'] and pc.static_mesh.get_path_name()==physical['mesh'] and a.get_actor_enable_collision()
        assert str(pc.get_collision_enabled())==physical['collision'] and str(pc.get_collision_profile_name())==physical['profile']
        assert (t.translation-unreal.Vector(physical['origin'][0],physical['origin'][1],-600)).length()<.01
        points=[unreal.MathLibrary.transform_location(t,b.origin+unreal.Vector(x*b.box_extent.x,y*b.box_extent.y,z*b.box_extent.z)) for x,y,z in itertools.product((-1,1),repeat=3)]
        low=[min(getattr(p,k) for p in points) for k in ('x','y','z')];high=[max(getattr(p,k) for p in points) for k in ('x','y','z')]
        assert max(abs(v-w) for actual,expected in zip((low,high),old['bounds']) for v,w in zip(actual,expected))<.02
        ignored=[other for other in actors if other!=a];p=a.get_actor_location();probes=[]
        for dx in (-100,0,100):
            for start,end in [(unreal.Vector(p.x+dx,p.y,-300),unreal.Vector(p.x+dx,p.y,-650)),(unreal.Vector(p.x+dx,p.y-200,-540),unreal.Vector(p.x+dx,p.y,-540)),(unreal.Vector(p.x+dx,p.y+200,-540),unreal.Vector(p.x+dx,p.y,-540))]:
                raw=unreal.SystemLibrary.line_trace_single_by_profile(world,start,end,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
                h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
                assert h and h.to_tuple()[0] and h.to_tuple()[9]==a
                hit=h.to_tuple()[5];expected=-480 if start.z==-300 else p.y+(-60 if start.y<p.y else 60)
                assert abs((hit.z if start.z==-300 else hit.y)-expected)<.01;probes.append([hit.x,hit.y,hit.z])
        for dx in (-200,200):
            raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(p.x+dx,p.y-200,-505),unreal.Vector(p.x+dx,p.y+200,-505),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
            h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert not(h and h.to_tuple()[0])
        rows.append(dict(instance=i,physical=physical['actor'],physical_contacts=probes,clear_side_lanes=2))
    assert len({r['physical'] for r in rows})==4
    return dict(instances=rows,qualification='Editor fit, isolated original cover contacts and side lanes; live combat cover use and final art remain unqualified.')
