"""Physical ivory wayfinding plaque for retained native refuge text."""
from pathlib import Path
import bpy,json
from mathutils import Vector
base=Path(__file__).resolve().parent
exec(compile((base/'build_architecture_kit.py').read_text().split('# Four metre bay:')[0],'helpers','exec'),globals())
ROOT=base/'RefugeSignKit';ROOT.mkdir(exist_ok=True);old_box=box
def box(name,loc,size,mat=stone,bevel=.004):return old_box(name,loc,size,mat,min(bevel,min(size)*.2))
box('Recessed backing',(0,0,.375),(2.4,.04,.75),dark,.006)
box('Ivory inscription field',(0,-.025,.375),(2.24,.012,.65),stone,.006)
for x in (-1.17,1.17):box('Raised side arris',(x,-.027,.375),(.06,.026,.69),stone,.004)
for z in (.015,.735):box('Protective rim',(0,-.027,z),(2.4,.026,.03),stone,.004)
for x in (-1.06,1.06):
    for z in (.075,.675):
        box('Isolated retaining socket',(x,-.033,z),(.024,.004,.024),dark,.002)
        box('Gold retaining pin',(x,-.036,z),(.009,.002,.009),gold,.001)
        box('Rear wall standoff',(x,.03,z),(.08,.02,.08),dark,.003)
for x in (-1.105,1.105):box('Edge conductor',(x,-.033,.375),(.005,.004,.47),gold,.0008)
export('SM_Aurelion_KIT_RefugeSign',[2.4,.08,.75]);manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='Visual mounting plaque; no collision added')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source authored; placement pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Plaque studio');scene.world.color=(.15,.15,.15)
for pos,power in [((1,-3,4),800),((-2,-1,2),600),((1,2,3),900)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=3;a.rotation_euler=(Vector((0,0,.375))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2,-4,2));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.375))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=2.9;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=800;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'plaque.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Refuge-Sign.blend'));bpy.ops.render.render(write_still=True)
