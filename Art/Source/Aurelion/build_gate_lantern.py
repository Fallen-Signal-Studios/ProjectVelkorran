"""Editable suspended Aurelion gate lantern, metre units and separate optical fields."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'GateLanternKit';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.92,.85,.70),0,.22)
p=lens.node_tree.nodes.get('Principled BSDF');p.inputs['Emission Color'].default_value=(.92,.85,.70,1);p.inputs['Emission Strength'].default_value=2
box('Ivory optical carrier',(0,0,.06),(2.4,.44,.22),stone,.018)
box('Recessed optical bed',(0,0,-.058),(2.2,.32,.014),dark,.003)
for y in (-.175,.175):
    box('Retaining gold rail',(0,y,-.065),(2.22,.035,.035),gold,.006)
for x in (-1.13,1.13):
    box('End cap',(x,0,-.057),(.11,.40,.052),stone,.008)
    box('Cap conductor',(x,0,-.087),(.055,.28,.008),gold,.002)
for i in range(12):
    x=-1.0+i*2.0/11
    box('Segmented diffusion glass',(x,0,-.070),(.165,.28,.012),lens,.002)
    if i<11:box('Optical separator',(x+.091,0,-.074),(.012,.30,.020),gold,.002)
for x in (-.9,.9):
    box('Ceiling anchor',(x,0,.915),(.28,.28,.07),stone,.012)
    box('Suspension stem',(x,0,.54),(.12,.12,.75),gold,.008)
    for z in (.20,.79):box('Stem bearing',(x,0,z),(.20,.20,.12),stone,.012)
    for y in (-.069,.069):box('Stem recessed stripe',(x,y,.50),(.026,.012,.43),dark,.002)
o=export('SM_Aurelion_KIT_GateLantern',[2.4,.44,1.041])
manifest[-1].update(nominal_dimensions_m=list(o.dimensions),collision='Visual fixture only; deployed NoCollision above the route')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; scene fit pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Lantern studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((3,-3,4),700,4),((-3,2,2),600,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,.3))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(3,-4,-1.3));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.35))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=3.5;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'gate-lantern.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-GateLantern.blend'));bpy.ops.render.render(write_still=True)
print('GATE_LANTERN_BUILD_PASS')
