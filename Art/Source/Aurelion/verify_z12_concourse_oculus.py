"""Clean Blender 4.5 round trip of the visual-only Z12 oculus roof tile."""
from pathlib import Path
import json
import math
import bpy

root=Path(__file__).resolve().parent/'Z12ConcourseOculus'
manifest=json.loads((root/'manifest.json').read_text())
spec=manifest['module']
fbx=root/(spec['asset']+'.fbx')
assert fbx.is_file()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(fbx),use_custom_normals=True)
meshes=[obj for obj in bpy.context.scene.objects if obj.type=='MESH']
assert len(meshes)==1 and meshes[0].name==spec['asset'],meshes
obj=meshes[0]
obj.data.calc_loop_triangles()
assert len(obj.data.loop_triangles)==spec['triangles'] and 10000<=spec['triangles']<=100000
assert len(obj.data.uv_layers)==2
assert set(mat.name for mat in obj.data.materials)==set(spec['materials'])
assert all(abs(a-b)<.02 for a,b in zip(obj.dimensions,spec['nominal_dimensions_m']))
assert 5.95<=obj.dimensions.x<=6.05 and 5.95<=obj.dimensions.y<=6.05
assert .7<=obj.dimensions.z<=.85
assert all(math.isfinite(v.co[i]) for v in obj.data.vertices for i in range(3))
degenerate=[poly.index for poly in obj.data.polygons if poly.area<=1e-8]
assert not degenerate,degenerate[:20]
assert spec['convex_hulls']==0
report=dict(status='round_trip_pass',asset=spec['asset'],triangles=spec['triangles'],
            dimensions_m=[round(v,5) for v in obj.dimensions],
            min_z_m=round(min(v.co.z for v in obj.data.vertices),5),
            max_z_m=round(max(v.co.z for v in obj.data.vertices),5),
            uv_channels=2,materials=spec['materials'],convex_hulls=0,
            degenerate_polygons=0,fbx_bytes=fbx.stat().st_size)
(root/'verification.json').write_text(json.dumps(report,indent=2))
print('Z12_CONCOURSE_OCULUS_ROUNDTRIP_PASS',report)
