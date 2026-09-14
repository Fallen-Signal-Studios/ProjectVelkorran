"""Guardrail coverage against every original instance envelope."""
from pathlib import Path
import itertools,json,unreal

def rail_transform(row):
    t=unreal.Transform();t.translation=unreal.Vector(*row['location']);t.rotation=unreal.Quat(*row['quaternion']);t.scale3d=unreal.Vector(*row['scale']);return t

def corners(t,origin,extent):
    return [unreal.MathLibrary.transform_location(t,unreal.Vector(*[origin[i]+s[i]*extent[i] for i in range(3)])) for s in itertools.product((-1,1),repeat=3)]

def bounds(points):
    return [[fn(getattr(p,k) for p in points) for k in ('x','y','z')] for fn in (min,max)]

def check_z08_railings(actors):
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08RailingKit'
    old=json.loads((source/'rail-baseline.json').read_text());fit=json.loads((source/'rail-fit.json').read_text())
    a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);mesh=c.static_mesh
    assert c.get_path_name()==old['component'] and a.get_actor_transform().export_text()==old['actor_transform'] and c.get_world_transform().export_text()==old['component_transform']
    assert a.get_actor_enable_collision()==old['actor_collision'] and str(c.get_collision_enabled())==old['collision'] and str(c.get_collision_profile_name())==old['profile']
    assert c.get_instance_count()==26 and mesh.get_name()=='SM_Aurelion_KIT_Z08Guardrail'
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);b=mesh.get_bounds()
    assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
    assert sm.get_convex_collision_count(mesh)==0 and sm.get_simple_collision_count(mesh)==0
    origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z];covered=[];results=[]
    for i,row in enumerate(fit['placements']):
        expected=[]
        for index in row['original_indices']:
            expected+=corners(rail_transform(old['instances'][index]),old['mesh_origin'],old['mesh_extent']);covered.append(index)
        actual=bounds(corners(c.get_instance_transform(i,world_space=True),origin,extent));target=bounds(expected)
        error=max(abs(v-w) for av,ev in zip(actual,target) for v,w in zip(av,ev))
        assert error<.02,(i,error,actual,target)
        results.append(dict(index=i,original_indices=row['original_indices'],bounds_error_cm=error))
    assert sorted(covered)==list(range(44)) and len(actors)==3140
    return dict(instances=results,qualification='All 44 former visual envelopes covered by 26 rails; native collision preserved; live and final visual acceptance remain open.')
