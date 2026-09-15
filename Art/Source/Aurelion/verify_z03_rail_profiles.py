"""Clean-FBX downward probes check continuous handrail height across every span."""
from pathlib import Path
import bpy,json
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parent/'Z03RailKit';results=[]
for suffix,w,rise in (('Z03SlopeRail',12,3),('Z03DeckRail',12,0),('Z03BridgeRail',4.2,0)):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root/('SM_Aurelion_KIT_'+suffix+'.fbx')))
    o=next(o for o in bpy.context.scene.objects if o.type=='MESH');o.data.calc_loop_triangles()
    bvh=BVHTree.FromPolygons([v.co for v in o.data.vertices],[t.vertices for t in o.data.loop_triangles],all_triangles=True)
    contacts=[]
    for j in range(25):
        x=-w/2+.025+(w-.05)*j/24
        hit,normal,_,_=bvh.ray_cast(Vector((x,0,10)),Vector((0,0,-1)),20)
        expected=1.3+rise*(x+w/2)/w
        assert hit is not None and abs(hit.z-expected)<.001,(suffix,x,hit,expected)
        contacts.append(dict(x_m=x,height_m=hit.z,expected_height_m=expected))
    results.append(dict(asset=o.name,contacts=contacts))
(root/'profile-verification.json').write_text(json.dumps(dict(status='PASS',modules=results,
    scope='75 vertical clean-FBX probes along the centre of the three handrails; does not qualify runtime collision or all joins.'),indent=2))
print('Z03_RAIL_PROFILE_PASS')
