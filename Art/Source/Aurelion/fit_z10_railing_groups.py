"""Consolidate only touching rectangular rail envelopes with matching longitudinal axes."""
from pathlib import Path
import json,math
root=Path(__file__).resolve().parent/'Z10RailingKit'
old=json.loads((root/'rail-baseline.json').read_text(encoding='utf-8-sig'))
groups={}
def rotate(q,v):
    x,y,z,w=q;a,b,c=v
    tx,ty,tz=2*(y*c-z*b),2*(z*a-x*c),2*(x*b-y*a)
    return [a+w*tx+y*tz-z*ty,b+w*ty+z*tx-x*tz,c+w*tz+x*ty-y*tx]
for row in old['instances']:
    q=row['quaternion'];q=[v*(-1 if q[3]<0 else 1) for v in q]
    sx,sy,sz=row['scale'];o=old['mesh_origin'];e=old['mesh_extent']
    local=rotate([-q[0],-q[1],-q[2],q[3]],row['location'])
    lx,ly,lz=[v+p*s for v,p,s in zip(local,o,(sx,sy,sz))]
    row=dict(row,x=lx,y=ly,z=lz,quaternion=q,extent=[e[0]*sx,e[1]*sy,e[2]*sz])
    groups.setdefault((tuple(round(v,5) for v in q),round(lx,1),round(e[0]*sx,3)),[]).append(row)
placements=[];types={}
for rows in groups.values():
    pending=list(rows)
    while pending:
        cluster=[pending.pop(0)]
        changed=True
        while changed:
            changed=False
            for r in list(pending):
                if any(abs(r['y']-s['y'])<=r['extent'][1]+s['extent'][1]+.02 and abs(r['z']-s['z'])<=r['extent'][2]+s['extent'][2]+.02 for s in cluster):
                    cluster.append(r);pending.remove(r);changed=True
        lows=[min(r[k]-r['extent'][i] for r in cluster) for i,k in enumerate(('x','y','z'))]
        highs=[max(r[k]+r['extent'][i] for r in cluster) for i,k in enumerate(('x','y','z'))]
        # Do not fill holes in a group whose bounding rectangle contains open space.
        ys=sorted({r['y']+s*r['extent'][1] for r in cluster for s in (-1,1)})
        zs=sorted({r['z']+s*r['extent'][2] for r in cluster for s in (-1,1)})
        for ya,yb in zip(ys,ys[1:]):
            for za,zb in zip(zs,zs[1:]):
                if yb-ya<.03 or zb-za<.03:continue
                assert any(abs((ya+yb)/2-r['y'])<=r['extent'][1]+.02 and abs((za+zb)/2-r['z'])<=r['extent'][2]+.02 for r in cluster), 'Nonrectangular rail group'
        dims=tuple(round((hi-lo)/100,4) for lo,hi in zip(lows,highs))
        name=types.setdefault(dims,'SM_Aurelion_KIT_Z10Guardrail_'+str(len(types)+1))
        q=cluster[0]['quaternion'];x=(lows[0]+highs[0])/2;y=(lows[1]+highs[1])/2
        placements.append(dict(asset=name,dimensions_m=dims,original_indices=sorted(r['index'] for r in cluster),location=rotate(q,[x,y,lows[2]]),quaternion=q))
assert sorted(i for p in placements for i in p['original_indices'])==list(range(192))
result=dict(original_instances=192,placements=placements,modules=[dict(asset=n,dimensions_m=d) for d,n in types.items()])
(root/'rail-fit.json').write_text(json.dumps(result,indent=2))
print(json.dumps(dict(original=192,replacements=len(placements),modules=result['modules'],group_sizes=sorted({len(p['original_indices']) for p in placements})),indent=2))
