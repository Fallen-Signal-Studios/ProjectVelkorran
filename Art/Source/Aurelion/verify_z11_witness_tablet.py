"""Independent Blender FBX round trip for the noncolliding Z11 request visual."""
from pathlib import Path
import json
import math

import bpy

root = Path(__file__).resolve().parent / 'Z11WitnessTablet'
manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
assert len(manifest['modules']) == 1
spec = manifest['modules'][0]
assert spec['convex_hulls'] == 0 and spec['protected_request_actor_count'] == 5
bpy.ops.wm.read_factory_settings(use_empty=True)
path = root / (spec['asset'] + '.fbx')
bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
assert len(meshes) == 1 and meshes[0].name == spec['asset']
obj = meshes[0]
obj.data.calc_loop_triangles()
assert len(obj.data.loop_triangles) == spec['triangles']
assert len(obj.data.uv_layers) == 2
assert set(m.name for m in obj.data.materials) == set(spec['materials'])
print('Z11_WITNESS_TABLET_IMPORTED_DIMENSIONS', tuple(round(v,5) for v in obj.dimensions))
assert all(abs(a-b) < .01 for a,b in zip(obj.dimensions,spec['nominal_dimensions_m']))
assert obj.dimensions.x * spec['original_actor_visual_scale'] < .7
assert obj.dimensions.y * spec['original_actor_visual_scale'] < .5
assert 1.54 < obj.dimensions.z < 1.57
assert -.005 <= min(v.co.z for v in obj.data.vertices) <= .005
assert max(v.co.z for v in obj.data.vertices) < 1.57
assert all(math.isfinite(c) for v in obj.data.vertices for c in v.co)
assert all(poly.area > 1e-11 for poly in obj.data.polygons)
report = dict(status='round_trip_pass', asset=spec['asset'],
              triangles=spec['triangles'], uv_channels=2,
              materials=spec['materials'],
              dimensions_m=[round(v,5) for v in obj.dimensions],
              collision_hulls=0, fbx_bytes=path.stat().st_size)
(root / 'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('Z11_WITNESS_TABLET_ROUNDTRIP_PASS')
