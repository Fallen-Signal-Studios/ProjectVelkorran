"""Verify the exported slab cap stays within six millimetres of its physical top."""
import bpy,json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z06FallenSlabKit'
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/'SM_Aurelion_KIT_Z06FallenMasonry.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH');samples=[]
for x in (-4.8,-10/6,-.5,0,.5,10/6,4.8):
    for y in (-3.3,-1.75,0,1.75,3.3):
        hit,p,n,index=o.ray_cast(Vector((x,y,3)),Vector((0,0,-1)),distance=2)
        assert hit and 2.3939<=p.z<=2.4001 and n.z>.9,(x,y,p)
        samples.append([p.x,p.y,p.z])
assert all(-5.001<=v.co.x<=5.001 and -3.501<=v.co.y<=3.501 and -.001<=v.co.z<=2.401 for v in o.data.vertices)
(root/'surface-verification.json').write_text(json.dumps(dict(status='passed',top_samples=samples,qualification='Exported cap correspondence and overall envelope only; cosmetic edge spalling does not alter the retained collision.'),indent=2))
print('Z06_SLAB_SURFACES_PASS')
