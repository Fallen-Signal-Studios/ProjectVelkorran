"""Aurelion chamber coffer: centered 4 m module fitted to the existing art envelope."""
from pathlib import Path
helper = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0], str(helper), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z10ChamberWall'
ROOT.mkdir(exist_ok=True)

# The old visual module occupies X/Z ±2 m and Y 0..0.2799 m.
# Positive Y is the inward face. Native chamber collision remains separate.
box('Continuous coffer backing', (0,.06,0), (3.99,.12,3.99), stone, .009)
for z in (-1.92,1.92):
    box('Dressed horizontal course', (0,.16,z), (3.99,.22,.15), stone, .012)
    box('Inset course conductor', (0,.273,z), (3.72,.012,.055), gold, .003)
for side in (-1,1):
    x=side*1.89
    box('Deep perimeter reveal', (x,.142,0), (.20,.045,3.66), dark, .006)
    box('Chamfered frame arris', (x,.204,0), (.105,.11,3.67), stone, .01)
    for j in (-1,1):
        box('Arris fine flute', (x+j*.028,.263,0), (.025,.012,3.48), gold, .002)
    x=side*.94
    box('Recessed panel bed', (x,.142,0), (1.59,.045,3.47), dark, .009)
    box('Floating carved coffer', (x,.185,0), (1.48,.058,3.34), stone, .016)
    points=[(x-.66,.219,-1.53),(x+.42,.219,-1.53),(x+.66,.219,-1.29),
            (x+.66,.219,1.53),(x-.42,.219,1.53),(x-.66,.219,1.29),(x-.66,.219,-1.53)]
    path('Inset coffer moulding',points,.07,.028,stone,.007)
    # Large oblique channels remain legible at chamber-scale viewing distances.
    points=[(x-side*.43,.25,-1.20),(x-side*.43,.25,.62),
            (x+side*.35,.25,1.20)]
    path('Recessed oblique channel',points,.14,.012,dark,.004)
    path('Ancient gold conductor',[(px,py+.014,pz) for px,py,pz in points],.065,.012,gold,.004)
    for j in range(4):
        box('Lower relief register', (x+side*.29,.258,-1.22+j*.16), (.34,.018,.075), gold, .004)
    for z in (-1.72,1.72):
        box('Recessed keyed joint',(x,.227,z),(.20,.026,.06),dark,.004)
        box('Keyed joint cap',(x,.251,z),(.105,.025,.041),gold,.003)
box('Central axial recess',(0,.15,0),(.20,.045,3.70),dark,.006)
for x in (-.055,.055):
    box('Axial spine rail',(x,.217,0),(.026,.056,3.66),gold,.004)
# Blender-to-Unreal FBX changes handedness on Y. Mirror the authored depth so
# the imported panel occupies the original Unreal 0..27.9 cm visual envelope.
for part in parts:
    part.location.y *= -1
    part.scale.y *= -1
obj=export('SM_Aurelion_KIT_Z10ChamberCoffer',[3.99,.279,3.99])
manifest[-1].update(nominal_dimensions_m=list(obj.dimensions), position_precision=10,
    preserve_fallback_geometry=True, collision='None: visual replacement only; native chamber collision preserved')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Chamber coffer studio');scene.world.color=(.15,.15,.15)
target=Vector((0,.1,0))
for pos,power,size in [((-3,5,5),1400,4),((4,3,1),1000,3),((0,-3,4),900,3)]:
    bpy.ops.object.light_add(type='AREA',location=(pos[0],-pos[1],pos[2]));light=bpy.context.object
    light.data.energy=power;light.data.size=size
    light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(5,-9,3.4));camera=bpy.context.object
camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=5.7;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1400;scene.render.resolution_y=1400;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'coffer.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Chamber-Coffer.blend'))
bpy.ops.render.render(write_still=True)
