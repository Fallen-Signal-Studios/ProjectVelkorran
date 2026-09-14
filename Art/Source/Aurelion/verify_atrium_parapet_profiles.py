"""Clean FBX containment within each authored miter and original guard envelope."""
import bpy,json
from pathlib import Path
root=Path(__file__).resolve().parent/'AtriumParapetKit';manifest=json.loads((root/'manifest.json').read_text());rows=[]
for spec in manifest['modules']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/(spec['asset']+'.fbx')))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH');length,la,lb,ra,rb=spec['miter_profile']
 for v in o.data.vertices:
  p=o.matrix_world@v.co
  assert abs(p.x)<=length/2+.00001 and abs(p.y)<=.11001 and -.00001<=p.z<=1.30001
  assert p.x>=la+lb*p.y-.00001 and p.x<=ra+rb*p.y+.00001
 rows.append(dict(asset=spec['asset'],vertices=len(o.data.vertices),contained=True))
(root/'profile-verification.json').write_text(json.dumps(dict(status='PASS',profiles=rows,scope='Export vertices remain within miter planes and original guard envelope'),indent=2))
print('ATRIUM_PARAPET_PROFILES_PASS')
