"""Independent FBX round trip and placement-envelope audit for the Z11 bay."""
from pathlib import Path
import json
import math
import bpy

root = Path(__file__).resolve().parent / 'Z11QuietWall'
manifest = json.loads((root/'manifest.json').read_text())
assert manifest['placement']['native_wall_collision_unchanged']
assert manifest['placement']['south_central_doorway_unchanged']
assert manifest['placement']['observation_window_unchanged']
specs = manifest['modules']
assert len(specs) == 1
spec = specs[0]
bpy.ops.wm.read_factory_settings(use_empty=True)
fbx = root/(spec['asset']+'.fbx')
bpy.ops.import_scene.fbx(filepath=str(fbx), use_custom_normals=True)
meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
assert len(meshes) == 1 and meshes[0].name == spec['asset']
obj = meshes[0]
obj.data.calc_loop_triangles()
assert len(obj.data.loop_triangles) == spec['triangles']
assert len(obj.data.uv_layers) == spec['uv_layers'] == 2
assert set(mat.name for mat in obj.data.materials) == set(spec['materials'])
assert all(abs(a-b) < .01 for a,b in zip(obj.dimensions, spec['nominal_dimensions_m']))
assert 4.48 <= obj.dimensions.x <= 4.50
assert obj.dimensions.y <= .5001
assert 5.98 <= obj.dimensions.z <= 6.001
assert all(math.isfinite(v.co[i]) for v in obj.data.vertices for i in range(3))
assert all(poly.area > 1e-11 for poly in obj.data.polygons), min(poly.area for poly in obj.data.polygons)
assert spec['convex_hulls'] == 0
assert max(abs(v.co.x) for v in obj.data.vertices) <= 2.251
assert max(abs(v.co.y) for v in obj.data.vertices) <= .251
assert min(v.co.z for v in obj.data.vertices) >= -.001
assert max(v.co.z for v in obj.data.vertices) <= 6.001
report = dict(status='round_trip_pass', asset=spec['asset'],
              triangles=len(obj.data.loop_triangles),
              dimensions_m=[round(v,5) for v in obj.dimensions],
              uv_channels=2, materials=spec['materials'], bytes=fbx.stat().st_size)
(root/'verification.json').write_text(json.dumps(report,indent=2))
print('Z11_QUIET_WALL_ROUNDTRIP_PASS')
