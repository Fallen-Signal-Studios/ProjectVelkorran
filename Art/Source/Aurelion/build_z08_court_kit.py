"""Flush quarantine court with a fitted surrounding paving margin."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z08CourtKit';ROOT.mkdir(exist_ok=True)
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)
box('Continuous court joint bed',(0,0,.022),(22,16,.044),dark,.002)
# Two-metre dressed stones; the central six metres retain the route palette.
for x in range(-10,11,2):
    for y in range(-7,8,2):
        box('Honed court stone',(x,y,.082),(1.992,1.992,.076),basalt if abs(x)<=2 else stone,.004)
# Slim registers stand only 2 mm above the slab and remain within the 12.2 cm assembly.
for x in (-9,9):
    box('Court isolation register',(x,0,.121),(.024,13.2,.002),gold,.0001)
for y in (-7,7):
    for x in (-6,6):box('Interrupted threshold register',(x,y,.121),(6,.024,.002),gold,.0001)
for x in (-2.84,2.84):
    box('Continuous route conductor',(x,0,.121),(.012,15.996,.002),gold,.0001)
# Quartered octagonal seal; narrow ribbons preserve the route and stone readability.
from math import pi,cos,sin,atan2,sqrt
for radius in (4.2,4.36):
    for i in range(8):
        a=2*pi*i/8;b=2*pi*(i+1)/8
        ax,ay=radius*cos(a),radius*sin(a);bx,by=radius*cos(b),radius*sin(b)
        o=box('Octagonal isolation seal',((ax+bx)/2,(ay+by)/2,.121),(sqrt((bx-ax)**2+(by-ay)**2)-.04,.024,.002),gold,.0001)
        o.rotation_euler.z=atan2(by-ay,bx-ax)
for sx in (-1,1):
    for sy in (-1,1):
        for j in range(3):
            box('Corner registration',(sx*(8.4-j*.12),sy*6.4,.121),(.028,.24+j*.08,.002),gold,.0001)
for x in (-10,-8,-6,-4,4,6,8,10):
    for y in (-7,-5,-3,-1,1,3,5,7):
        for dx,dy in ((-.86,-.86),(.86,.86)):
            box('Stone alignment key',(x+dx,y+dy,.121),(.12,.012,.002),basalt,.0001)
court=export('SM_Aurelion_KIT_Z08IsolationCourt',[22,16,.122])
manifest[-1].update(collision='None; original Z08_Floor remains authoritative',court_footprint_m=[18,14],surrounding_margin_m=[2,1],surface_relief_mm=2,position_precision=6)
scene.world=bpy.data.worlds.new('Isolation court studio');scene.world.color=(.12,.12,.12)
for pos in ((-9,-8,14),(9,6,15)):
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=6000;o.data.size=12;o.rotation_euler=(Vector((0,0,0))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(14,-18,26));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=28;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'court.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored court source; room fit and live qualification pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z08-Court.blend'));bpy.ops.render.render(write_still=True)
