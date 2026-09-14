"""Clean FBX surface, end-envelope and retained-instance ownership checks."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent;source=root/'AtriumBridgeFloorKit'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source/'SM_Aurelion_KIT_AtriumBridgeFloor.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH');slope=math.tan(math.radians(5.625))
reference=json.loads((root/'AtriumRingKit/reference-vertices.json').read_text());joint_checks=[]
for row in reference:
 radius=28 if '28_36' in row['file'] else 18
 edge=[v for v in row['vertices'] if v[2]==0 and abs(math.hypot(v[0],v[1])-radius)<.00001]
 assert len(edge)==2
 p0,p1=sorted(edge,key=lambda p:p[1])
 for y in (0,.7,1.7,2.7,3):
  measured_x=p0[0]+(-y-p0[1])*(p1[0]-p0[0])/(p1[1]-p0[1])
  expected_x=radius-y*slope
  assert abs(measured_x-expected_x)<.00001
  joint_checks.append(dict(radius=radius,lateral=y,reference_x=measured_x,authored_x=expected_x))
for v in o.data.vertices:
 x,y,z=v.co
 assert -3.00001<=y<=3.00001 and -1.32001<=z<=.00001
 assert -5-abs(y)*slope-.00001<=x<=5-abs(y)*slope+.00001
samples=[]
for y in (-2.82,-2.3,-1.49,-.68,.68,1.49,2.3,2.82):
 for u in (.013,.23,.47,.73,.987):
  x=-5+10*u-abs(y)*slope;hit,p,n,index=o.ray_cast(Vector((x,y,1)),Vector((0,0,-1)),distance=3)
  assert hit and abs(p.z)<.00001,(x,y,p)
  samples.append([x,y,float(p.z)])
for y,expected in ((.028,-.002),(-.028,-.002),(.01,-.008),(-.01,-.008)):
 hit,p,n,index=o.ray_cast(Vector((-abs(y)*slope,y,1)),Vector((0,0,-1)),distance=3)
 assert hit and abs(p.z-expected)<.00001
baseline=json.loads((root/'AtriumCanopyKit/canopy-baseline.json').read_text());selected=[]
for row in baseline['floor']['instances']:
 lo,hi=row['bounds'];matches=[]
 for angle in (0,90,180,270):
  r=math.radians(angle);c=math.cos(r);s=math.sin(r)
  points=[(x*c+y*s,-x*s+y*c) for x in (lo[0],hi[0]) for y in (lo[1],hi[1])]
  if all(1774-.001<=x<=2826+.001 and -300-.001<=y<=300+.001 for x,y in points):matches.append(angle)
 if matches:
  assert len(matches)==1;selected.append(dict(row,bridge_angle=matches[0]))
assert len(selected)==24 and all(sum(r['bridge_angle']==a for r in selected)==6 for a in (0,90,180,270))
indices={r['index'] for r in selected};retained=[r['transform'] for r in baseline['floor']['instances'] if r['index'] not in indices]
assert len(retained)==132
(source/'floor-fit.json').write_text(json.dumps(dict(source_baseline='AtriumCanopyKit/canopy-baseline.json',selected=selected,retained_transforms=retained),indent=2))
(source/'surface-verification.json').write_text(json.dumps(dict(status='passed',reference_joint_checks=joint_checks,surface_samples=samples,inlay_samples=4,all_vertices_inside_fitted_chord_ends=True,selected_instances=24,retained_instances=132,qualification='Source geometry only; in-engine placement and live traversal pending'),indent=2))
print('PASS: bridge surface, chord envelope, inlay depth and 24-instance ownership')
