"""Round-trip both owned Z08 FBX meshes in a fresh Blender process."""
import bpy
import json
from pathlib import Path

root = Path(__file__).resolve().parent/'Z08ContainmentPylon'
specs = json.loads((root/'manifest.json').read_text())['modules']
rows = []
for spec in specs:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source = root/(spec['asset']+'.fbx')
    assert source.is_file(), source
    bpy.ops.import_scene.fbx(filepath=str(source), use_custom_normals=True)
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    render = [o for o in meshes if not o.name.startswith('UCX_')]
    collision = [o for o in meshes if o.name.startswith('UCX_')]
    assert len(render) == 1 and render[0].name == spec['asset']
    assert len(collision) == spec['convex_hulls']
    o = render[0]
    assert len(o.data.uv_layers) == 2
    assert set(m.name for m in o.data.materials) == set(spec['materials'])
    assert all(0.001 < d < 10 for d in o.dimensions)
    assert all(abs(actual-expected)<.01 for actual,expected in
               zip(o.dimensions,spec['nominal_dimensions_m']))
    assert min(v.co.z for v in o.data.vertices) >= -.1
    o.data.calc_loop_triangles()
    assert len(o.data.loop_triangles) == spec['triangles']
    rows.append(dict(asset=spec['asset'], dimensions_m=[round(v,5) for v in o.dimensions],
                     triangles=len(o.data.loop_triangles), uv_layers=len(o.data.uv_layers),
                     convex_hulls=len(collision), materials=[m.name for m in o.data.materials],
                     bytes=source.stat().st_size))
(root/'verification.json').write_text(json.dumps(dict(status='round_trip_pass',meshes=rows),indent=2))
print('Z08_CONTAINMENT_PYLON_ROUNDTRIP_PASS')
