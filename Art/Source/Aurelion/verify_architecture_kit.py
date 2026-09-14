"""Clean-process FBX round-trip checks; not visual or runtime acceptance."""
import bpy
import json
import math
from pathlib import Path
root=Path(__file__).resolve().parent/'ArchitectureKit'
manifest=json.loads((root/'manifest.json').read_text())
rows=[]
for entry in manifest['modules']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root/(entry['asset']+'.fbx')))
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
    assert len(objects)==1
    o=objects[0]
    assert o.name==entry['asset'] and o.location.length<.001
    assert len(o.data.materials)==3 and len(o.data.uv_layers)==2
    assert all(abs(a-b)<.05 for a,b in zip(o.dimensions,entry['nominal_dimensions_m'])), (o.name,list(o.dimensions))
    assert all(math.isfinite(v) for vertex in o.data.vertices for v in vertex.co)
    assert all(poly.area>1e-12 for poly in o.data.polygons)
    o.data.calc_loop_triangles()
    assert len(o.data.loop_triangles)==entry['triangles']
    rows.append(dict(asset=o.name,dimensions_m=list(o.dimensions),triangles=len(o.data.loop_triangles)))
(root/'verification.json').write_text(json.dumps(dict(status='PASS',modules=rows,
    scope='FBX geometry, nominal bounds, origin, UV presence, material slots and finite coordinates only'),indent=2))
print('ARCHITECTURE_KIT_FBX_ROUNDTRIP_PASS')
