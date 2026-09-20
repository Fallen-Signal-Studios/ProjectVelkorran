"""Aurelion support module fitted to the existing upright and diagonal rib instances."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z10ChamberRibs';ROOT.mkdir(exist_ok=True)
# Bounds remain inside the stock beam: .572798 x 1.121431 x 5.2 m, bottom pivot.
box('Solid load-bearing spine',(0,0,2.6),(.40,.91,5.2),stone,.025)
for z in (.045,.175,5.025,5.155):
    box('Stepped square capital',(0,0,z),(.57,1.12,.09),stone,.014)
for z in (.285,4.915):
    box('Inset bronze capital course',(0,0,z),(.50,1.05,.07),gold,.008)
for z in (.395,4.805):
    box('Chamfered load transition',(0,0,z),(.48,1.01,.15),stone,.02)
for side in (-1,1):
    # Broad inward and outward faces: four fluted stone staves, gold in the three reveals.
    for y in (-.36,-.12,.12,.36):
        box('Deep fluted stone stave',(side*.223,y,2.6),(.085,.175,4.25),stone,.022)
    for y in (-.24,0,.24):
        box('Recessed axial channel',(side*.205,y,2.6),(.012,.063,4.19),dark,.003)
        box('Recessed gold conductor',(side*.219,y,2.6),(.014,.039,4.03),gold,.004)
    # Narrow sides retain the same language when seen along the elevated braces.
    box('Side channel bed',(0,side*.46,2.6),(.24,.022,4.19),dark,.006)
    for x in (-.15,.15):box('Side dressed arris',(x,side*.475,2.6),(.075,.045,4.25),stone,.012)
    box('Side gold conductor',(0,side*.478,2.6),(.065,.014,4.03),gold,.005)
    for z in (.70,4.5):
        box('Bronze load register',(side*.263,0,z),(.025,.43,.24),gold,.012)
        for y in (-.125,0,.125):box('Register relief',(side*.281,y,z),(.01,.045,.13),stone,.004)
obj=export('SM_Aurelion_KIT_Z10ChamberRib',[.57,1.12,5.2])
manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,
    collision='None; preserve separate native gameplay collision',pivot='Bottom center')
scene.world=bpy.data.worlds.new('Rib studio');scene.world.color=(.14,.14,.14)
for pos,energy,size in (((-4,-5,7),1400,5),((4,2,6),1800,4),((-3,4,2),1000,3)):
    d=bpy.data.lights.new('Rib softbox','AREA');d.energy=energy;d.size=size
    o=bpy.data.objects.new('Rib softbox',d);scene.collection.objects.link(o);o.location=pos
    o.rotation_euler=(Vector((0,0,2.6))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Rib review');camera=bpy.data.objects.new('Rib review',d);scene.collection.objects.link(camera);scene.camera=camera
camera.location=(-7,-8,6.5);camera.rotation_euler=(Vector((0,0,2.6))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=60
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1000;scene.render.resolution_y=1400;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'Chamber-rib.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Chamber-Rib.blend'))
bpy.ops.render.render(write_still=True)
