"""Clean-process FBX round-trip checks; not visual or runtime acceptance."""
import bpy
import json
import math
import sys
from mathutils import Vector
from pathlib import Path
root=Path(sys.argv[sys.argv.index('--')+1]).resolve() if '--' in sys.argv else Path(__file__).resolve().parent/'ArchitectureKit'
manifest=json.loads((root/'manifest.json').read_text())
rows=[]
for entry in manifest['modules']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root/(entry['asset']+'.fbx')))
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
    hulls=[o for o in objects if o.name.startswith('UCX_')]
    assert len(hulls)==entry.get('convex_hulls',0)
    visible=[o for o in objects if not o.name.startswith('UCX_')]
    assert len(visible)==1
    o=visible[0]
    assert o.name==entry['asset'] and o.location.length<.001
    assert len(o.data.materials)==3 and len(o.data.uv_layers)==2
    assert all(abs(a-b)<.05 for a,b in zip(o.dimensions,entry['nominal_dimensions_m'])), (o.name,list(o.dimensions))
    assert all(math.isfinite(v) for vertex in o.data.vertices for v in vertex.co)
    assert all(poly.area>1e-12 for poly in o.data.polygons)
    o.data.calc_loop_triangles()
    assert len(o.data.loop_triangles)==entry['triangles']
    if hulls:
        assert all(len(h.data.vertices)==8 for h in hulls)
        for x in (-2.9,0,2.9):
            for z in (.5,2.5,4.5):
                assert not any(h.ray_cast(Vector((x,-5,z)),Vector((0,1,0)),distance=10)[0] for h in hulls), 'Portal aperture obstructed'
        for x in (-3.5,3.5):
            assert any(h.ray_cast(Vector((x,-5,2.5)),Vector((0,1,0)),distance=10)[0] for h in hulls), 'Portal side has no collision'
    rows.append(dict(asset=o.name,dimensions_m=list(o.dimensions),triangles=len(o.data.loop_triangles)))
(root/'verification.json').write_text(json.dumps(dict(status='PASS',modules=rows,
    scope='FBX geometry, nominal bounds, origin, UV presence, material slots and finite coordinates only'),indent=2))
print('ARCHITECTURE_KIT_FBX_ROUNDTRIP_PASS')
