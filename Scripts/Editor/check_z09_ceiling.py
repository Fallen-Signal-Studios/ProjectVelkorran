"""Full roof coverage and unchanged gallery headroom/native boundary geometry."""
from pathlib import Path
import json,runpy,unreal

def check_z09_ceiling(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09CeilingKit'
    old=json.loads((source/'ceiling-baseline.json').read_text(encoding='utf-8-sig'))
    a=next(a for a in actors if a.get_actor_label()==old['actor'])
    assert len(actors)==3140 and a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    cs=a.get_components_by_class(unreal.InstancedStaticMeshComponent);assert len(cs)==1;c=cs[0]
    mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z09Coffer')
    assert c.static_mesh==mesh and c.get_world_transform().export_text()==old['component_transform']
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
    assert not c.get_editor_property('can_ever_affect_navigation') and not c.get_editor_property('override_materials')
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and c.get_instance_count()==56
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(mesh)
    assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1
    assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
    b=mesh.get_bounds();o=[b.origin.x,b.origin.y,b.origin.z];e=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
    assert max(abs(e[i]*2-[400,5500/14,55][i]) for i in range(3))<.03
    assert abs(o[2]-e[2])<.01
    geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));rows=[]
    for i in range(56):
        t=c.get_instance_transform(i,world_space=True)
        expected=unreal.Vector(-800+(i//14+.5)*400,25350+(i%14+.5)*5500/14,-900)
        assert (t.translation-expected).length()<.02 and (t.scale3d-unreal.Vector(1,1,1)).length()<.001
        assert t.rotation.angular_distance(unreal.Rotator().quaternion())<.0001
        lo,hi=geo['bounds'](geo['corners'](t,o,e))
        assert lo[2]>=-900.02 and hi[2]<=-844.98
        assert lo[0]>=-800.02 and hi[0]<=800.02 and lo[1]>=25349.98 and hi[1]<=30850.02
        rows.append(dict(index=i,bounds_cm=[lo,hi]))
    by_path={c.get_path_name():c for a in actors for c in a.get_components_by_class(unreal.StaticMeshComponent)}
    neighbors=json.loads((source/'native-roof-neighbors.json').read_text(encoding='utf-8-sig'));assert len(neighbors)==16
    for row in neighbors:
        nc=by_path[row['component']];owner=nc.get_owner();nb=nc.static_mesh.get_bounds()
        assert nc.static_mesh.get_path_name()==row['mesh'] and str(nc.get_collision_enabled())==row['collision']
        assert owner.get_actor_enable_collision()==row['actor_collision'] and owner.get_editor_property('hidden')==row['actor_hidden']
        assert nc.get_editor_property('visible')==row['visible'] and nc.get_editor_property('hidden_in_game')==row['hidden_in_game']
        bounds=geo['bounds'](geo['corners'](nc.get_world_transform(),[nb.origin.x,nb.origin.y,nb.origin.z],[nb.box_extent.x,nb.box_extent.y,nb.box_extent.z]))
        assert max(abs(x-y) for left,right in zip(bounds,row['bounds']) for x,y in zip(left,right))<.02
    return dict(status='passed',coffers=rows,headroom_cm=600,unchanged_native_neighbors=len(neighbors),actor_count=len(actors),qualification='Full 55 x 16 m visual roof, underside at original datum; upward relief intentionally changes roof thickness. Static geometry only; live route, lighting and performance remain separate.')
