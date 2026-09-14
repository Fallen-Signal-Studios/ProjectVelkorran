"""Centred moving gate face for the existing 44 x 430 x 258 cm rescue door body."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z06RescueGateKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
box('Continuous gate backing',(0,0,0),(.32,4.12,2.40),grout,.005)
for side in (-1,1):
    box('Terminal stone bearing',(0,side*2.055,0),(.44,.19,2.58),stone,.012)
    box('Transverse stone rail',(0,0,side*1.20),(.44,3.92,.18),stone,.012)
    # Detailed faces on both sides, entirely inside the moving collision envelope.
    for y in (-1.00,1.00):
        box('Recessed gate leaf',(side*.174,y,0),(.028,1.84,2.12),stone,.005)
        for dy in (-.82,.82):box('Leaf vertical frame',(side*.193,y+dy,0),(.030,.075,1.94),stone,.004)
        for z in (-.93,.93):box('Leaf horizontal frame',(side*.193,y,z),(.030,1.565,.075),stone,.004)
        box('Inset central field',(side*.193,y,0),(.032,1.42,1.64),stone,.005)
        for z in (-.64,.64):box('Recessed conductor crossbar',(side*.213,y,z),(.012,1.148,.028),gold,.002)
        for dy in (-.56,.56):box('Recessed conductor upright',(side*.213,y+dy,0),(.012,.028,1.252),gold,.002)
        # Small registers have a clear mechanical rhythm without dense repeated microgeometry.
        for z in (-.18,0,.18):box('Power register',(side*.214,y,z),(.010,.16,.065),gold,.002)
    box('Central closing seam',(side*.175,0,0),(.018,.06,2.20),grout,.002)
gate=export('SM_Aurelion_KIT_Z06RescueGate',[.44,4.3,2.58]);manifest[-1]['collision']='None; visual attaches to the existing authoritative moving box'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; current-world fit and native motion validation pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Gate studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((5,-4,6),1100,5),((-4,3,5),850,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(7,-5,3));camera=bpy.context.object;camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=5.6;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'rescue-gate.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z06-RescueGate.blend'));bpy.ops.render.render(write_still=True)
print('Z06_RESCUE_GATE_BUILD_PASS')
