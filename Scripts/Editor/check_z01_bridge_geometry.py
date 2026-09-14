"""Isolated bridge geometry checks. No movement, mission state or map mutations."""
import unreal

def check_bridge(world,bridge,spec,ignored):
    def hit_result(raw):
        if raw is None:return None
        return next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
    def line(start,end,complex=False):
        return hit_result(unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*start),unreal.Vector(*end),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex,ignored,unreal.DrawDebugTrace.NONE,True))
    ox,oy,oz=spec['world_origin_cm']; profile=spec['deck_samples']; rows=[]
    for y,z in profile:
        sy=min(max(y,profile[0][0]+.001),profile[-1][0]-.001)
        wy=oy-sy*100
        for x,height in ((-6200,z*100),(ox+217,(z+1.1)*100)):
            hit=line((x,wy,650),(x,wy,-50))
            assert hit and hit.to_tuple()[0],(x,wy,'Missing simple collision')
            actual=hit.to_tuple()[5].z
            assert abs(actual-height)<1.7,(x,wy,actual,height)
            rows.append(dict(x=x,y=wy,expected_z=height,actual_z=actual))
    center=min(profile,key=lambda p:abs(p[0]))[1]*100
    gap=line((ox-300,oy,center+75),(ox+300,oy,center+75))
    assert gap is None or not gap.to_tuple()[0], 'Balustrade gap blocked'
    control=line((ox-300,oy,center+52),(ox+300,oy,center+52))
    assert control and control.to_tuple()[0], 'Guard rail has no collision'
    capsule_checks=0
    for (ya,za),(yb,zb) in zip(profile,profile[1:]):
        start=unreal.Vector(-6200,oy-ya*100,za*100+93)
        end=unreal.Vector(-6200,oy-yb*100,zb*100+93)
        raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
        hit=hit_result(raw)
        assert hit is None or not hit.to_tuple()[0],(ya,yb,'Deck clearance blocked')
        capsule_checks+=1
    return dict(surface_queries=rows,clear_balustrade_query=True,blocked_guard_control=True,capsule_queries=capsule_checks,
        capsule_radius_cm=42,capsule_half_height_cm=88,clearance_above_profile_cm=5,
        scope='Bridge-only queries with other actors ignored; not a live traversal or encounter pass')
