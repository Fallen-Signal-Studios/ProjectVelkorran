"""Ray-test the exported assembly against all original surface planes."""
from pathlib import Path
import bpy,json,hashlib
from mathutils import Vector,Quaternion
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parent/'Z08DeckKit';spec=json.loads((root/'manifest.json').read_text());filename=root/'SM_Aurelion_KIT_Z08DeckAssembly.fbx'
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(filename))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH');verts=[o.matrix_world@v.co for v in o.data.vertices];tree=BVHTree.FromPolygons(verts,[tuple(p.vertices) for p in o.data.polygons])
anchor=Vector(spec['anchor_world_m']);results=[]
for row in spec['placements']:
    q=row['quaternion'];rot=Quaternion((q[3],-q[0],q[1],-q[2]));delta=Vector(row['top_center_world_m'])-anchor;center=Vector((delta.x,-delta.y,delta.z));normal=rot@Vector((0,0,1));w,l,h=row['size_m'];hits=[]
    for u,v in ((.13,.17),(-.33,-.27),(-.33,.27),(.33,-.27),(.33,.27)):
        p=center+rot@Vector((u*w,v*l,0));hit,n,index,distance=tree.ray_cast(p+normal*.05,-normal,.10)
        assert hit is not None,(row['original_index'],u,v,'missing surface')
        deviation=(hit-p).dot(normal);assert -.0061<=deviation<=.0002,(row['original_index'],deviation)
        assert n.dot(normal)>.99,(row['original_index'],'surface normal',n.dot(normal))
        hits.append(dict(surface_offset_m=deviation,normal_dot=n.dot(normal)))
    results.append(dict(original_index=row['original_index'],probes=hits))
assert len(results)==42
(root/'coverage.json').write_text(json.dumps(dict(status='passed',fbx_sha256=hashlib.sha256(filename.read_bytes()).hexdigest(),panels=results,qualification='210 exported surface probes; not live traversal or visual acceptance'),indent=2))
print('DECK_42_PANEL_210_PROBE_PASS')
