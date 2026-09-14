"""Inspect full assembly footprints, including radial-bridge corner intersections."""
import json,math
from pathlib import Path
root=Path(__file__).resolve().parent/'AtriumParapetKit';fit=json.loads((root/'guard-fit.json').read_text())
profiles={p['asset']:p['values'] for p in fit['profiles']}
def cross(a,b):return a[0]*b[1]-a[1]*b[0]
def area(poly):return abs(sum(cross(p,q) for p,q in zip(poly,poly[1:]+poly[:1])))/2
def intersect(poly,clip):
 for a,b in zip(clip,clip[1:]+clip[:1]):
  result=[];v=(b[0]-a[0],b[1]-a[1])
  for p,q in zip(poly,poly[1:]+poly[:1]):
   dp=cross(v,(p[0]-a[0],p[1]-a[1]));dq=cross(v,(q[0]-a[0],q[1]-a[1]))
   if dp>=0:result.append(p)
   if (dp<0)!=(dq<0):
    t=dp/(dp-dq);result.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
  poly=result
  if not poly:break
 return poly
polygons=[]
for p in fit['placements']:
 length,la,lb,ra,rb=profiles[p['mesh']];points=[(la-lb*.11,-.11),(ra-rb*.11,-.11),(ra+rb*.11,.11),(la+lb*.11,.11)]
 a=math.radians(p['yaw']);c=math.cos(a);s=math.sin(a)
 # Reverse handedness from Blender-local Y to Unreal-local Y, then restore winding.
 poly=[(p['position'][0]+100*(x*c+y*s),p['position'][1]+100*(x*s-y*c)) for x,y in points][::-1]
 polygons.append((p['actor'],poly))
overlaps=[]
for i,(name,a) in enumerate(polygons):
 for other,b in polygons[i+1:]:
  overlap=intersect(a,b)
  if overlap and area(overlap)>1:overlaps.append(dict(a=name,b=other,area_cm2=area(overlap)))
(root/'joint-verification.json').write_text(json.dumps(dict(status='PASS' if not overlaps else 'CORRECTION_REQUIRED',overlaps=overlaps,scope='Unbeveled footprint intersections over 1 square centimetre; not gameplay acceptance'),indent=2))
print('ATRIUM_PARAPET_JOINTS',len(overlaps),overlaps[:20])
