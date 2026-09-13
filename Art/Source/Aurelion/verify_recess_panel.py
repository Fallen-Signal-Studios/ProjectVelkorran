"""Round-trip the exported FBX in a clean Blender process; no Unreal writes."""
import bpy
import json
from pathlib import Path

root = Path(__file__).resolve().parent
name = 'SM_Aurelion_RecessPanel_2m'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root / (name + '.fbx')))
objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
assert len(objects) == 2, [o.name for o in objects]
mesh = bpy.data.objects[name]
collision = bpy.data.objects['UCX_' + name + '_00']
for obj in (mesh, collision):
    assert all(abs(a-b) < .001 for a, b in zip(obj.dimensions, (2, .4, 3))), tuple(obj.dimensions)
assert mesh.location.length < .001, tuple(mesh.location)
assert len(mesh.data.uv_layers) >= 1
assert len(mesh.data.materials) == 4
assert len(collision.data.vertices) == 8
mesh.data.calc_loop_triangles()
result = dict(status='PASS', mesh=mesh.name,
              dimensions_metres=list(mesh.dimensions),
              triangles=len(mesh.data.loop_triangles),
              material_slots=len(mesh.data.materials),
              collision_vertices=len(collision.data.vertices),
              limitation='Blender FBX round-trip only; Unreal import and gameplay pending')
(root / (name + '-verification.json')).write_text(json.dumps(result, indent=2), encoding='utf8')
print(json.dumps(result))
