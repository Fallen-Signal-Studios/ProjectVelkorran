"""Clean-FBX grip profiles for all seven relay rail modules."""
from pathlib import Path
import bpy,json
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parent/'Z04RailKit';results=[]
for suffix,w,rise in [('Slope',12,3),('East',10.84,0),('North',14,0),('South',7,0),('Corner',1,0),('West',3.92,0),('Return',.92,0)]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root/('SM_Aurelion_KIT_Z04'+suffix+'Rail.fbx')))
    o=next(o for o in bpy.context.scene.objects if o.type=='MESH');o.data.calc_loop_triangles()
    bvh=BVHTree.FromPolygons([v.co for v in o.data.vertices],[t.vertices for t in o.data.loop_triangles],all_triangles=True);contacts=[]
    for j in range(25):
        x=-w/2+.025+(w-.05)*j/24
        hit,normal,_,_=bvh.ray_cast(Vector((x,0,10)),Vector((0,0,-1)),20)
        expected=(1.3 if rise else 1.1)+rise*(x+w/2)/w
        assert hit is not None and abs(hit.z-expected)<.001,(suffix,x,hit,expected)
        contacts.append(dict(x_m=x,height_m=hit.z,expected_height_m=expected))
    spans={'Slope':6,'East':6,'North':7,'South':4,'Corner':1,'West':2,'Return':1}[suffix]
    feet=[]
    for i in range(spans+1):
        center=-w/2+.08+(w-.16)*i/spans
        for offset in (-.05,0,.05):
            x=center+offset;expected=rise*(x+w/2)/w
            hit,_,_,_=bvh.ray_cast(Vector((x,0,expected-.5)),Vector((0,0,1)),1)
            assert hit is not None and abs(hit.z-expected)<.001,(suffix,'foot',x,hit,expected)
            feet.append(dict(x_m=x,height_m=hit.z,expected_height_m=expected))
    results.append(dict(asset=o.name,contacts=contacts,foot_contacts=feet))
(root/'profile-verification.json').write_text(json.dumps(dict(status='PASS',modules=results,
    scope='175 clean-FBX handrail height probes and 102 foot seating probes. Native collision and assembled joins are checked separately.'),indent=2))
print('Z04_RAIL_PROFILE_PASS')
