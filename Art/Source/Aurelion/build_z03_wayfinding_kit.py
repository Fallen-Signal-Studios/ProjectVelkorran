"""Recessed sensor-gallery route register, metres and unit-scale placement."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z03WayfindingKit';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.54,.76,.82),0,.35)
# A low inset chassis, cut-stone side margins and two light apertures.
box('Register chassis',(0,0,.003),(.12,1,.006),dark,.0008)
for x in (-.048,.048):
    box('Stone margin',(x,0,.005),(.018,.992,.004),stone,.0005)
for y in (-.486,.486):
    box('Service end cap',(0,y,.005),(.074,.02,.004),stone,.0005)
for y in (-.25,.25):
    box('Light aperture',(0,y,.006),(.026,.34,.003),lens,.0004)
    for x in (-.020,.020):
        box('Conductor border',(x,y,.0058),(.003,.35,.002),gold,.0003)
for y in (-.457,.457):
    for x in (-.022,.022):
        bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.003,depth=.001,location=(x,y,.0065))
        finish(bpy.context.object,gold,.0001)
o=export('SM_Aurelion_KIT_Z03RouteRegister',[.12,1,.0075])
o.hide_render=True
box('Sign backing',(0,0,.325),(1.56,.08,.65),dark,.008)
for x in (-.785,.785):box('Stone sign edge',(x,-.012,.325),(.03,.06,.62),stone,.004)
for z in (.023,.627):box('Sign edge register',(0,-.0445,z),(1.49,.005,.012),gold,.001)
for x in (-.735,.735):
    for z in (.085,.565):
        bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.006,depth=.003,location=(x,-.0415,z),rotation=(math.pi/2,0,0))
        finish(bpy.context.object,gold,.0004)
sign=export('SM_Aurelion_KIT_Z03DestinationPlaque',[1.6,.087,.65]);sign.hide_render=True
for spec in manifest:spec.update(position_precision=10,preserve_fallback_geometry=True,collision='None; visual wayfinding only')
placements=[dict(location_cm=[7200,-19550+100*i,-.10],yaw=0) for i in range(44)]
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
room=json.loads((ROOT.parent/'Z03CeilingKit/room-baseline.json').read_text())
(ROOT/'guidance-baseline.json').write_text(json.dumps(next(c for c in room['components'] if c['actor']=='Aurelion_Art_M12_Z03_15_6ec551'),indent=2))
o.hide_render=False
scene.world=bpy.data.worlds.new('Route register studio');scene.world.color=(.18,.18,.18)
for pos,power,size in [((1,-2,3),220,2),((-2,1,2),180,2)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(.9,-1.3,1.8));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=1.3;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=900;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'route-register.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z03-Wayfinding.blend'));bpy.ops.render.render(write_still=True)
