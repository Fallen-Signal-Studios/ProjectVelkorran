"""Measured cut paving pieces for Survivor Bend; existing four-metre interiors reused."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z02PavingKit'; ROOT.mkdir(exist_ok=True)
fit=json.loads((ROOT/'floor-fit.json').read_text())
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)

def paving(name,width,length,route=False):
    box('Continuous recessed joint bed',(0,0,.022),(width,length,.044),dark,.002)
    intervals=[(-2,-1.852),(-1.828,-.004),(.004,1.828),(1.852,2)] if route else [(-width/2+.004,-.004),(.004,width/2-.004)]
    for x1,x2 in intervals:
        for y in (-length/4,length/4):
            box('Fitted honed slab',((x1+x2)/2,y,.082),(x2-x1,length/2-.008,.076),basalt if route else stone,.004)
    if route:
        for x in (-1.84,1.84):
            box('Inlay groove bed',(x,0,.112),(.024,length,.012),dark,.001)
            box('Flush route conductor',(x,0,.117),(.012,length-.004,.006),gold,.001)
            for y in (-length/2+.05,length/2-.05):box('Route index stone',(x,y,.116),(.085,.028,.008),stone,.001)
    else:
        for x in (-width/2+.13,width/2-.13):
            for y in (-length/2+.13,length/2-.13):
                box('Paving alignment key',(x,y,.117),(.16,.012,.006),basalt,.001)
                box('Key gold end',(x+.087,y,.117),(.012,.012,.006),gold,.001)
    o=export('SM_Aurelion_KIT_'+name,[width,length,.12]); o.hide_render=True
    manifest[-1].update(collision='None; fitted over retained Z02 floor collision',surface_top_metres=.12)
    return o

short=fit['rows'][-1]['length']
for side,col in (('West',fit['columns'][0]),('East',fit['columns'][-1])):
    paving('Z02Paving'+side+'_4m',col['width'],4)
    paving('Z02Paving'+side+'_Short',col['width'],short)
paving('Z02PavingIvory_Short',4,short)
route=paving('Z02PavingRoute_Short',4,short,True)
# Source review shows the complete short row, each source FBX still at its origin.
lookup={o.name:o for o in modules}
for col in fit['columns']:
    suffix='Z02Paving'+col['kind'].title()+'_Short'; src=lookup['SM_Aurelion_KIT_'+suffix]
    o=src.copy(); o.data=src.data; scene.collection.objects.link(o); o.hide_render=False; o.location.x=(col['x']+7000)/100
scene.world=bpy.data.worlds.new('Cut paving studio'); scene.world.color=(.12,.12,.12)
for pos in ((-8,-2,8),(0,2,9),(8,-2,8)):
    d=bpy.data.lights.new('Cut paving light','AREA'); d.energy=2200; d.size=8
    o=bpy.data.objects.new('Cut paving light',d); scene.collection.objects.link(o); o.location=pos; o.rotation_euler=(Vector((0,0,0))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Cut paving review'); camera=bpy.data.objects.new('Cut paving review',d); scene.collection.objects.link(camera)
camera.location=(9,-15,17); camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler(); d.lens=33; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=24; scene.cycles.use_denoising=True
scene.render.resolution_x=1600; scene.render.resolution_y=900; scene.render.resolution_percentage=100; scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'Cut-paving-row.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Measured cut pieces; room review required',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_Z02PavingKit.blend')); bpy.ops.render.render(write_still=True)
