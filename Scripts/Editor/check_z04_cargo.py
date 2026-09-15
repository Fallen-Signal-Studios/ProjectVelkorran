"""Relay stores fit against each native cover envelope and preceding architecture."""
from pathlib import Path
import json,runpy,unreal
def check_z04_cargo(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04CargoKit'
    old=json.loads((source/'cargo-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text())
    labels={a.get_actor_label():a for a in actors};a=labels[old['actor']]
    assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    cs=a.get_components_by_class(unreal.InstancedStaticMeshComponent);assert len(cs)==5
    actual=[];fitted={}
    for c in cs:
        m=c.static_mesh;spec=next(s for s in manifest['modules'] if s['asset']==m.get_name())
        assert c.get_world_transform().export_text()==old['component_transform']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        n=sm.get_nanite_settings(m)
        assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
        b=m.get_bounds();origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        assert max(abs(2*extent[i]-100*spec['nominal_dimensions_m'][i]) for i in range(3))<.02
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);p=t.translation;r=t.rotation.rotator()
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.yaw)+abs(r.roll)<.001
            pose=tuple(round(v,2) for v in (p.x,p.y,p.z));actual.append((m.get_name(),*pose))
            fitted[pose]=geo['bounds'](geo['corners'](t,origin,extent))
    assert sorted(actual)==sorted((r['asset'],*r['location_cm']) for r in manifest['placements']) and len(actual)==9
    assert sorted(i for r in manifest['placements'] for i in r['original_indices'])==list(range(13))
    contacts=[]
    for row in manifest['placements']:
        native=next(c for c in old['native_covers'] if c['actor']==row['native_actor']);owner=labels[native['actor']]
        body=owner.get_component_by_class(unreal.StaticMeshComponent)
        assert owner.get_actor_transform().export_text()==native['transform'] and body.get_path_name()==native['component']
        assert str(body.get_collision_enabled())==native['collision'] and str(body.get_collision_profile_name())==native['profile']
        target=[[v-e for v,e in zip(native['origin'],native['extent'])],[v+e for v,e in zip(native['origin'],native['extent'])]]
        # Cap-backed assemblies terminate exactly at the retained cap underside.
        if not row['original_indices']:target[1][2]-=7.5
        bounds=fitted[tuple(row['location_cm'])]
        assert max(abs(v-w) for av,ev in zip(bounds,target) for v,w in zip(av,ev))<.02,(row['native_actor'],bounds,target)
        # Two real component traces establish the retained obstruction in each axis.
        for axis in (0,1):
            start=native['origin'].copy();end=start.copy();start[axis]-=native['extent'][axis]+10;end[axis]+=native['extent'][axis]+10
            hit=body.line_trace_component(unreal.Vector(*start),unreal.Vector(*end),False,False,False);assert hit
            assert (hit[0]-unreal.Vector(*native['contacts'][axis])).length()<.02
            contacts.append(dict(actor=native['actor'],axis=axis,contact_cm=[hit[0].x,hit[0].y,hit[0].z]))
        if row['original_indices']:
            points=[]
            for i in row['original_indices']:points+=geo['corners'](geo['rail_transform'](old['instances'][i]),old['mesh_origin'],old['mesh_extent'])
            prior=geo['bounds'](points)
            assert max(abs(v-w) for av,ev in zip(bounds,prior) for v,w in zip(av,ev))<.02
    runpy.run_path(str(root/'Scripts/Editor/check_z04_rails.py'))['check_z04_rails'](actors)
    assert len(actors)==3140
    return dict(assemblies=9,replaced_vendor_instances=13,new_cap_supports=2,native_cover_contacts=contacts,
        qualification='Static envelopes and retained native collision. Live cover, Chaos, save/load and packaged performance remain unqualified.')
