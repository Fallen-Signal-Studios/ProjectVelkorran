"""Clean-export passage, roof opening and solid roof controls."""
import bpy,json
from mathutils import Vector
from pathlib import Path
root=Path(__file__).resolve().parent/'AtriumCanopyKit';rows=[]
for name in ('Frame','Vault'):
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/f'SM_Aurelion_KIT_AtriumCanopy{name}.fbx'));o=next(o for o in bpy.context.scene.objects if o.type=='MESH')
 for x in (-2.77,-1.7,0,1.7,2.77):
  for z in (.1,1.8,3.5):assert not o.ray_cast(Vector((x,-5,z)),Vector((0,1,0)),distance=10)[0],(name,x,z)
 if name=='Vault':
  for y in (-2,0,2):assert not o.ray_cast(Vector((0,y,4)),Vector((0,0,1)),distance=5)[0]
  for x in (-2.6,-1.5,1.5,2.6):
   for y in (.8,2.8):
    hit,p,n,index=o.ray_cast(Vector((x,y,4)),Vector((0,0,1)),distance=5);assert hit and 5.2<p.z<7.2
 else:assert o.ray_cast(Vector((0,-5,6.95)),Vector((0,1,0)),distance=10)[0]
 rows.append(dict(asset=o.name,passage_rays=15,oculus_rays=3 if name=='Vault' else 0,solid_controls=8 if name=='Vault' else 1))
(root/'clearance-verification.json').write_text(json.dumps(dict(status='PASS',modules=rows,scope='Visual export geometry only; live traversal and rendering acceptance separate'),indent=2));print('ATRIUM_CANOPY_CLEARANCE_PASS')
