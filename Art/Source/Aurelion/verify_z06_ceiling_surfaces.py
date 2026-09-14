"""Check exported roof closure and visible recessed conductors from the room side."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z06CeilingKit';reports=[]
for spec in json.loads((root/'manifest.json').read_text())['modules']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root/(spec['asset']+'.fbx')))
    o=next(o for o in bpy.context.scene.objects if o.type=='MESH');width=spec['nominal_dimensions_m'][0];roof=[];gold=[]
    for fx in (-.49,-.25,0,.25,.49):
        for fy in (-.49,-.25,0,.25,.49):
            hit,p,n,index=o.ray_cast(Vector((width*fx,4*fy,-1)),Vector((0,0,1)),distance=2)
            assert hit and -.001<=p.z<=.551 and n.z<-.5,(spec['asset'],fx,fy,p)
            roof.append([p.x,p.y,p.z])
    for j in range(8):
        a=math.pi/4+j*math.pi/4;factor=math.cos(math.pi/8-.005)
        x=(width/2-.22-.376)*math.cos(a)*factor;y=(1.78-.376)*math.sin(a)*factor
        hit,p,n,index=o.ray_cast(Vector((x,y,-1)),Vector((0,0,1)),distance=2)
        assert hit and o.data.materials[o.data.polygons[index].material_index].name=='M_Aurelion_AncientGold',(j,p)
        gold.append([p.x,p.y,p.z])
    reports.append(dict(asset=spec['asset'],roof_closure_samples=roof,visible_gold_samples=gold))
(root/'surface-verification.json').write_text(json.dumps(dict(status='passed',meshes=reports,qualification='Exported visual geometry only; runtime navigation and performance not qualified'),indent=2))
print('Z06_CEILING_SURFACE_PASS')
