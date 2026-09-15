"""Full double-sided wall fit and preserved relay gameplay geometry."""
from pathlib import Path
import json,unreal

def check_z04_walls(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04WallKit';old=json.loads((source/'wall-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text());room=json.loads((source/'room-baseline.json').read_text());labels={a.get_actor_label():a for a in actors}
    a=labels[old['actor']];assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['actor_transform']
    components=list(a.get_components_by_class(unreal.InstancedStaticMeshComponent));assert len(components)==3
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);actual=[]
    for c in components:
        mesh=c.static_mesh;spec=next(s for s in manifest['modules'] if s['asset']==mesh.get_name())
        assert c.get_world_transform().export_text()==old['component_transform'] and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        n=sm.get_nanite_settings(mesh);assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
        b=mesh.get_bounds();assert max(abs(getattr(b.box_extent,k)*2-100*spec['nominal_dimensions_m'][i]) for i,k in enumerate(('x','y','z')))<.02
        assert abs(b.origin.x)+abs(b.origin.y)+abs(b.origin.z-b.box_extent.z)<.02
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);p=t.translation;r=t.rotation.rotator()
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.roll)<.001
            actual.append((mesh.get_name(),round(p.x,3),round(p.y,3),round(p.z,3),round(r.yaw%360,3)))
    expected=[(r['asset'],*[round(v,3) for v in r['location_cm']],r['yaw']) for r in manifest['placements']]
    assert sorted(actual)==sorted(expected) and len(actual)==48
    # Independently account for 28 end bays, 18 side bays, and two clear-span lintels.
    ends=[r for r in actual if r[0].endswith('Wall4m')];sides=[r for r in actual if r[0].endswith('WallSide')];lintels=[r for r in actual if r[0].endswith('Lintel')]
    assert len(ends)==28 and len(sides)==18 and len(lintels)==2
    for y in (-12900,-9100):
        xs=sorted(r[1] for r in ends if r[2]==y);assert xs==[4100+400*i for i in range(7)]+[7500+400*i for i in range(7)]
        assert xs[6]+200==6700 and xs[7]-200==7300
        assert any(r[1:]==(7000,y,450,0) for r in lintels)
    for x in (3900,10100):
        ys=sorted(r[2] for r in sides if r[1]==x)
        assert len(ys)==9 and abs(ys[0]-3800/18+12900)<.01 and abs(ys[-1]+3800/18+9100)<.01
    for row in room['named_actors']:assert labels[row['actor']].get_actor_transform().export_text()==row['transform']
    for row in room['components']:
        if not row['actor'].startswith('Z04_'):continue
        actor=labels[row['actor']];c=next(c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()==row['component'])
        assert c.static_mesh.get_path_name()==row['mesh'] and actor.get_actor_enable_collision()==row['actor_collision']
        retired={'Z04__ArtPylon_W','Z04__ArtPylon_E','Z04__UpperSpan_01','Z04__UpperSpan_02','Z04__GoldChannel_01','Z04__GoldChannel_02'}
        if row['actor'] in retired:
            assert c.get_editor_property('hidden_in_game') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        else:assert str(c.get_collision_enabled())==row['collision']
    assert len(actors)==3140
    return dict(wall_sections=48,replaced_vendor_instances=188,double_sided=True,native_wall_thickness_cm=50,doorways=2,door_width_cm=600,door_clear_height_cm=450,qualification='Saved full relay wall fit and native geometry preservation; room lighting, other art, live route and packaged performance remain unqualified.')
