"""Independent clean-export oculus and overhead clearance controls."""
import bpy,json,math
from mathutils import Vector
from pathlib import Path
root=Path(__file__).resolve().parent/'AtriumCrownKit'
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/'SM_Aurelion_KIT_AtriumOpenCrown.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH')
assert min(v.co.z for v in o.data.vertices)>7.9
clear=[];solid=[]
for i in range(32):
 a=math.radians(i*11.25+3)
 for radius in (0,4.5,8.9):
  x,y=radius*math.cos(a),radius*math.sin(a)
  assert not o.ray_cast(Vector((x,y,0)),Vector((0,0,1)),distance=25)[0]
  clear.append([x,y])
 radius=9.6;x,y=radius*math.cos(a),radius*math.sin(a)
 hit,p,n,index=o.ray_cast(Vector((x,y,0)),Vector((0,0,1)),distance=25)
 assert hit and 13.3<p.z<14.7;solid.append([x,y,float(p.z)])
(root/'clearance-verification.json').write_text(json.dumps(dict(status='passed',minimum_overhead_height_m=min(v.co.z for v in o.data.vertices),oculus_clear_rays=clear,ring_solid_controls=solid,qualification='Source aperture geometry; not gameplay or performance acceptance'),indent=2))
print('PASS: 96 open-oculus rays, 32 solid ring controls, overhead height')
