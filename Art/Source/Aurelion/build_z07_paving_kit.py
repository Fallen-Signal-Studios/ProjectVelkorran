"""Three true-size perimeter cuts for the 34 by 22 metre capture gallery."""
from pathlib import Path
source=Path(__file__).with_name('build_z02_paving_kit.py')
code=source.read_text().split("short=fit['rows']")[0]
code=code.replace("/'Z02PavingKit'","/'Z07PavingKit'").replace("fit=json.loads((ROOT/'floor-fit.json').read_text())",'').replace('retained Z02 floor','retained Z07 floor')
exec(compile(code,str(source),'exec'))
for name,width,route in [('Z07PavingIvory_4x3',4,False),('Z07PavingRoute_4x3',4,True),('Z07PavingCorner_3x3',3,False)]:
    paving(name,width,3,route)
for obj,x in zip(modules,(-4,0,3.5)):
    obj.hide_render=False;obj.location.x=x
scene.world=bpy.data.worlds.new('Capture gallery paving studio');scene.world.color=(.12,.12,.12)
for pos in ((-5,-2,7),(5,1,8)):
    d=bpy.data.lights.new('Softbox','AREA');d.energy=1800;d.size=6
    o=bpy.data.objects.new('Softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,0))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Cut paving review');camera=bpy.data.objects.new('Cut paving review',d);scene.collection.objects.link(camera)
camera.location=(7,-10,12);camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=40;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ROOT/'Cut-paving.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Measured perimeter cuts; visual and live traversal qualification pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_Z07PavingKit.blend'));bpy.ops.render.render(write_still=True)
