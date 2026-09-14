"""Authored faction capture assemblies on the existing 5 x 4 m platform envelopes."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z07CaptureKit';ROOT.mkdir(exist_ok=True)
def floor_ring(radius,width,z,mat):
    # Horizontal circular register; register sits less than one millimetre above the platform surface.
    for i in range(64):
        a=i*math.tau/64;b=(i+1)*math.tau/64
        xy=[(r*math.cos(t),r*math.sin(t)) for r,t in ((radius,a),(radius,b),(radius-width,b),(radius-width,a))]
        verts=[(x,y,zz) for zz in (z,z+.006) for x,y in xy];faces=[(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
        m=bpy.data.meshes.new('Resonator register');m.from_pydata(verts,[],faces);m.update();o=bpy.data.objects.new('Resonator register',m);scene.collection.objects.link(o);finish(o,mat,0)
def assembly(faction):
    box('Continuous foundation',(0,0,.20),(5,4,.40),stone,.02)
    for x in (-1.25,1.25):
        for y in (-1,1):box('Dressed plinth course',(x,y,.45),(2.48,1.98,.22),stone,.018)
    box('Recessed mechanism bedding',(0,0,.62),(4.98,3.98,.12),dark,.006)
    for x in (-1.25,1.25):
        for y in (-1,1):box('Honed upper platform',(x,y,.68),(2.488,1.988,.04),stone,.006)
    for sign in (-1,1):
        box('Long plinth arris',(0,sign*1.965,.575),(4.97,.07,.05),stone,.006)
        box('Short plinth arris',(sign*2.465,0,.575),(.07,3.84,.05),stone,.006)
        for x in (-1.8,-.9,0,.9,1.8):
            box('Plinth access panel',(x,sign*1.991,.29),(.58,.015,.25),dark,.004)
            box('Panel inner stone',(x,sign*2,.29),(.50,.008,.19),stone,.003)
    for x in (-2.4,2.4):
        for y in (-1.9,1.9):
            box('Capture spine',(x,y,2),(.136,.136,4),stone,.009)
            for z in (.8,1.6,2.4,3.2,3.85):box('Spine collar',(x,y,z),(.15,.15,.075),dark,.004)
            for sign in (-1,1):
                box('Spine recessed conductor',(x+sign*.067,y,2.28),(.007,.036,2.90),gold,.001)
                box('Spine face groove',(x,y+sign*.067,2.28),(.052,.004,2.90),dark,.001)
            for z in (1.2,2,2.8,3.6):box('Spine bearing node',(x,y,z),(.148,.148,.14),stone,.006)
    for y in (-1.9,1.9):
        for x in (-1.1655,1.1655):box('Upper resonant beam',(x,y,3.91),(2.319,.15,.18),stone,.009)
        box('Upper beam conductor',(0,y,3.806),(4.64,.036,.012),gold,.001)
    for x in (-2.4,2.4):
        box('Transverse capture beam',(x,0,3.91),(.15,3.65,.18),stone,.009)
    if faction=='Dominion':
        for radius in (1.18,.94,.58):floor_ring(radius,.024,.701,gold)
        for i in range(16):
            a=i*math.tau/16;o=box('Resonance index',(1.30*math.cos(a),1.30*math.sin(a),.704),(.10,.022,.006),gold,.001);o.rotation_euler.z=a
    else:
        for x in (-1.2,-.6,0,.6,1.2):box('Isolation grid',(x,0,.704),(.014,2.70,.006),gold,.001)
        for y in (-1.35,0,1.35):box('Isolation cross register',(0,y,.704),(2.7,.014,.006),gold,.001)
    o=export('SM_Aurelion_KIT_Z07'+faction+'Capture',[5,4.008,4]);manifest[-1]['nominal_dimensions_m']=list(o.dimensions);manifest[-1]['collision']='None; original platform and four spine proxies retained';o.hide_render=True;return o
dom=assembly('Dominion');ref=assembly('Reformation');dom.hide_render=False
scene.world=bpy.data.worlds.new('Capture kit studio');scene.world.color=(.13,.13,.13)
for pos in ((2,-6,7),(-5,-2,4),(4,4,6)):
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=1700;o.data.size=5;o.rotation_euler=(Vector((0,0,1.5))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(8,-10,7));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,1.5))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=8;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'capture-kit.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Capture frame source; scene binding and in-engine review pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z07-Capture.blend'));bpy.ops.render.render(write_still=True)
