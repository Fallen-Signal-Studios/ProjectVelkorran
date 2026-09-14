"""Retain the existing upper enclosure while custom modules replace its lower walls.

Input: FBX produced by export_z01_enclosure.py. Does not overwrite the original asset.
"""
import bpy
import bmesh
import json
from pathlib import Path
import sys
from mathutils import Vector
source=Path(sys.argv[sys.argv.index('--')+1]).resolve()
out=Path(__file__).resolve().parent/'Z01Upper'; out.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source))
objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_')]
assert len(objects)==1,[o.name for o in objects]
o=objects[0]; bpy.context.view_layer.objects.active=o
bpy.ops.object.select_all(action='DESELECT'); o.select_set(True)
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
assert all(abs(a-b)<.03 for a,b in zip(o.dimensions,(6,14,4.120843))),list(o.dimensions)
assert len(o.data.materials)==4
for i,m in enumerate(o.data.materials): m.name='M_Z01_Upper_'+str(i)
cut=(695+7.86791)/300 # World Z695 cm, existing actor Z=-7.86791 cm and scale Z=3.
bm=bmesh.new(); bm.from_mesh(o.data)
before=len(bm.faces)
bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
    dist=.00001,plane_co=Vector((0,0,cut)),plane_no=Vector((0,0,1)),clear_inner=True,clear_outer=False)
bm.to_mesh(o.data); bm.free(); o.data.update()
assert len(o.data.polygons)>0 and len(o.data.polygons)<before
assert min(v.co.z for v in o.data.vertices)>=cut-.0001
o.name='SM_Aurelion_Z01_UpperEnclosure'
scene=bpy.context.scene; scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.export_scene.fbx(filepath=str(out/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},
    axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(out/(o.name+'.blend')))
o.data.calc_loop_triangles()
(out/'derivation.json').write_text(json.dumps(dict(source_asset='/Game/Aurelion/Art/Props/aurelionwalls/StaticMeshes/aurelionwalls',
    operation='Remove geometry below world Z695; retain upper enclosure at original actor transform',
    cut_local_metres=cut,faces_before=before,faces_after=len(o.data.polygons),triangles=len(o.data.loop_triangles),
    dimensions_metres=list(o.dimensions),material_slots=4,uv_channels=len(o.data.uv_layers),
    scope='Owned visual derivative; original collision asset remains unchanged'),indent=2))
print('Z01_UPPER_DERIVATION_COMPLETE')
