"""Nineteen cargo visuals preserve original transforms and envelopes."""
from pathlib import Path
import json,runpy,unreal

def check_z08_cargo(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z08CargoKit'
    old=json.loads((source/'cargo-baseline.json').read_text());a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);mesh=c.static_mesh
    assert a.get_actor_transform().export_text()==old['actor_transform'] and c.get_world_transform().export_text()==old['component_transform'] and c.get_path_name()==old['component']
    assert a.get_actor_enable_collision()==old['actor_collision'] and str(c.get_collision_enabled())==old['collision'] and str(c.get_collision_profile_name())==old['profile']
    assert mesh.get_name()=='SM_Aurelion_KIT_Z08SealedStores' and c.get_instance_count()==19
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(mesh)
    assert sm.get_num_uv_channels(mesh,0)==2 and n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
    assert sm.get_convex_collision_count(mesh)==0 and sm.get_simple_collision_count(mesh)==0
    helper=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));b=mesh.get_bounds();origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z];rows=[]
    for row in old['instances']:
        t=c.get_instance_transform(row['index'],world_space=True);assert t.export_text()==row['transform']
        actual=helper['bounds'](helper['corners'](t,origin,extent));target=helper['bounds'](helper['corners'](t,old['mesh_origin'],old['mesh_extent']))
        error=max(abs(v-w) for av,ev in zip(actual,target) for v,w in zip(av,ev));assert error<.02,(row['index'],error)
        rows.append(dict(index=row['index'],bounds_error_cm=error))
    assert len(actors)==3140
    return dict(instances=rows,qualification='Measured visual envelopes and collision preserved; final art, live encounter readability and packaged performance remain unqualified.')
