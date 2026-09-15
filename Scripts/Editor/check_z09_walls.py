"""Authored wall poses must fit every original Z09 art envelope."""
from pathlib import Path
import json,runpy,unreal

def check_z09_walls(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09WallKit'
    old=json.loads((source/'wall-baseline.json').read_text(encoding='utf-8-sig'))
    manifest=json.loads((source/'manifest.json').read_text())
    owner=next(a for a in actors if a.get_actor_label()==old['actor'])
    assert len(actors)==3140 and owner.get_actor_transform().export_text()==old['actor_transform']
    assert owner.get_actor_enable_collision()==old['actor_collision']
    components=owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
    assert len(components)==4
    native=[c for c in owner.get_components_by_class(unreal.StaticMeshComponent) if not isinstance(c,unreal.InstancedStaticMeshComponent)]
    native_state=[[c.get_path_name(),c.get_world_transform().export_text(),c.static_mesh.get_path_name() if c.static_mesh else None,str(c.get_collision_enabled()),str(c.get_collision_profile_name()),c.get_editor_property('visible'),c.get_editor_property('hidden_in_game')] for c in native]
    assert native_state==json.loads((source/'native-components.json').read_text())
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'))
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);rows=[]
    for c in components:
        mesh=c.static_mesh;spec=next(s for s in manifest['modules'] if s['asset']==mesh.get_name())
        assert c.get_world_transform().export_text()==old['component_transform']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert not c.get_editor_property('can_ever_affect_navigation') and not c.get_editor_property('override_materials')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
        n=sm.get_nanite_settings(mesh);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1
        expected=[r for r in manifest['placements'] if r['asset']==mesh.get_name()]
        assert len(expected)==c.get_instance_count()
        b=mesh.get_bounds();origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        assert max(abs(2*extent[i]-100*spec['nominal_dimensions_m'][i]) for i in range(3))<.1
        for i,row in enumerate(expected):
            t=c.get_instance_transform(i,world_space=True)
            assert (t.translation-unreal.Vector(*row['location_cm'])).length()<.02
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001
            assert t.rotation.angular_distance(unreal.Rotator(yaw=row['yaw']).quaternion())<.0001
            original=old['instances'][row['source_index']]
            prior=unreal.Transform(location=unreal.Vector(*original['location']),rotation=unreal.Quat(*original['quaternion']).rotator(),scale=unreal.Vector(*original['scale']))
            oldbox=geometry['bounds'](geometry['corners'](prior,old['mesh_origin'],old['mesh_extent']))
            newbox=geometry['bounds'](geometry['corners'](t,origin,extent))
            assert all(newbox[0][j]>=oldbox[0][j]-1 and newbox[1][j]<=oldbox[1][j]+1 for j in range(3)),(row,newbox,oldbox)
            rows.append(dict(source_index=row['source_index'],mesh=mesh.get_name(),old_bounds_cm=oldbox,new_bounds_cm=newbox))
    assert len(rows)==78 and len({r['source_index'] for r in rows})==78
    return dict(status='passed',parts=rows,actor_count=len(actors),qualification='78 art envelopes within 1 cm of source bounds; native collision preserved separately. Live gallery route and final material/light quality remain open.')
