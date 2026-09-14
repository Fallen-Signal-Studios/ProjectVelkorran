"""Seven-metre Crucible engaged piers with cut stone and recessed service conduits."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z08ColumnKit';ROOT.mkdir(exist_ok=True)
# Full footprint 1.20 x 1.00 m; bottom origin, 7 m height.
for z,w,d,h in [(.09,1.2,1.,.18),(.23,1.13,.94,.10),(.36,1.03,.86,.16),
                 (6.40,.99,.82,.16),(6.55,1.08,.90,.14),(6.74,1.16,.97,.22),(6.94,1.2,1.,.12)]:
    box('Dressed capital' if z>6 else 'Stepped plinth',(0,0,z),(w,d,h),stone,.012)
for row in range(8):
    z=.81+row*.72
    box('Cut shaft course',(0,.045,z),(.86,.66,.702),stone,.014)
    for x in (-.29,0,.29):
        box('Front ashlar flute',(x,-.306,z),(.238,.085,.676),stone,.008)
    for side in (-1,1):
        box('Return stone field',(side*.429,.045,z),(.045,.51,.662),stone,.008)
for side in (-1,1):
    x=side*.145
    box('Recessed continuous conduit bed',(x,-.297,3.34),(.048,.025,5.73),dark,.003)
    box('Inset continuous conductor',(x,-.313,3.34),(.012,.013,5.68),gold,.002)
    for row in range(8):
        z=.51+row*.72
        if side<0:box('Course service collar',(0,-.009,z),(.91,.77,.058),stone,.006)
        box('Collar conductor marker',(side*.333,-.399,z),(.082,.012,.012),gold,.0015)
    pts=[(side*.145,-.355,6.08),(side*.145,-.355,6.26),(side*.40,-.355,6.45)]
    path('Capital oblique service recess',pts,.052,.02,dark,.003)
    path('Capital oblique inlay',[(x,y-.013,z) for x,y,z in pts],.012,.009,gold,.0015)
    for row in range(7):
        box('Base maintenance register',(side*.34,-.406,.64+row*.036),(.10,.016,.011),gold,.0015)
# Shallow front access panel with mechanical corner fasteners and narrow engraving.
box('Service access recess',(0,-.360,1.12),(.34,.034,.44),dark,.005)
box('Service access stone inset',(0,-.384,1.12),(.30,.026,.40),stone,.008)
for x in (-.115,.115):
    for z in (.975,1.265):
        bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.009,depth=.004,location=(x,-.400,z),rotation=(math.pi/2,0,0))
        finish(bpy.context.object,gold,.001)
for z in (1.07,1.12,1.17):box('Service panel engraved register',(0,-.400,z),(.14,.003,.006),dark,.001)
o=export('SM_Aurelion_KIT_Z08EngagedPier',[1.2,1.,7.])
manifest[-1]['collision']='None; retains six former visual-only column envelopes'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored pier; engine fit and visual review pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Crucible pier studio');scene.world.color=(.18,.18,.18)
for pos,power,size in [((2,-5,6),1800,4),((-3,-1,3),1000,3),((3,3,7),1800,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,3.5))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(7,-13,8));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,3.5))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=8.3;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1000;scene.render.resolution_y=1400;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'pier.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Crucible-Pier.blend'));bpy.ops.render.render(write_still=True)
print('CRUCIBLE_PIER_BUILD_PASS')
