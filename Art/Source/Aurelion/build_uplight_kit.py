"""Pier-mounted architectural uplight, with a modeled upward-facing aperture."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'UplightKit'; ROOT.mkdir(exist_ok=True)
emitter=material('M_Aurelion_UplightLens',(.92,.81,.56),0,.24)
bsdf=emitter.node_tree.nodes.get('Principled BSDF')
bsdf.inputs['Emission Color'].default_value=(1,.84,.55,1); bsdf.inputs['Emission Strength'].default_value=3
# Rear plate rests on the pier face. Front is Blender -Y / imported Unreal +Y.
box('Stone mounting plate',(0,.055,.36),(.7,.11,.72),stone,.02)
for x in (-.28,.28):
    box('Mounting reveal',(x,-.006,.36),(.025,.014,.58),dark,.003)
    box('Conductive spine',(x,-.021,.36),(.008,.014,.55),gold,.002)
box('Cantilever plinth',(0,-.24,.11),(.76,.59,.22),stone,.025)
box('Recessed luminaire body',(0,-.26,.3),(.62,.49,.25),dark,.018)
for x in (-.32,.32):
    box('Stone side cheek',(x,-.24,.37),(.09,.53,.38),stone,.018)
box('Front stone lip',(0,-.49,.32),(.72,.12,.32),stone,.018)
box('Lip gold band',(0,-.554,.37),(.61,.018,.025),gold,.004)
box('Upward optical aperture',(0,-.275,.442),(.49,.32,.035),emitter,.006)
for i in range(5):
    box('Optical louver',(-.2+i*.1,-.275,.471),(.013,.335,.047),gold,.003)
for x in (-.26,.26):
    for z in (.13,.62):
        ring('Recessed fastener socket',x,-.007,z,.024,.008,dark)
fixture=export('SM_Aurelion_KIT_PierUplight',[.76,.62,.72])
manifest[-1]['nominal_dimensions_m']=list(fixture.dimensions)
manifest[-1]['collision']='None; pier-mounted visual fixture above player height'
manifest[-1]['optical_center_local_m']=[0,-.275,.46]
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Architectural luminaire candidate; room lighting and performance review required',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Fixture studio'); scene.world.color=(.1,.1,.1)
data=bpy.data.lights.new('Softbox','AREA'); data.energy=100; data.size=2
o=bpy.data.objects.new('Softbox',data); scene.collection.objects.link(o); o.location=(1,-2,2)
o.rotation_euler=(Vector((0,0,.35))-o.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Fixture review'); camera=bpy.data.objects.new('Fixture review',data); scene.collection.objects.link(camera)
camera.location=(1,-1.7,1.3); camera.rotation_euler=(Vector((0,-.2,.35))-camera.location).to_track_quat('-Z','Y').to_euler()
data.lens=58; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1000; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'Pier-uplight.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_UplightKit.blend'))
bpy.ops.render.render(write_still=True)
