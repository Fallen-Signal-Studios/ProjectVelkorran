"""Low Aurelion containment coffer, fitted to the measured 3 x 1.2 x 1.2 m cover."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'CoverCofferKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
box('Continuous containment body',(0,0,.60),(2.88,1.08,1.08),stone,.018)
for z,size in [(.055,(3,1.2,.11)),(.155,(2.96,1.16,.09)),(1.125,(2.96,1.16,.07))]:box('Stepped cap or socle',(0,0,z),size,stone,.009)
for y in (-.575,.575):box('Lid perimeter rail',(0,y,1.18),(3,.05,.04),stone,.006)
for x in (-1.475,1.475):box('Lid end rail',(x,0,1.18),(.05,1.10,.04),stone,.006)
for side in (-1,1):
    for x in (-1,0,1):
        box('Recessed fitted panel',(x,side*.547,.635),(.87,.014,.77),grout,.003)
        box('Ivory inset field',(x,side*.563,.635),(.79,.018,.69),stone,.007)
        for xx in (x-.405,x+.405):box('Containment vertical bead',(xx,side*.578,.635),(.018,.014,.70),gold,.002)
        box('Seal register',(x,side*.579,.64),(.085,.014,.24),gold,.002)
    for x in (-1.43,1.43):
        box('Reinforced corner arris',(x,side*.566,.63),(.10,.06,.88),stone,.007)
        for z in (.34,.91):box('Corner bearing',(x,side*.593,z),(.11,.014,.08),gold,.002)
    box('Functional horizontal channel',(0,side*.585,1.015),(2.76,.018,.024),gold,.003)
for x in (-1.46,1.46):
    box('End panel bed',(x,0,.64),(.018,.83,.74),grout,.003)
    box('End inset stone',(x*1.008,0,.64),(.014,.75,.66),stone,.003)
    for y in (-.31,.31):box('End lock channel',(x*1.016,y,.64),(.012,.028,.53),gold,.002)
# The top is a complete level surface with shallow perimeter joints, not an open lid.
for x in (-.99,0,.99):box('Dressed lid stone',(x,0,1.162),(.978,1.06,.068),stone,.008)
o=export('SM_Aurelion_KIT_Z06CoverCoffer',[3,1.2,1.2]);manifest[-1].update(nominal_dimensions_m=list(o.dimensions),collision='None; four existing cover boxes remain authoritative')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; scene fit and live cover acceptance pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Coffer studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((3,-4,5),700,4),((-3,2,3),500,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,.6))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4,-5,3));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.6))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=4;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'cover-coffer.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-CoverCoffer.blend'));bpy.ops.render.render(write_still=True)
print('COVER_COFFER_BUILD_PASS')
