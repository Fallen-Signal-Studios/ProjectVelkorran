"""Compare every paving footprint and top against the saved vendor-floor baseline."""
from pathlib import Path
import json,runpy,unreal
def check_z09_floor(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09FloorKit'
    old=json.loads((source/'floor-baseline.json').read_text(encoding='utf-8-sig'))
    owner=next(a for a in actors if a.get_actor_label()==old['actor'])
    assert len(actors)==3140 and owner.get_actor_transform().export_text()==old['actor_transform'] and owner.get_actor_enable_collision()==old['actor_collision']
    cs=owner.get_components_by_class(unreal.InstancedStaticMeshComponent);assert len(cs)==2
    geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'))
    def box(c,t):
        b=c.static_mesh.get_bounds();return geo['bounds'](geo['corners'](t,[b.origin.x,b.origin.y,b.origin.z],[b.box_extent.x,b.box_extent.y,b.box_extent.z]))
    rows=[];sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for c in cs:
        main=c.static_mesh.get_name()=='SM_Aurelion_KIT_Z09Paving'
        assert main or c.static_mesh.get_name()=='SM_Aurelion_KIT_Z09PavingApproach'
        assert c.get_world_transform().export_text()==old['component_transform']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert not c.get_editor_property('can_ever_affect_navigation') and not c.get_editor_property('override_materials')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        n=sm.get_nanite_settings(c.static_mesh);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1
        assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_simple_collision_count(c.static_mesh)==sm.get_convex_collision_count(c.static_mesh)==0
        expected=old['instances'][:56] if main else old['instances'][56:];assert c.get_instance_count()==len(expected)
        for i,r in enumerate(expected):
            t=c.get_instance_transform(i,world_space=True);assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and t.rotation.angular_distance(unreal.Rotator().quaternion())<.0001
            prior=unreal.Transform(location=unreal.Vector(*r['location']),rotation=unreal.Quat(*r['quaternion']).rotator(),scale=unreal.Vector(*r['scale']))
            lo,hi=geo['bounds'](geo['corners'](prior,old['mesh_origin'],old['mesh_extent']))
            newlo,newhi=box(c,t)
            assert max(abs(lo[j]-newlo[j]) for j in (0,1))<.02 and max(abs(hi[j]-newhi[j]) for j in (0,1))<.02
            assert abs(hi[2]-newhi[2])<.02 and abs((newhi[2]-newlo[2])-12)<.02
            rows.append(dict(source_index=r['index'],bounds_cm=[newlo,newhi]))
    assert len(rows)==60 and len({r['source_index'] for r in rows})==60
    by_path={c.get_path_name():c for a in actors for c in a.get_components_by_class(unreal.StaticMeshComponent)}
    neighbors=json.loads((source/'native-floor-neighbors.json').read_text(encoding='utf-8-sig'));assert neighbors
    for r in neighbors:
        c=by_path[r['component']];a=c.get_owner()
        assert c.static_mesh.get_path_name()==r['mesh'] and str(c.get_collision_enabled())==r['collision'] and a.get_actor_enable_collision()==r['actor_collision']
        assert a.get_editor_property('hidden')==r['actor_hidden'] and c.get_editor_property('visible')==r['visible'] and c.get_editor_property('hidden_in_game')==r['hidden_in_game']
        bounds=box(c,c.get_world_transform());assert max(abs(x-y) for l,h in zip(bounds,r['bounds']) for x,y in zip(l,h))<.02
    return dict(status='passed',paving=rows,actor_count=len(actors),native_neighbors=len(neighbors),qualification='All 60 original XY footprints and walking tops preserved; visual depth increases downward to 12 cm. Native collision unchanged. Live traversal and performance not qualified.')
