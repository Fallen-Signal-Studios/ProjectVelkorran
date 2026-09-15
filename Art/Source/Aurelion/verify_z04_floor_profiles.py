"""Clean-FBX walking-plane checks independent of engine placement manifests."""
from pathlib import Path
import bpy,json
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parent/'Z04FloorKit';results=[]
for suffix,w,d,rise,nx,ny in [('Z04Paving',3.875,3.8,0,2,3),('Z04Ascent',6,12,3,3,6),
                              ('Z04Balcony',14,11,0,7,6),('Z04LowCoverCap',3,2,0,2,3),('Z04HighCoverCap',3,3,0,2,3)]:
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/('SM_Aurelion_KIT_'+suffix+'.fbx')))
    o=next(o for o in bpy.context.scene.objects if o.type=='MESH');o.data.calc_loop_triangles()
    tree=BVHTree.FromPolygons([v.co for v in o.data.vertices],[t.vertices for t in o.data.loop_triangles],all_triangles=True);contacts=[]
    for i in range(nx):
        for j in range(ny):
            x=-w/2+(i+.5)*w/nx;y=-d/2+(j+.5)*d/ny;expected=rise*(.5-y/d)
            hit,normal,_,_=tree.ray_cast(Vector((x,y,10)),Vector((0,0,-1)),20)
            assert hit is not None and abs(hit.z-expected)<.0001,(suffix,x,y,hit)
            contacts.append(dict(x_m=x,y_m=y,height_m=hit.z,expected_m=expected))
    results.append(dict(asset=o.name,contacts=contacts))
assert sum(len(r['contacts']) for r in results)==78
(root/'profile-verification.json').write_text(json.dumps(dict(status='PASS',modules=results,
    scope='78 clean-FBX tile-centre walking-plane contacts; bevels, joints and gameplay collision are separate checks.'),indent=2))
print('Z04_FLOOR_PROFILE_PASS')
