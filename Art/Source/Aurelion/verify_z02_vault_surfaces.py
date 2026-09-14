"""Regression check for ceiling backing occlusion found in the room preview."""
import bpy
import json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z02VaultKit'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'SM_Aurelion_KIT_Z02CentralCeiling.fbx'))
mesh=next(o for o in bpy.context.scene.objects if o.type=='MESH')
fit=json.loads((root/'measured-profile.json').read_text()); width=12.76; length=fit['roof_y_half_m']*2
rows=[]
for ix in range(6):
    for iy in range(10):
        x=-width/2+(ix+.5)*width/6; y=-length/2+(iy+.5)*length/10
        hit,p,n,index=mesh.ray_cast(Vector((x,y,-1)),Vector((0,0,1)),distance=2)
        assert hit and abs(p.z-.07)<.001,(ix,iy,p)
        material=mesh.data.materials[mesh.data.polygons[index].material_index].name
        assert material=='M_Aurelion_IvoryStone',(ix,iy,material)
        rows.append(dict(column=ix,row=iy,underside_z=p.z,material=material))
(root/'ceiling-surface-verification.json').write_text(json.dumps(dict(status='passed',coffer_surfaces=rows,scope='Sixty exported coffer inset ray checks; not runtime or final visual acceptance'),indent=2))
print('Z02_VAULT_COFFER_SURFACE_PASS')
