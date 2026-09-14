"""Thin, fitted ascent panel for the measured Z06 climb collision envelope."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z06ClimbKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
# All relief stays inside the 20 cm thickness. Continuous backing closes joints.
box('Structural stone web',(0,0,1.5),(4.38,.10,2.98),grout,.006)
for x in (-2.15,2.15):box('Dressed end stile',(x,0,1.5),(.10,.20,3),stone,.006)
for z in (.04,2.96):box('Continuous foot and graspable crown',(0,0,z),(4.2,.196,.08),stone,.009)
for side in (-1,1):
    for row in range(6):
        z=.31+row*.475
        for column in range(5):
            x=(column-2)*.824
            box('Fitted ascent ashlar',(x,side*.066,z),(.808,.046,.422),stone,.009)
            # Upper arris makes the gripping edge legible without false protrusions.
            box('Grip overhang',(x,side*.080,z+.185),(.728,.032,.044),stone,.006)
            for xx in (x-.315,x+.315):
                box('Grip terminal inset',(xx,side*.093,z+.175),(.018,.010,.022),gold,.002)
    for x in (-2.062,2.062):
        box('Recessed route channel',(x,side*.064,1.5),(.026,.020,2.70),dark,.003)
        for row in range(6):
            box('Ascent route register',(x,side*.080,.34+row*.475),(.012,.010,.22),gold,.002)
    for x in (-.824,0,.824):
        for dx in (-.027,.027):
            o=box('Crown direction chevron',(x+dx,side*.097,2.959),(.044,.006,.010),gold,.001)
            o.rotation_euler.y=math.radians(32 if dx<0 else -32)
o=export('SM_Aurelion_KIT_Z06ClimbPanel',[4.4,.2,3]);manifest[-1].update(nominal_dimensions_m=list(o.dimensions),collision='None; existing climb wall and landing boxes remain authoritative')
manifest[-1]['material_overrides']={'M_Aurelion_IvoryStone': '/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ClimbIvory'}
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; scene fit and live climb acceptance pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Climb panel studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((2,-4,5),650,4),((-3,2,3),500,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,1.5))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4,-7,4));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,1.5))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=5.7;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'climb-panel.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-ClimbPanel.blend'));bpy.ops.render.render(write_still=True)
print('CLIMB_PANEL_BUILD_PASS')
