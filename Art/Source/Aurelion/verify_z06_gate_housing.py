"""Check source housing solids remain outside the entire moving leaf volume."""
import bpy,json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z06GateHousingKit';spec=json.loads((root/'manifest.json').read_text())['modules'][0]
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(root/(spec['asset']+'.fbx')))
lo,hi=spec['moving_leaf_swept_bounds_m'];objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];rows=[]
for o in objects:
    groups=[list(o.data.vertices)] if o.name.startswith('UCX_') else [[o.data.vertices[i] for i in p.vertices] for p in o.data.polygons]
    margins=[]
    for group in groups:
        points=[o.matrix_world@v.co for v in group]
        separation=max(max(min(p[k] for p in points)-hi[k],lo[k]-max(p[k] for p in points)) for k in range(3))
        assert separation>=.0499,(o.name,separation)
        margins.append(separation)
    rows.append(dict(object=o.name,checked_groups=len(groups),minimum_separation_m=min(margins)))
visual=next(o for o in objects if not o.name.startswith('UCX_'));aperture=[]
for y in (-2.1,-1,0,1,2.1):
    for z in (.1,1.5,3.0):
        assert not visual.ray_cast(Vector((-1,y,z)),Vector((1,0,0)),distance=2)[0]
        aperture.append([y,z])
for y in (-1,0,1):assert visual.ray_cast(Vector((-1,y,4.5)),Vector((1,0,0)),distance=2)[0]
(root/'travel-clearance.json').write_text(json.dumps(dict(status='passed',objects=rows,aperture_rays=aperture,qualification='Conservative polygon/hull bounds separation from swept leaf volume plus source aperture rays; Unreal scene fit and native transit remain pending.'),indent=2))
print('Z06_GATE_HOUSING_CLEARANCE_PASS')
