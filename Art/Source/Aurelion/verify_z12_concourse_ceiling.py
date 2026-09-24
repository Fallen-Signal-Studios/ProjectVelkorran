"""Fresh Blender import of both six-metre ceiling FBXs."""
from pathlib import Path
import json
import bpy

root=Path(__file__).resolve().parent/'Z12ConcourseCeiling'
manifest=json.loads((root/'manifest.json').read_text())
assert manifest['layout']==[7,4] and manifest['room_dimensions_m']==[42,24]
rows=[]
for spec in manifest['modules']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path=root/(spec['asset']+'.fbx')
    assert path.is_file(),path
    bpy.ops.import_scene.fbx(filepath=str(path),use_custom_normals=True)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    assert len(meshes)==1 and meshes[0].name==spec['asset'],meshes
    mesh=meshes[0]
    mesh.data.calc_loop_triangles()
    assert len(mesh.data.uv_layers)==2
    assert len(mesh.data.loop_triangles)==spec['triangles']
    assert set(m.name for m in mesh.data.materials)==set(spec['materials'])
    assert all(abs(a-b)<.01 for a,b in zip(mesh.dimensions,spec['nominal_dimensions_m']))
    assert min(v.co.z for v in mesh.data.vertices)>=-.01
    assert spec['convex_hulls']==0
    rows.append(dict(asset=spec['asset'],triangles=spec['triangles'],
                     dimensions_m=[round(v,5) for v in mesh.dimensions],
                     uv_channels=len(mesh.data.uv_layers),materials=spec['materials'],
                     bytes=path.stat().st_size))
assert len(rows)==2 and rows[0]['triangles']>10000 and rows[1]['triangles']>10000
(root/'verification.json').write_text(json.dumps(dict(status='round_trip_pass',modules=rows),indent=2))
print('Z12_CONCOURSE_CEILING_ROUNDTRIP_PASS')
