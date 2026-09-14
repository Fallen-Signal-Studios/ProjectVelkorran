"""Six inward-facing piers within the original visual envelopes."""
from pathlib import Path
import json,itertools,unreal

def check_z08_columns(actors):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08ColumnKit/column-fit.json').read_text())
    a=next(a for a in actors if a.get_actor_label()==fit['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);mesh=c.static_mesh
    assert a.get_actor_transform().export_text()==fit['actor_transform'] and c.get_world_transform().export_text()==fit['component_transform']
    assert a.get_actor_enable_collision()==fit['actor_collision'] and str(c.get_collision_enabled())==fit['collision'] and str(c.get_collision_profile_name())==fit['profile']
    assert mesh.get_name()=='SM_Aurelion_KIT_Z08EngagedPier' and c.get_instance_count()==6
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
    assert sm.get_simple_collision_count(mesh)==0 and sm.get_convex_collision_count(mesh)==0
    b=mesh.get_bounds();rows=[]
    for row in fit['instances']:
        t=c.get_instance_transform(row['index'],world_space=True);assert (t.scale3d-unreal.Vector(1,1,1)).length()<.00001
        points=[unreal.MathLibrary.transform_location(t,b.origin+unreal.Vector(x*b.box_extent.x,y*b.box_extent.y,z*b.box_extent.z)) for x,y,z in itertools.product((-1,1),repeat=3)]
        bounds=[[fn(getattr(p,k) for p in points) for k in ('x','y','z')] for fn in (min,max)]
        assert max(abs(v-w) for actual,expected in zip(bounds,row['bounds']) for v,w in zip(actual,expected))<.02
        r=t.rotation.rotator();expected=-90 if t.translation.x<0 else 90;assert abs(r.yaw-expected)+abs(r.pitch)+abs(r.roll)<.001
        rows.append(dict(index=row['index'],transform=t.export_text(),bounds=bounds))
    assert len(actors)==3140
    return dict(piers=rows,qualification='Exact visual envelopes and retained collision; live routes, final art and performance unqualified.')
