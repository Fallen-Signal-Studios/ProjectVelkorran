"""Measure same-facing, axis-aligned coplanar face overlaps in the climb source."""
from pathlib import Path
from collections import defaultdict
import bpy,json,sys
root=Path(__file__).resolve().parent
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
blend=root/args[0] if args else root/'Z06ClimbKit/Aurelion-ClimbPanel.blend'
mesh_name=args[1] if args else 'SM_Aurelion_KIT_Z06ClimbPanel'
output_name=args[2] if args else 'climb-coplanar-faces.json'
bpy.ops.wm.open_mainfile(filepath=str(blend))
o=bpy.data.objects[mesh_name];groups=defaultdict(list)
def area(points):return sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(points,points[1:]+points[:1]))/2
def cross(a,b,p):return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
def clip(subject,clipper):
    for a,b in zip(clipper,clipper[1:]+clipper[:1]):
        result=[]
        if not subject:return []
        prev=subject[-1];dp=cross(a,b,prev)
        for p in subject:
            d=cross(a,b,p)
            if (d>=0)!=(dp>=0):
                t=dp/(dp-d);result.append((prev[0]+t*(p[0]-prev[0]),prev[1]+t*(p[1]-prev[1])))
            if d>=0:result.append(p)
            prev=p;dp=d
        subject=result
    return subject
for face in o.data.polygons:
    axis=max(range(3),key=lambda i:abs(face.normal[i]))
    if abs(face.normal[axis])<.999999:continue
    xyz=[o.data.vertices[i].co for i in face.vertices];plane=sum(v[axis] for v in xyz)/len(xyz)
    if max(abs(v[axis]-plane) for v in xyz)>1e-6:continue
    axes=[i for i in range(3) if i!=axis];points=[tuple(v[i] for i in axes) for v in xyz]
    if area(points)<0:points.reverse()
    groups[(axis,round(plane,6),1 if face.normal[axis]>0 else -1)].append((face.index,points))
rows=[]
for key,faces in groups.items():
    for i,(idx,a) in enumerate(faces):
        for jdx,b in faces[i+1:]:
            if any(max(p[k] for p in a)<=min(p[k] for p in b) or max(p[k] for p in b)<=min(p[k] for p in a) for k in (0,1)):continue
            polygon=clip(a,b);overlap=abs(area(polygon)) if polygon else 0
            if overlap<1e-8:continue
            rows.append(dict(axis=key[0],plane_m=key[1],normal_sign=key[2],faces=[idx,jdx],materials=[o.data.materials[o.data.polygons[k].material_index].name for k in (idx,jdx)],overlap_m2=overlap,projected_centroid=[sum(p[k] for p in polygon)/len(polygon) for k in (0,1)]))
out=root/'Z06SurfaceReview';out.mkdir(exist_ok=True)
(out/output_name).write_text(json.dumps(dict(status='measured',overlaps=rows,scope='Same-facing axis-aligned convex source faces within one micrometre; visibility and non-axis overlaps are not inferred.'),indent=2))
print('COPLANAR_FACE_OVERLAPS',len(rows))
