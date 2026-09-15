"""Clean FBX probes prove corrected coverage and absence at the stale locations."""
from pathlib import Path
import bpy,json
from mathutils import Vector
base=Path(__file__).resolve().parent;source=base/'CacheEnclosureKit'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(base/'Z08WallKit/SM_Aurelion_KIT_Z08WallAssembly.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH');checks=[]
for row in json.loads((source/'skin-relocation.json').read_text())['rows']:
    for key,expected in (('old_bounds_cm',False),('bounds_cm',True)):
        lo,hi=row[key];axis=0 if hi[0]-lo[0]<hi[1]-lo[1] else 1
        center=Vector(((lo[0]+hi[0])/200,-((lo[1]+hi[1])/200-208),(lo[2]+hi[2])/200+12))
        center[1-axis]+=.13
        extent=(hi[axis]-lo[axis])/200
        for z in (.5,1.2,2.3):
            center.z=z
            for side in (-1,1):
                start=center.copy();start[axis]+=side*(extent+.002);direction=Vector((0,0,0));direction[axis]=-side
                hit=o.ray_cast(start,direction,distance=extent*2+.004)
                assert hit[0]==expected,(row['actor'],key,z,side,hit)
                checks.append(dict(actor=row['actor'],location=key,z=z,side=side,hit=hit[0]))
(source/'relocation-verification.json').write_text(json.dumps(dict(status='passed',checks=checks,qualification='18 corrected surface contacts and 18 empty former-location probes on exported assembly; native collision qualified separately.'),indent=2))
print('CACHE_SKIN_RELOCATION_PASS')
