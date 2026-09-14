"""Imported deck assembly, complete source coverage and unchanged component state."""
from pathlib import Path
import hashlib,json,unreal

def check_z08_decking(actors):
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08DeckKit'
    old=json.loads((source/'deck-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text());coverage=json.loads((source/'coverage.json').read_text());spec=manifest['modules'][-1]
    assert coverage['status']=='passed' and coverage['fbx_sha256']==hashlib.sha256((source/(spec['asset']+'.fbx')).read_bytes()).hexdigest()
    assert sorted(p['original_index'] for p in coverage['panels'])==list(range(42)) and all(len(p['probes'])==5 for p in coverage['panels'])
    assert sorted(p['original_index'] for p in manifest['placements'])==list(range(42))
    a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);mesh=c.static_mesh
    assert c.get_path_name()==old['component'] and a.get_actor_transform().export_text()==old['actor_transform'] and c.get_world_transform().export_text()==old['component_transform']
    assert a.get_actor_enable_collision()==old['actor_collision'] and str(c.get_collision_enabled())==old['collision'] and str(c.get_collision_profile_name())==old['profile']
    assert c.get_instance_count()==1 and mesh.get_name()==spec['asset']
    t=c.get_instance_transform(0,world_space=True);r=t.rotation.rotator()
    assert (t.translation-unreal.Vector(*[v*100 for v in manifest['anchor_world_m']])).length()<.001 and (t.scale3d-unreal.Vector(1,1,1)).length()<.00001
    assert abs(r.pitch)+abs(r.yaw)+abs(r.roll)<.001
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
    assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(spec['asset']+'.fbx')).resolve()
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);settings=sm.get_nanite_settings(mesh);e=mesh.get_bounds().box_extent
    assert sm.get_num_uv_channels(mesh,0)==2 and settings.get_editor_property('enabled')
    assert settings.get_editor_property('fallback_percent_triangles')==1 and settings.get_editor_property('fallback_relative_error')==0
    assert sm.get_simple_collision_count(mesh)==0 and sm.get_convex_collision_count(mesh)==0
    assert max(abs(a-b*100) for a,b in zip((e.x*2,e.y*2,e.z*2),spec['nominal_dimensions_m']))<1
    assert len(actors)==3140
    return dict(retained_surface_footprints=42,exported_surface_probes=210,visual_instances=1,qualification='Source surface coverage and imported fit; live traversal and final art/performance remain unqualified.')
