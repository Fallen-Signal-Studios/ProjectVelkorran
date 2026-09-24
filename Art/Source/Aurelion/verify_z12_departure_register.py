"""Clean Blender FBX round trip for the rare visual-only Z12 service coffer."""
from pathlib import Path
import json
import math
import bpy

root = Path(__file__).resolve().parent / 'Z12DepartureRegister'
manifest = json.loads((root / 'manifest.json').read_text())
assert len(manifest['modules']) == 1
spec = manifest['modules'][0]
bpy.ops.wm.read_factory_settings(use_empty=True)
fbx = root / (spec['asset'] + '.fbx')
bpy.ops.import_scene.fbx(filepath=str(fbx), use_custom_normals=True)
objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
assert len(objects) == 1 and objects[0].name == spec['asset']
obj = objects[0]
obj.data.calc_loop_triangles()
assert len(obj.data.loop_triangles) == spec['triangles']
assert len(obj.data.uv_layers) == 2
assert set(mat.name for mat in obj.data.materials) == set(spec['materials'])
assert all(abs(actual-expected) < .01 for actual,expected in zip(obj.dimensions, manifest['visual_envelope_m']))
assert all(math.isfinite(v.co[i]) for v in obj.data.vertices for i in range(3))
bad = [(poly.index, poly.area,
        obj.data.materials[poly.material_index].name,
        [[round(obj.data.vertices[i].co[j],5) for j in range(3)] for i in poly.vertices])
       for poly in obj.data.polygons if poly.area <= 1e-8]
assert not bad, (len(bad), bad[:20])
assert spec['convex_hulls'] == 0
report = dict(status='round_trip_pass', asset=spec['asset'],
              triangles=spec['triangles'], dimensions_m=[round(v,5) for v in obj.dimensions],
              uv_channels=2, materials=spec['materials'], convex_hulls=0, fbx_bytes=fbx.stat().st_size)
(root / 'verification.json').write_text(json.dumps(report,indent=2))
print('Z12_SERVICE_REGISTER_ROUNDTRIP_PASS', report)
