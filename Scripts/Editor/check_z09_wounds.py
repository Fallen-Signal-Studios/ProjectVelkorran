"""Saved marker identities, authored mesh references, wall poses and collision policy."""
from pathlib import Path
import json,unreal
def check_z09_wounds(actors):
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z09WoundKit'
    manifest=json.loads((source/'manifest.json').read_text());old=json.loads((source/'marker-baseline.json').read_text(encoding='utf-8-sig'))
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);rows=[];assert len(actors)==3140
    for r in manifest['placements']:
        a=next(a for a in actors if a.get_actor_label()==r['actor']);b=next(b for b in old if b['actor']==r['actor'])
        assert a.get_path_name()==b['path'] and a.get_class().get_name()=='StaticMeshActor'
        cs=a.get_components_by_class(unreal.StaticMeshComponent);assert len(cs)==1;c=cs[0];mesh=c.static_mesh
        assert c.get_path_name()==b['component'] and mesh.get_name()==r['asset']
        assert (a.get_actor_location()-unreal.Vector(*r['location_cm'])).length()<.01 and (a.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001
        assert a.get_actor_rotation().quaternion().angular_distance(unreal.Rotator(yaw=r['yaw']).quaternion())<.001
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert not c.get_editor_property('can_ever_affect_navigation') and not c.get_editor_property('override_materials')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        n=sm.get_nanite_settings(mesh);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
        assert {c.get_material(i).get_name() for i in range(c.get_num_materials())}=={'M_AurelionKit_PavingIvory','M_AurelionKit_EclipseIntrusion','M_AurelionKit_EclipseSeam'}
        rows.append(r)
    return dict(status='passed',fractures=rows,actor_count=len(actors),qualification='Three visual-only north-wall/lintel overlays replace floating decorative cubes. Native walls remain unchanged; visual mounting and live narrative acceptance require separate review.')
