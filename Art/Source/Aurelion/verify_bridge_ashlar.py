"""Clean FBX rays distinguish dressed stone faces from recessed mortar joints."""
import bpy,json,hashlib
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent;rows=[]
baseline=json.loads((root/'NorthBridgeKit/ashlar-collision-baseline.json').read_text())
for folder,name in [('ApproachBridgeKit','ApproachArchSupports'),('NorthBridgeKit/NorthSpanA','NorthSpanAArchSupports'),('NorthBridgeKit/NorthSpanB','NorthSpanBArchSupports')]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root/folder/('SM_Aurelion_KIT_'+name+'.fbx')))
    hulls=sorted([sorted([tuple(round(v,5) for v in vertex.co) for vertex in o.data.vertices]) for o in bpy.context.scene.objects if o.name.startswith('UCX_')])
    assert len(hulls)==180 and hashlib.sha256(json.dumps(hulls,sort_keys=True).encode()).hexdigest()==baseline['hull_signatures'][name], 'Authored collision changed'
    o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_'))
    for z,expected,material in [(-12.39,7.02,'M_Aurelion_IvoryStone'),(-11.64,6.97,'M_Aurelion_StoneGrout')]:
        hit,point,normal,index=o.ray_cast(Vector((9,8.45,z)),Vector((-1,0,0)))
        assert hit and abs(point.x-expected)<.002,(name,z,point[:],expected)
        actual=o.data.materials[o.data.polygons[index].material_index].name
        assert actual==material,(name,z,actual,material)
        rows.append(dict(mesh=name,z=z,x=point.x,material=actual))
(root/'NorthBridgeKit/ashlar-surface-verification.json').write_text(json.dumps(dict(status='PASS',samples=rows,scope='Stone and mortar depth/material ray checks; visual and live acceptance separate.'),indent=2))
print('BRIDGE_ASHLAR_SURFACES_PASS')
