"""Whole-world turn clearance and floor checks at the bridge-to-atrium corridor."""
import unreal

def check_exit(world,expect_clear=True):
    rows=[]
    for y in (-2250,-2200,-2125,-2050,-1975):
        start=unreal.Vector(-6950,y,104.134024);end=unreal.Vector(-6050,y,104.134024)
        hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
        blocker=hit.to_tuple()[9].get_actor_label() if hit and hit.to_tuple()[0] else None
        edge_control=y==-2250
        if expect_clear:
            assert blocker==('A_Connectors_Tarrik_to_Atrium_Guard_1_-1' if edge_control else None),('Atrium turn/edge control failed',y,blocker)
        floors=[]
        for x in (-6950,-6500,-6350,-6200,-6050):
            hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,250),unreal.Vector(x,y,-100),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
            assert hit and hit.to_tuple()[0],('Missing turn floor',x,y)
            z=hit.to_tuple()[5].z;assert -.1<=z<=11.2,(x,y,z)
            floors.append(dict(x=x,z=z))
        rows.append(dict(y=y,edge_control=edge_control,blocker=blocker,floors=floors))
    return rows
