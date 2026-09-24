"""Import the Z12 frame and pane FBX independently to catch export defects."""
import bpy
import json
from pathlib import Path

root=Path(__file__).resolve().parent/'Z12ArmoredView'
specs=json.loads((root/'manifest.json').read_text())['modules']
assert len(specs)==3
rows=[]
for spec in specs:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source=root/(spec['asset']+'.fbx')
    assert source.is_file(),source
    bpy.ops.import_scene.fbx(filepath=str(source),use_custom_normals=True)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    assert len(meshes)==1 and meshes[0].name==spec['asset'],meshes
    mesh=meshes[0]
    assert len(mesh.data.uv_layers)==2
    assert set(m.name for m in mesh.data.materials)==set(spec['materials'])
    assert all(abs(a-b)<.01 for a,b in zip(mesh.dimensions,spec['nominal_dimensions_m']))
    assert min(v.co.z for v in mesh.data.vertices)>=-.01
    assert spec['convex_hulls']==0
    mesh.data.calc_loop_triangles()
    assert len(mesh.data.loop_triangles)==spec['triangles']
    rows.append(dict(asset=spec['asset'],triangles=len(mesh.data.loop_triangles),
                     dimensions_m=[round(v,5) for v in mesh.dimensions],
                     materials=[m.name for m in mesh.data.materials],
                     uv_channels=len(mesh.data.uv_layers),bytes=source.stat().st_size))
assert rows[0]['triangles']>rows[1]['triangles'] and rows[2]['triangles']>rows[1]['triangles']
(root/'verification.json').write_text(json.dumps(dict(status='round_trip_pass',meshes=rows),indent=2))
print('Z12_ARMORED_VIEW_ROUNDTRIP_PASS')
