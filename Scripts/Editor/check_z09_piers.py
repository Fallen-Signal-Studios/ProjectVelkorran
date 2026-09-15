"""Eight unit-scale custom piers must retain their former visual envelopes."""
from pathlib import Path
import json,runpy,unreal

def check_z09_piers(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09PierKit'
    old=json.loads((source/'pier-baseline.json').read_text(encoding='utf-8-sig'));manifest=json.loads((source/'manifest.json').read_text())
    a=next(a for a in actors if a.get_actor_label()==old['actor'])
    assert len(actors)==3140 and a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    cs=a.get_components_by_class(unreal.InstancedStaticMeshComponent);assert len(cs)==1;c=cs[0]
    mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z09EngagedPier')
    assert c.static_mesh==mesh and c.get_world_transform().export_text()==old['component_transform']
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
    assert not c.get_editor_property('can_ever_affect_navigation') and not c.get_editor_property('override_materials')
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and c.get_instance_count()==8
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(mesh)
    assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1
    assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
    b=mesh.get_bounds();o=[b.origin.x,b.origin.y,b.origin.z];e=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
    assert max(abs(e[i]*2-[120,100,600][i]) for i in range(3))<.02
    geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));rows=[]
    for i,row in enumerate(manifest['placements']):
        t=c.get_instance_transform(i,world_space=True)
        assert (t.translation-unreal.Vector(*row['location_cm'])).length()<.02 and (t.scale3d-unreal.Vector(1,1,1)).length()<.001
        assert t.rotation.angular_distance(unreal.Rotator(yaw=row['yaw']).quaternion())<.0001
        prior=old['instances'][i]
        pt=unreal.Transform(location=unreal.Vector(*prior['location']),rotation=unreal.Quat(*prior['quaternion']).rotator(),scale=unreal.Vector(*prior['scale']))
        original=geo['bounds'](geo['corners'](pt,old['mesh_origin'],old['mesh_extent']))
        current=geo['bounds'](geo['corners'](t,o,e))
        assert max(abs(x-y) for left,right in zip(original,current) for x,y in zip(left,right))<.02,(i,original,current)
        rows.append(dict(index=i,old_bounds_cm=original,new_bounds_cm=current))
    return dict(status='passed',piers=rows,actor_count=len(actors),qualification='Static eight-pier fit at unit scale, preserving prior envelopes. Structural piers remain non-destructible; live route and performance are separate qualification.')
