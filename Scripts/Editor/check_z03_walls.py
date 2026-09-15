"""Check saved full-size wall poses and native scanner-room clearance."""
from pathlib import Path
import json, runpy, unreal

def check_z03_walls(actors):
    root=Path(unreal.Paths.project_dir()); source=root/'Art/Source/Aurelion/Z03WallKit'
    manifest=json.loads((source/'manifest.json').read_text()); old=json.loads((source/'wall-baseline.json').read_text())
    a=next(a for a in actors if a.get_actor_label()==old['actor'])
    assert a.get_actor_transform().export_text()==old['actor_transform'] and a.get_actor_enable_collision()==old['actor_collision']
    components=list(a.get_components_by_class(unreal.InstancedStaticMeshComponent)); assert len(components)==3
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem); actual=[]
    geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'))
    for c in components:
        m=c.static_mesh; spec=next(s for s in manifest['modules'] if s['asset']==m.get_name())
        assert c.get_world_transform().export_text()==old['component_transform']
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_simple_collision_count(m)==0 and sm.get_convex_collision_count(m)==0
        n=sm.get_nanite_settings(m); assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        b=m.get_bounds(); origin=[b.origin.x,b.origin.y,b.origin.z]; extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        assert max(abs(2*extent[i]-100*spec['nominal_dimensions_m'][i]) for i in range(3))<.02
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True); r=t.rotation.rotator()
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.pitch)+abs(r.roll)<.001
            p=t.translation; actual.append((m.get_name(),round(p.x,2),round(p.y,2),round(p.z,2),round(r.yaw%360,2)))
            lo,hi=geometry['bounds'](geometry['corners'](t,origin,extent))
            # No visual volume may enter the native clear doorway or the room interior.
            assert lo[2]>=-.02 and hi[2]<=655.02
            doorway_overlap=min(hi[0],7300)-max(lo[0],6700)>.02 and min(hi[1],-14975)-max(lo[1],-15025)>.02
            assert not doorway_overlap or lo[2]>=449.98
            interior_overlap=min(hi[0],8075)-max(lo[0],5925)>.02 and min(hi[1],-15025)-max(lo[1],-19775)>.02
            assert not interior_overlap
    expected=[(p['asset'],*p['location_cm'],p['yaw']%360) for p in manifest['placements']]
    assert sorted(actual)==sorted(expected) and len(actual)==34 and len(set(actual))==34
    # This independently guards native room meshes, actor poses and collision against the original survey.
    runpy.run_path(str(root/'Scripts/Editor/check_z03_ceiling.py'))['check_z03_ceiling'](actors)
    return dict(wall_sections=34,retired_vendor_instances=68,doorway_cm=[600,450],
                qualification='Static full wall placement and unchanged native room/scanner staging. Live route and final art acceptance remain open.')
