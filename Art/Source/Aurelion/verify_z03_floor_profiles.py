"""Clean-FBX contact checks on all four walking surface modules."""
from pathlib import Path
import bpy,json
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parent/'Z03FloorKit';results=[]
for suffix,w,d,rise,rows in [('Z03Paving',11/3,4,0,3),('Z03BridgePaving',3,4.224,0,3),('Z03ServiceSlope',4,12,3,6),('Z03ServiceDeck',4,12,0,6)]:
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/('SM_Aurelion_KIT_'+suffix+'.fbx')))
    o=next(o for o in bpy.context.scene.objects if o.type=='MESH');o.data.calc_loop_triangles()
    tree=BVHTree.FromPolygons([v.co for v in o.data.vertices],[t.vertices for t in o.data.loop_triangles],all_triangles=True);contacts=[]
    for x in (-w/4,w/4):
        for j in range(rows):
            y=-d/2+(j+.5)*d/rows;expected=rise*(.5-y/d)
            hit,normal,_,_=tree.ray_cast(Vector((x,y,10)),Vector((0,0,-1)),20)
            assert hit is not None and abs(hit.z-expected)<.0001,(suffix,x,y,hit)
            contacts.append(dict(x_m=x,y_m=y,height_m=hit.z,expected_m=expected))
    results.append(dict(asset=o.name,contacts=contacts))
(root/'profile-verification.json').write_text(json.dumps(dict(status='PASS',modules=results,scope='36 tile-centre clean-FBX walking-plane contacts; bevels, joints and gameplay collision are outside this test.'),indent=2))
print('Z03_FLOOR_PROFILE_PASS')
