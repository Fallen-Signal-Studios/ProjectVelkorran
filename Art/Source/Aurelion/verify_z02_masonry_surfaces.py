"""Confirm visible limestone remains fixed while joint backing is recessed."""
import bpy,json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z02PerimeterKit'
fit=json.loads((root/'perimeter-fit.json').read_text());rows=[]
for suffix,width in [('4m',4),('Closing',fit['wall_length_m']-16)]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    name='SM_Aurelion_KIT_Z02Perimeter_'+suffix;bpy.ops.import_scene.fbx(filepath=str(root/(name+'.fbx')))
    o=bpy.data.objects[name];z=2.5*fit['wall_height_m']/14
    for x,expected_y,material in [(0,-.005,'M_Aurelion_IvoryStone'),(-width/2+1.3,.04,'M_Aurelion_StoneGrout')]:
        hit,p,n,index=o.ray_cast(Vector((x,-2,z)),Vector((0,1,0)),distance=4)
        assert hit and abs(p.y-expected_y)<.0001,(name,x,expected_y,p)
        assert o.data.materials[o.data.polygons[index].material_index].name==material
        rows.append(dict(mesh=name,x=x,z=z,y=p.y,material=material))
(root/'masonry-surface-verification.json').write_text(json.dumps(dict(status='PASS',samples=rows,scope='Local source face and grout depths; not a render or live gameplay qualification.'),indent=2))
print('Z02_MASONRY_SURFACE_PASS')
