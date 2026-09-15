"""Grounded end-wall plinths, explicit central opening and visual-only collision."""
from pathlib import Path
import json,unreal
def check_z09_plinths(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09PlinthKit'
    manifest=json.loads((source/'manifest.json').read_text());old=json.loads((source/'band-baseline.json').read_text(encoding='utf-8-sig'))
    mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z09SplitPlinth');assert mesh
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(mesh)
    assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1
    assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
    assert len(actors)==3140
    rows=[]
    for r in manifest['placements']:
        a=next(a for a in actors if a.get_actor_label()==r['actor']);baseline=next(b for b in old if b['actor']==r['actor'])
        assert a.get_path_name()==baseline['path'] and a.get_class().get_name()=='StaticMeshActor'
        cs=a.get_components_by_class(unreal.StaticMeshComponent);assert len(cs)==1;c=cs[0]
        assert c.get_path_name()==baseline['component'] and c.static_mesh==mesh
        assert (a.get_actor_location()-unreal.Vector(*r['location_cm'])).length()<.01
        assert (a.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001
        assert a.get_actor_rotation().quaternion().angular_distance(unreal.Rotator(yaw=r['yaw']).quaternion())<.001
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert not c.get_editor_property('can_ever_affect_navigation') and not c.get_editor_property('override_materials')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        rows.append(dict(actor=r['actor'],location_cm=r['location_cm'],yaw=r['yaw']))
    return dict(status='passed',plinths=rows,actor_count=len(actors),qualification='Two old floating art bands become floor-mounted split plinths. Their art collision is intentionally removed; native end-wall collision is retained and checked by prior floor verification. Six-metre visual opening is source-ray verified. Live route not qualified.')
