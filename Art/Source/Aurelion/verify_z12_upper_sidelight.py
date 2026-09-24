"""Independent Blender FBX round trip for the Z12 upper sidelight kit."""
from pathlib import Path
import json
import math
import bpy

root = Path(__file__).resolve().parent / 'Z12UpperSidelight'
manifest = json.loads((root / 'manifest.json').read_text())
assert manifest['placement']['lower_service_registers_unchanged']
assert manifest['placement']['native_wall_collision_unchanged']

rows = []
for spec in manifest['modules']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = root / (spec['asset'] + '.fbx')
    bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    assert len(meshes) == 1 and meshes[0].name == spec['asset']
    mesh = meshes[0]
    mesh.data.calc_loop_triangles()
    assert len(mesh.data.loop_triangles) == spec['triangles']
    assert len(mesh.data.uv_layers) == spec['uv_layers'] == 2
    assert set(m.name for m in mesh.data.materials) == set(spec['materials'])
    assert all(abs(a-b) < .01 for a,b in zip(mesh.dimensions, spec['nominal_dimensions_m']))
    assert all(math.isfinite(v.co[i]) for v in mesh.data.vertices for i in range(3))
    assert all(p.area > 1e-8 for p in mesh.data.polygons)
    assert spec['convex_hulls'] == 0
    rows.append(dict(asset=spec['asset'], triangles=len(mesh.data.loop_triangles),
                     dimensions_m=[round(v, 5) for v in mesh.dimensions],
                     uv_channels=2, materials=spec['materials'], bytes=path.stat().st_size))
assert len(rows) == 2
(root / 'verification.json').write_text(json.dumps(dict(status='round_trip_pass', modules=rows), indent=2))
print('Z12_UPPER_SIDELIGHT_ROUNDTRIP_PASS')
