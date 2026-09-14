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
        hit=o.ray_cast(Vector((side*34.70,along,2)),Vector((side,0,0)),distance=1);assert hit[0]
        gap=abs(hit[1].x)-34.75;assert -.011<=gap<=.2,(side,along,gap)
        faces.append(dict(axis='x',side=side,along=along,recess_from_native_m=gap))
    for along in list(range(-33,-4,4))+list(range(5,34,4)):
        hit=o.ray_cast(Vector((along,side*23.70,2)),Vector((0,side,0)),distance=1);assert hit[0]
        gap=abs(hit[1].y)-23.75;assert -.011<=gap<=.2,(side,along,gap)
        faces.append(dict(axis='y',side=side,along=along,recess_from_native_m=gap))
interior=[]
for row in json.loads((root/'manifest.json').read_text())['interior_coverage']:
    lo,hi=row['bounds_cm'];axis=0 if hi[0]-lo[0]<hi[1]-lo[1] else 1
    center=Vector(((lo[0]+hi[0])/200,-((lo[1]+hi[1])/200-208),(lo[2]+hi[2])/200+12))
    # Probe within this slab's exact envelope so adjacent paired panels cannot
    # substitute for missing coverage. Inset from shared edges and tile seams.
    center.z+=.17;center[1-axis]+=.13
    extent=(hi[axis]-lo[axis])/200
    for side in (-1,1):
        start=center.copy();start[axis]+=side*(extent+.001);direction=Vector((0,0,0));direction[axis]=-side
        hit=o.ray_cast(start,direction,distance=extent*2+.002);assert hit[0],row
    interior.append(row['original_index'])
assert interior==list(range(228,266))
(root/'visual-fit-verification.json').write_text(json.dumps(dict(status='passed',doorway_rays=doors,wall_face_rays=faces,interior_indices=interior,qualification='FBX visible openings, perimeter recess and all 38 partition envelopes; native collision and live wall-running must be checked separately.'),indent=2))
print('Z08_WALL_VISUAL_FIT_PASS')
