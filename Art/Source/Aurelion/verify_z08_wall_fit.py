"""FBX visual openings and wall-face fit; not collision or live wall-run proof."""
import bpy,json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z08WallKit';bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'SM_Aurelion_KIT_Z08WallAssembly.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH');doors=[];faces=[]
for x in (-2.9,0,2.9):
    for z in (.1,2.25,4.4):
        hit=o.ray_cast(Vector((x,-30,z)),Vector((0,1,0)),distance=60)
        assert not hit[0],(x,z,hit)
        doors.append([x,z])
for side in (-1,1):
    for along in range(-22,23,4):
        hit=o.ray_cast(Vector((0,along,2)),Vector((side,0,0)),distance=40);assert hit[0]
        gap=abs(hit[1].x)-34.75;assert -.011<=gap<=.2,(side,along,gap)
        faces.append(dict(axis='x',side=side,along=along,recess_from_native_m=gap))
    for along in list(range(-33,-4,4))+list(range(5,34,4)):
        hit=o.ray_cast(Vector((along,0,2)),Vector((0,side,0)),distance=30);assert hit[0]
        gap=abs(hit[1].y)-23.75;assert -.011<=gap<=.2,(side,along,gap)
        faces.append(dict(axis='y',side=side,along=along,recess_from_native_m=gap))
(root/'visual-fit-verification.json').write_text(json.dumps(dict(status='passed',doorway_rays=doors,wall_face_rays=faces,qualification='FBX visible openings and sampled wall-face recess only; native collision and live wall-running must be checked separately.'),indent=2))
print('Z08_WALL_VISUAL_FIT_PASS')
