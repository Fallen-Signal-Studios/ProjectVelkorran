"""Validate fitted cover against measured placements and isolated collision queries."""
import json
import math
import re
from pathlib import Path
import unreal

def vector_text(text): return [float(v) for v in re.findall(r'[XYZ]=([-+0-9.]+)',text)]
def variant(label):
    if label.endswith(('A11','A12')): return 'CoverButtress'
    return 'CoverCofferStack' if label.endswith('A5') else 'CoverCoffer'

def check_cover(world,actors):
    labels={a.get_actor_label():a for a in actors}; sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    baseline=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/CoverKit/placement-baseline.json').read_text())
    def hit(raw):
        if raw is None:return None
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        return h if h.to_tuple()[0] else None
    rows=[]
    for row in baseline:
        a=labels[row['actor']]; c=a.static_mesh_component; mesh=c.static_mesh; scale=a.get_actor_scale3d()
        old_position=vector_text(re.search(r'Translation=\(([^)]+)\)',row['transform']).group(1))
        p=a.get_actor_location(); assert all(abs(v-w)<.001 for v,w in zip((p.x,p.y,p.z),old_position))
        old_rotation=[float(v) for v in re.findall(r'[XYZW]=([-+0-9.]+)',re.search(r'Rotation=\(([^)]+)\)',row['transform']).group(1))]
        q=a.get_actor_transform().rotation
        assert abs(abs(sum(v*w for v,w in zip((q.x,q.y,q.z,q.w),old_rotation)))-1)<.00001
        assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z))
        assert mesh.get_name()=='SM_Aurelion_KIT_'+variant(row['actor'])
        assert sm.get_convex_collision_count(mesh)==1 and sm.get_simple_collision_count(mesh)==0
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
        assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS and str(c.get_collision_profile_name())=='BlockAll'
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and a.get_actor_enable_collision()
        assert len(c.get_editor_property('override_materials'))==0
        expected_o=vector_text(row['bounds_origin']); expected_e=vector_text(row['bounds_extent']); o,e=a.get_actor_bounds(False)
        assert abs(o.x-expected_o[0])<.01 and abs(o.y-expected_o[1])<.01
        assert abs(e.x-expected_e[0])<.02 and abs(e.y-expected_e[1])<.02
        top=expected_o[2]+expected_e[2]; assert abs(o.z+e.z-top)<.02
        expected_bottom=expected_o[2]-expected_e[2]-(4.2158 if row['actor'].endswith('A5') else 0)
        assert abs(o.z-e.z-expected_bottom)<.02
        p=a.get_actor_location(); angle=math.radians(a.get_actor_rotation().yaw); co,si=math.cos(angle),math.sin(angle)
        def point(x,y,z):return unreal.Vector(p.x+co*x-si*y,p.y+si*x+co*y,z)
        ignored=[other for other in actors if other!=a]
        def line(start,end):return hit(unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True))
        for x,y in ((0,0),(-50,-110),(50,-110),(-50,110),(50,110)):
            h=line(point(x,y,top+20),point(x,y,top-30)); assert h and abs(h.to_tuple()[5].z-top)<.03
        for x in (-300,300): assert line(point(x,0,top-25),point(0,0,top-25))
        assert not line(point(-300,0,top+10),point(300,0,top+10))
        for side in (-1,1):
            x=side*(71.7002+42+5)
            raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,point(x,-230,p.z+93),point(x,230,p.z+93),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
            assert not hit(raw),(row['actor'],'Side clearance blocked')
        rows.append(dict(actor=row['actor'],mesh=mesh.get_name(),top_z=top,bottom_z=o.z-e.z,top_queries=5,blocked_side_controls=2,clear_above_control=1,clear_side_capsules=2))
    lower=labels['SM_KB3D_CPI_PropSmallCrate_A6']; upper=labels['SM_KB3D_CPI_PropSmallCrate_A5']
    lo,le=lower.get_actor_bounds(False); uo,ue=upper.get_actor_bounds(False)
    assert abs((uo.z-ue.z)-(lo.z+le.z))<.02
    return dict(cover=rows,stack_seating_gap_cm=(uo.z-ue.z)-(lo.z+le.z),qualification='Isolated authored cover collision; hidden proxies, live navigation, crouching and combat are not qualified.')
