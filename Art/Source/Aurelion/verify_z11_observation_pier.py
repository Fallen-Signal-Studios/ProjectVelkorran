"""Independent Blender FBX round-trip for the visual Z11 pier cladding."""
from pathlib import Path
import json
import math

import bpy

root = Path(__file__).resolve().parent/'Z11ObservationPier'
manifest = json.loads((root/'manifest.json').read_text(encoding='utf-8'))
assert len(manifest['modules']) == 1
spec = manifest['modules'][0]
assert spec['convex_hulls'] == 0
assert spec['measured_native_envelope_m'] == [1.0,1.2,6.0]
bpy.ops.wm.read_factory_settings(use_empty=True)
path = root/(spec['asset']+'.fbx')
bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
assert len(meshes) == 1 and meshes[0].name == spec['asset']
obj = meshes[0]
obj.data.calc_loop_triangles()
assert len(obj.data.loop_triangles) == spec['triangles']
assert len(obj.data.uv_layers) == 2
assert set(m.name for m in obj.data.materials) == set(spec['materials'])
assert all(abs(a-b) < .01 for a,b in zip(obj.dimensions,spec['nominal_dimensions_m']))
assert 1.0 < obj.dimensions.x <= spec['authored_max_footprint_m'][0]
assert 1.2 < obj.dimensions.y <= spec['authored_max_footprint_m'][1]
assert 5.9 < obj.dimensions.z < 6.0
assert -.005 <= min(v.co.z for v in obj.data.vertices) <= .005
assert max(v.co.z for v in obj.data.vertices) < 6.0
assert all(math.isfinite(c) for v in obj.data.vertices for c in v.co)
assert all(p.area > 1e-11 for p in obj.data.polygons)
report = dict(status='round_trip_pass',asset=spec['asset'],
              triangles=spec['triangles'],uv_channels=2,materials=spec['materials'],
              dimensions_m=[round(v,5) for v in obj.dimensions],
              collision_hulls=0,fbx_bytes=path.stat().st_size)
(root/'verification.json').write_text(json.dumps(report,indent=2))
print('Z11_OBSERVATION_PIER_ROUNDTRIP_PASS')
