"""Independent FBX round trip for the fitted Z11 table and chair visuals."""
from pathlib import Path
import json
import math

import bpy

root = Path(__file__).resolve().parent / 'Z11ObservationFurniture'
specs = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))['modules']
assert {s['asset'] for s in specs} == {
    'SM_Aurelion_KIT_Z11ConversationTable',
    'SM_Aurelion_KIT_Z11ConversationChair',
}
rows = []
for spec in specs:
    assert spec['convex_hulls'] == 0
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
    assert all(abs(a-b) < .01 for a,b in zip(obj.dimensions,spec['nominal_dimensions_m']))
    assert all(math.isfinite(c) for v in obj.data.vertices for c in v.co)
    bad = [i for i,poly in enumerate(obj.data.polygons) if poly.area <= 1e-11]
    assert not bad,(spec['asset'],'zero-area faces',len(bad),
                    [(i,obj.data.polygons[i].material_index,
                      tuple(round(v,5) for v in obj.data.polygons[i].center)) for i in bad[:12]])
    assert -.001 <= min(v.co.z for v in obj.data.vertices) <= .001
    if 'Table' in spec['asset']:
        assert obj.dimensions.x <= 5.001 and obj.dimensions.y <= 2.001
        assert max(v.co.z for v in obj.data.vertices) <= .801
    else:
        assert obj.dimensions.x <= .78 and obj.dimensions.y <= .86
        assert max(v.co.z for v in obj.data.vertices) <= 1.21
    rows.append(dict(asset=spec['asset'],dimensions_m=[round(v,5) for v in obj.dimensions],
                     triangles=spec['triangles'],materials=spec['materials'],
                     uv_channels=2,collision_hulls=0,fbx_bytes=path.stat().st_size))
report = dict(status='round_trip_pass',modules=rows,
              scope='FBX geometry, source envelopes, UVs, material slots and finite positive-area faces')
(root/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('Z11_FURNITURE_ROUNDTRIP_PASS')
