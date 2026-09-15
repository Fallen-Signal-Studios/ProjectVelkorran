"""Ray-test the exported plinth opening independently of the placement manifest."""
from pathlib import Path
import bpy,json
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z09PlinthKit'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'SM_Aurelion_KIT_Z09SplitPlinth.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH')
samples=[]
for x in (-2.99,-1.5,0,1.5,2.99):
    for z in (.02,.2,.4,.6,.79):
        hit=o.ray_cast(Vector((x,-2,z)),Vector((0,1,0)),distance=4)[0]
        assert not hit,(x,z)
        samples.append(dict(x=x,z=z,blocked=hit))
for x in (-7.8,-5.5,-3.2,3.2,5.5,7.8):
    assert o.ray_cast(Vector((x,-2,.35)),Vector((0,1,0)),distance=4)[0],x
for x,z,material in ((5.5,.715,'M_Aurelion_AncientGold'),(7.93,.435,'M_Aurelion_IvoryStone')):
    hit,location,normal,index=o.ray_cast(Vector((x,-2,z)),Vector((0,1,0)),distance=4)
    assert hit and o.data.materials[o.data.polygons[index].material_index].name==material,(x,z,material)
(root/'opening-verification.json').write_text(json.dumps(dict(status='passed',opening_clear_samples=samples,solid_control_hits=6,qualification='Exported visual mesh rays only; native gameplay collision checked in editor.'),indent=2))
