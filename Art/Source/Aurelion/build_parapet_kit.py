"""Aurelion solid parapet matching the connector's 22 cm x 130 cm barrier."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'ParapetKit';ROOT.mkdir(exist_ok=True)
box('Dressed parapet footing',(0,0,.09),(3.72,.22,.18),stone,.008)
box('Parapet solid core',(0,0,.65),(3.72,.13,.94),stone,.008)
box('Upper shadow course',(0,0,1.135),(3.72,.17,.03),dark,.003)
box('Hand-rest cornice',(0,0,1.225),(3.72,.22,.15),stone,.009)
for x in (-1.93,1.93):box('Parapet terminal pier',(x,0,.65),(.14,.22,1.3),stone,.008)
for side in (-1,1):
    for x in (-1.23,0,1.23):
        box('Recessed panel bed',(x,side*.069,.65),(1.14,.014,.75),dark,.003)
        box('Dressed panel face',(x,side*.081,.65),(1.06,.012,.66),stone,.004)
        for dx in (-.47,.47):box('Panel carved border',(x+dx,side*.091,.65),(.026,.012,.55),stone,.002)
        for z in (.39,.91):box('Panel cross border',(x,side*.091,z),(.95,.012,.026),stone,.002)
        box('Panel conductor',(x,side*.104,.65),(.014,.006,.30),gold,0)
    box('Cornice narrow inlay',(0,side*.107,1.19),(3.70,.006,.018),gold,0)
o=export('SM_Aurelion_KIT_Parapet_4m',[4,.22,1.3]);manifest[-1]['collision']='None; intended to visualize retained solid connector guard volumes'
scene.world=bpy.data.worlds.new('Parapet studio');scene.world.color=(.17,.17,.17)
d=bpy.data.lights.new('Softbox','AREA');d.energy=550;d.size=5;l=bpy.data.objects.new('Softbox',d);scene.collection.objects.link(l);l.location=(0,-4,5);l.rotation_euler=(Vector((0,0,.65))-l.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Parapet review');camera=bpy.data.objects.new('Parapet review',d);scene.collection.objects.link(camera);camera.location=(4,-6,3);camera.rotation_euler=(Vector((0,0,.65))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=45;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'Parapet.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Custom parapet source; full bridge and live route qualification pending',modules=manifest),indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_ParapetKit.blend'));bpy.ops.render.render(write_still=True)
