"""Exported landing/ramp top and footprint correspondence with the measured proxies."""
import bpy,json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z06RefugeKit';manifest=json.loads((root/'manifest.json').read_text());rows=[]
for spec in manifest['modules']:
    bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/(spec['asset']+'.fbx')))
    obj=next(o for o in bpy.context.scene.objects if o.type=='MESH');length,width,height=spec['nominal_dimensions_m'];samples=[]
    for fx in (-.49,-.45,-.3,-.15,0,.15,.3,.45,.49):
        for fy in (-.49,-.45,-.3,-.15,0,.15,.3,.45,.49):
            hit,p,n,index=obj.ray_cast(Vector((length*fx,width*fy,1)),Vector((0,0,-1)),distance=2)
            assert hit and .1939<=p.z<=.2001 and n.z>.9,(spec['asset'],fx,fy,p)
            samples.append([p.x,p.y,p.z])
    assert all(abs(v.co.x)<=length/2+.0001 and abs(v.co.y)<=width/2+.0001 and -.2001<=v.co.z<=.2001 for v in obj.data.vertices)
    gold_samples=[]
    for sign in (-1,1):
        for origin,direction in ((Vector((0,sign*(width/2+1),-.045)),Vector((0,-sign,0))),(Vector((sign*(length/2+1),0,-.045)),Vector((-sign,0,0)))):
            hit,p,n,index=obj.ray_cast(origin,direction,distance=2)
            assert hit and obj.data.materials[obj.data.polygons[index].material_index].name=='M_Aurelion_AncientGold',(spec['asset'],origin,index)
            gold_samples.append([p.x,p.y,p.z])
    rows.append(dict(asset=spec['asset'],samples=samples,gold_visibility_samples=gold_samples))
(root/'surface-verification.json').write_text(json.dumps(dict(status='passed',modules=rows,qualification='162 source rays and envelope checks; not Unreal fit, ramp traversal or visual acceptance.'),indent=2))
print('Z06_REFUGE_SURFACES_PASS')
