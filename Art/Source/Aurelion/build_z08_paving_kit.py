"""True-size six-metre central route for the quarantine crucible."""
from pathlib import Path
source=Path(__file__).with_name('build_z02_paving_kit.py')
code=source.read_text().split("short=fit['rows']")[0]
code=code.replace("/'Z02PavingKit'","/'Z08PavingKit'").replace('retained Z02 floor','retained Z08 floor')
code=code.replace('[(-2,-1.852),(-1.828,-.004),(.004,1.828),(1.852,2)]','[(-3,-2.852),(-2.828,-.004),(.004,2.828),(2.852,3)]').replace('(-1.84,1.84)','(-2.84,2.84)')
exec(compile(code,str(source),'exec'))
o=paving('Z08PavingRoute_6x4',6,4,True);o.hide_render=False
scene.world=bpy.data.worlds.new('Quarantine route studio');scene.world.color=(.12,.12,.12)
for pos in ((-4,-3,6),(4,3,7)):
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=1600;light.data.size=5;light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(6,-8,9));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=8;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'route.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Fitted six-metre route source; room and live qualification pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z08-Route.blend'));bpy.ops.render.render(write_still=True)
