"""Check butt-jointed visible decks and whole-world Pawn clearance across both seams."""
import json
from pathlib import Path
import unreal

def check_seams(world,actors):
    labels={a.get_actor_label():a for a in actors}
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/NorthBridgeKit/bridge-fit.json').read_text())
    decks=[labels['KIT_Z02_Bridge_bridge3_ApproachDeck']]+[labels['KIT_North_Bridge_'+b['actor']+'_'+b['asset_prefix']+'Deck'] for b in fit['baseline']]
    rows=[]
    for left,right,y in zip(decks,decks[1:],fit['seam_world_y_cm']):
        lo,le=left.get_actor_bounds(False);ro,re=right.get_actor_bounds(False)
        assert abs(lo.y+le.y-y)<.02 and abs(ro.y-re.y-y)<.02, 'Gap or overlap at shared end plane'
        for x in (-180,0,180):
            for dy in (-2,2):
                hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(-7014.786575+x,y+dy,200),unreal.Vector(-7014.786575+x,y+dy,-100),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
                assert hit and hit.to_tuple()[0]
                assert abs(hit.to_tuple()[5].z-11.134024)<.06
            hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(-7014.786575+x,y-120,104.134024),unreal.Vector(-7014.786575+x,y+120,104.134024),42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
            assert not (hit and hit.to_tuple()[0]), ('Seam blocks Pawn',y,x)
            rows.append(dict(y=y,lane_x=x,clear=True))
    return rows
