"""Independent Blender FBX round-trip for the two Z11 ceiling variants."""
from pathlib import Path
import json
import math
import bpy

root = Path(__file__).resolve().parent / 'Z11ObservationCeiling'
manifest = json.loads((root/'manifest.json').read_text(encoding='utf-8'))
assert manifest['layout'] == [6,4] and manifest['room_dimensions_m'] == [24,16]
rows = []
for spec in manifest['modules']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = root/(spec['asset']+'.fbx')
    bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    assert len(meshes) == 1 and meshes[0].name == spec['asset'], meshes
    obj = meshes[0]
    obj.data.calc_loop_triangles()
    assert len(obj.data.loop_triangles) == spec['triangles']
    assert len(obj.data.uv_layers) == 2
    assert set(m.name for m in obj.data.materials) == set(spec['materials'])
    assert all(abs(a-b) < .01 for a,b in zip(obj.dimensions, spec['nominal_dimensions_m']))
    assert 3.999 <= obj.dimensions.x <= 4.001 and 3.999 <= obj.dimensions.y <= 4.001
    assert obj.dimensions.z < .42
    assert min(v.co.z for v in obj.data.vertices) >= -.005
    assert max(v.co.z for v in obj.data.vertices) < .42
    assert all(math.isfinite(c) for v in obj.data.vertices for c in v.co)
    assert all(poly.area > 1e-11 for poly in obj.data.polygons), min(p.area for p in obj.data.polygons)
    assert spec['convex_hulls'] == 0
    rows.append(dict(asset=spec['asset'], triangles=spec['triangles'],
                     dimensions_m=[round(v,5) for v in obj.dimensions],
                     uv_channels=2, materials=spec['materials'], bytes=path.stat().st_size))
assert len(rows) == 2
(root/'verification.json').write_text(json.dumps(dict(status='round_trip_pass', modules=rows), indent=2))
print('Z11_OBSERVATION_CEILING_ROUNDTRIP_PASS')
