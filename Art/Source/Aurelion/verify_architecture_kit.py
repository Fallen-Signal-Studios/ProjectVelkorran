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
    assert len(o.data.materials)==len(entry['materials']) and len(o.data.uv_layers)==2
    assert all(abs(a-b)<.05 for a,b in zip(o.dimensions,entry['nominal_dimensions_m'])), (o.name,list(o.dimensions))
    assert all(math.isfinite(v) for vertex in o.data.vertices for v in vertex.co)
    assert all(poly.area>1e-12 for poly in o.data.polygons)
    o.data.calc_loop_triangles()
    assert len(o.data.loop_triangles)==entry['triangles']
    if 'aperture_m' in entry:
        width,height=entry['aperture_m']
        for x in (-width/2+.06,0,width/2-.06):
            for z in (.1,height/2,height-.06):
                assert not o.ray_cast(Vector((x,-5,z)),Vector((0,1,0)),distance=10)[0], 'Visual portal aperture obstructed'
        for x,z in ((-width/2-.5,2),(width/2+.5,2),(0,height+.5)):
            assert o.ray_cast(Vector((x,-5,z)),Vector((0,1,0)),distance=10)[0], 'Visual frame missing'
    if hulls and 'solid_bounds_m' in entry:
        assert len(hulls)==1 and len(hulls[0].data.vertices)==8
        lo,hi=entry['solid_bounds_m']; h=hulls[0]
        coords=[h.matrix_world@v.co for v in h.data.vertices]
        for axis in range(3):
            assert abs(min(v[axis] for v in coords)-lo[axis])<.001
            assert abs(max(v[axis] for v in coords)-hi[axis])<.001
    elif hulls and 'deck_samples' in entry:
        assert all(len(h.data.vertices)==8 for h in hulls)
        for y,z in entry['deck_samples']:
            # Sample 1 mm inside terminal faces to avoid float-rounded FBX boundaries.
            sample_y=min(max(y,entry['deck_samples'][0][0]+.001),entry['deck_samples'][-1][0]-.001)
            for x,expected in ((0,z),(entry['rail_x'],z+entry['rail_height'])):
                hits=[h.ray_cast(Vector((x,sample_y,10)),Vector((0,0,-1)),distance=15) for h in hulls]
                heights=[result[1].z for result in hits if result[0]]
                assert heights and abs(max(heights)-expected)<.016,(x,y,expected,heights)
        center=min(entry['deck_samples'],key=lambda p:abs(p[0]))[1]
        assert not any(h.ray_cast(Vector((-3,0,center+.75)),Vector((1,0,0)),distance=6)[0] for h in hulls), 'Balustrade opening blocked by a solid collider'
    elif hulls and 'furniture_surface_samples' in entry:
        for x,y,z in entry['furniture_surface_samples']:
            hits=[h.ray_cast(Vector((x,y,3)),Vector((0,0,-1)),distance=5) for h in hulls]
            heights=[p.z for hit,p,n,index in hits if hit]
            assert (not heights) if z is None else heights and abs(max(heights)-z)<.001,(x,y,z,heights)
    elif hulls and 'pier_collision_samples' in entry:
        assert all(len(h.data.vertices)==8 and h.location.length<.001 for h in hulls)
        for x,z,expected in entry['pier_collision_samples']:
            blocked=any(h.ray_cast(Vector((x,-5,z)),Vector((0,1,0)),distance=10)[0] for h in hulls)
            assert blocked==expected,(x,z,expected,blocked)
    elif hulls:
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
