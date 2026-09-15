"""Audit assembled rail corners for coplanar overlap, without saving an assembly."""
from pathlib import Path
from collections import defaultdict
import bpy,json,math
root=Path(__file__).resolve().parent;kit=root/'Z04RailKit'
bpy.ops.wm.open_mainfile(filepath=str(kit/'Aurelion-Z04-Rails.blend'))
manifest=json.loads((kit/'manifest.json').read_text());assembled=[]
for row in manifest['placements']:
    original=bpy.data.objects[row['asset']]
    o=original.copy();o.data=original.data.copy();bpy.context.scene.collection.objects.link(o)
    x,y,z=row['location_cm'];o.location=((x-9000)/100,-(y+10000)/100,z/100)
    o.rotation_euler=(0,0,-math.radians(row['yaw']));o.scale=(1,1,1);o.hide_set(False);assembled.append(o)
bpy.ops.object.select_all(action='DESELECT')
for o in assembled:o.select_set(True)
bpy.context.view_layer.objects.active=assembled[0];bpy.ops.object.join()
o=bpy.context.object;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
output_name='../Z04RailKit/assembly-coplanar.json'
code=(root/'audit_climb_coplanar_faces.py').read_text().split('o=bpy.data.objects[mesh_name];',1)[1]
exec(compile(code,'assembled_rail_coplanar','exec'),globals())
assert not rows, rows[:5]
