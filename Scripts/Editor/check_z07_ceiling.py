"""Exact coffer fit, retired decorative bands and isolated native doorway controls."""
import json
from pathlib import Path
import unreal

def check_z07_ceiling(world,actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir())
    fit=json.loads((root/'Art/Source/Aurelion/Z07CeilingKit/ceiling-fit.json').read_text())
    baseline=fit['roof_baseline'];a=labels[baseline['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);m=c.static_mesh
    assert a.get_actor_transform().export_text()==baseline['actor_transform'] and c.get_world_transform().export_text()==baseline['component_transform']
    assert m.get_name()=='SM_Aurelion_KIT_Z07Coffer' and c.get_instance_count()==45
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert not c.get_editor_property('override_materials')
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')
    assert sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
    b=m.get_bounds();assert abs(2*b.box_extent.x-3400/9)<.01 and abs(2*b.box_extent.y-440)<.01
    placed=[]
    for row in fit['placements']:
        t=c.get_instance_transform(row['index'],world_space=True);p=t.translation;r=t.rotation.rotator();s=t.scale3d
        assert abs(p.x-row['x'])<.01 and abs(p.y-row['y'])<.01
        assert max(abs(v) for v in (r.pitch,r.yaw,r.roll))<.001 and max(abs(v-1) for v in (s.x,s.y,s.z))<.001
        assert abs(p.z+b.origin.z+b.box_extent.z+300)<.01
        clearance=p.z+b.origin.z-b.box_extent.z+900;assert clearance>=544.99
        placed.append(dict(index=row['index'],clearance_cm=clearance))
    for row in fit['bands']:
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    for row in fit['physical_walls']:
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert a.get_actor_enable_collision()==row['actor_collision'] and str(c.get_collision_enabled())==row['collision']
    retained=[labels[r['actor']] for r in fit['bands']+fit['physical_walls']];ignored=[a for a in actors if a not in retained];probes=[]
    for y in (13900,16100):
        for x in (-1000,-200,0,200,1000):
            raw=unreal.SystemLibrary.capsule_trace_single(world,unreal.Vector(x,y-350,-805),unreal.Vector(x,y+350,-805),42,88,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next((v for v in raw if isinstance(v,unreal.HitResult)),None) if isinstance(raw,tuple) else raw
            blocked=bool(hit and hit.to_tuple()[0]);assert blocked==(abs(x)==1000),(x,y,blocked)
            if blocked:assert hit.to_tuple()[9] in [labels[r['actor']] for r in fit['physical_walls']]
            probes.append(dict(x=x,y=y,blocked=blocked))
    lights=[]
    for row in fit['lights']:
        c=labels[row['actor']].get_component_by_class(unreal.RectLightComponent);color=c.get_light_color()
        assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b,color.a),row['after_linear']))<.005
        assert abs(c.intensity-row['after_intensity'])<.01 and c.get_world_transform().export_text()==row['transform']
        assert {k:str(c.get_editor_property(k)) for k in row['properties']}==row['properties']
        lights.append(dict(actor=row['actor'],lumens=c.intensity))
    return dict(instances=placed,retired_bands=2,doorway_capsules=probes,lights=lights,qualification='Stopped-editor geometric fit and isolated doorway collision; live traversal, cinematics, wall-running and GPU performance remain unqualified.')
