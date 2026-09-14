"""Derive modular miter profiles and explicit old-railing ownership from the saved survey."""
import json,math,re,sys
from pathlib import Path
root=Path(__file__).resolve().parent/'AtriumParapetKit';root.mkdir(exist_ok=True)
survey=json.loads(Path(sys.argv[1]).read_text())
def vector(text):return [float(v) for v in re.findall(r'[XYZ]=(-?[\d.]+)',text)]
def pose(text):
 q=[float(v) for v in re.search(r'Rotation=\((.*?)\)',text)[1].replace('X=','').replace('Y=','').replace('Z=','').replace('W=','').split(',')]
 p=vector(re.search(r'Translation=\((.*?)\)',text)[1]);s=vector(re.search(r'Scale3D=\((.*?)\)',text)[1])
 return p,s,math.atan2(2*(q[3]*q[2]+q[0]*q[1]),1-2*(q[1]**2+q[2]**2))
guards=[r for r in survey['components'] if re.match(r'^Z05_.*Guard',r['actor'])]
assert len(guards)==266
groups={}
for g in guards:
 p,s,a=pose(g['transform']);g.update(position=[p[0],p[1],0],yaw=math.degrees(a),length_m=s[0],tangent=[math.cos(a),math.sin(a)])
 key=g['actor'].rsplit('_',1)[0];groups.setdefault(key,[]).append(g)
def joint(current,other):
 p=current['position'];q=other['position'];t=current['tangent'];v=other['tangent'];n=[-t[1],t[0]]
 cross=lambda a,b:a[0]*b[1]-a[1]*b[0]
 distance=cross([q[0]-p[0],q[1]-p[1]],v)/cross(t,v)
 normal=[t[0]+v[0],t[1]+v[1]]
 slope=-sum(normal[i]*n[i] for i in range(2))/sum(normal[i]*t[i] for i in range(2))
 return distance/100,-slope # Blender Y mirrors the Unreal local Y axis.
profiles=[];placements=[]
for key,run in groups.items():
 run.sort(key=lambda g:int(g['actor'].rsplit('_',1)[1]))
 for i,g in enumerate(run):
  left=[-g['length_m']/2,0];right=[g['length_m']/2,0]
  if 'Bridge_' not in key:
   neighbors=[]
   if i:neighbors.append(run[i-1])
   if i<len(run)-1:neighbors.append(run[i+1])
   if key=='Z05_Control_Platform_InnerGuard_0':neighbors.append(run[-1] if i==0 else run[0] if i==len(run)-1 else run[i-1])
   for neighbor in neighbors:
    a,b=joint(g,neighbor)
    assert abs(a)<=g['length_m']/2+.001
    if a<0:left=[a,b]
    else:right=[a,b]
  values=[g['length_m'],*left,*right]
  found=next((p for p in profiles if all(abs(a-b)<.0002 for a,b in zip(p['values'],values))),None)
  if found is None:
   found=dict(asset=f'SM_Aurelion_KIT_AtriumParapet_{len(profiles):02}',values=values);profiles.append(found)
  placements.append(dict(actor=g['actor'],position=g['position'],yaw=g['yaw'],mesh=found['asset']))
rail=next(r for r in survey['components'] if r['actor']=='Aurelion_Art_M12_Z05_51_94dfdd')
selected=[];assignments={g['actor']:[] for g in guards}
for item in rail['instances']:
 matches=[]
 for g in guards:
  o=vector(g['origin']);e=vector(g['extent'])
  if all(item['bounds'][0][i]>=o[i]-e[i]-.02 and item['bounds'][1][i]<=o[i]+e[i]+.02 for i in range(3)):matches.append(g['actor'])
 if matches:
  assert len(matches)==1,(item['index'],matches)
  selected.append(item);assignments[matches[0]].append(item['index'])
assert len(selected)==564,len(selected)
for name,indices in assignments.items():assert len(indices)==(6 if 'Bridge_' in name else 2),(name,indices)
(root/'guard-fit.json').write_text(json.dumps(dict(baseline=guards,profiles=profiles,placements=placements,railing_actor=rail['actor'],railing_component=rail['component'],railing_mesh=rail['mesh'],removed_instances=selected,assignments=assignments),indent=2))
print('ATRIUM_PARAPET_RECIPE',len(placements),'placements',len(profiles),'profiles',len(selected),'owned railing instances')
