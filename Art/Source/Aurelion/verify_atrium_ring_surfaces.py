"""Check the clean exports against the original chord envelope and playable top."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent/'AtriumRingKit'
manifest=json.loads((root/'manifest.json').read_text());rows=[]
for spec in manifest['modules']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/(spec['asset']+'.fbx')))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH');a=math.radians(spec['sector_degrees']);ri=spec['inner_radius_m'];ro=spec['outer_radius_m']
 for v in o.data.vertices:
  p=o.matrix_world@v.co
  r=p.x-p.y*(1-math.cos(a))/math.sin(a);u=-p.y/(r*math.sin(a))
  assert ri-1e-5<=r<=ro+1e-5 and -1e-6<=u<=1+1e-6,(spec['asset'],list(p),r,u)
  assert -2.40001<=p.z<=.00001
 samples=[]
 for fraction in (.08,.2,.37,.61,.82,.94):
  r=ri+(ro-ri)*fraction
  for u in (.11,.31,.56,.89):
   start=Vector((r*(1-u+u*math.cos(a)),-r*u*math.sin(a),1))
   hit,p,n,index=o.ray_cast(start,Vector((0,0,-1)),distance=2)
   assert hit and abs(p.z)<.0001,(spec['asset'],r,u,hit,list(p))
   samples.append(dict(r=r,u=u,z=p.z))
 for r in (ri+.21,ro-.21):
  for offset,expected in ((0,-.001),(.01,-.006),(.05,0)):
   radius=r+offset;u=.5
   start=Vector((radius*(1-u+u*math.cos(a)),-radius*u*math.sin(a),1))
   hit,p,n,index=o.ray_cast(start,Vector((0,0,-1)),distance=2)
   assert hit and abs(p.z-expected)<.0001,(spec['asset'],r,offset,expected,list(p))
 rows.append(dict(asset=spec['asset'],top_samples=samples,inlay_depth_samples=6,chord_envelope='All vertices contained within original XY wedge'))
(root/'surface-verification.json').write_text(json.dumps(dict(status='PASS',modules=rows,scope='Export geometry and sampled surfaces only; no runtime collision or gameplay acceptance'),indent=2))
print('ATRIUM_RING_SURFACES_PASS')
