"""Aurelion paving: flush route inlays, jointed stone and fitted side strips."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'PavingKit'; ROOT.mkdir(exist_ok=True)
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)

def paving(name,width,route=False):
    box('Continuous recessed joint bed',(0,0,.022),(width,4,.044),dark,.002)
    if route:
        intervals=[(-2,-1.852),(-1.828,-.004),(.004,1.828),(1.852,2)]
    else:
        intervals=[(-width/2+.004,-.004),(.004,width/2-.004)]
    for x1,x2 in intervals:
        for y in (-1,1):
            box('Honed paving slab',((x1+x2)/2,y,.082),((x2-x1),1.992,.076),basalt if route else stone,.004)
    if route:
        for x in (-1.84,1.84):
            box('Inlay groove bed',(x,0,.112),(.024,4,.012),dark,.001)
            box('Flush route conductor',(x,0,.117),(.012,3.996,.006),gold,.001)
            # Ivory index stones interrupt the border only at the module ends.
            for y in (-1.95,1.95):
                box('Route index stone',(x,y,.116),(.085,.028,.008),stone,.001)
    else:
        # Quiet dark perimeter keys orient the paving without decorating every slab.
        for x in (-width/2+.13,width/2-.13):
            for y in (-1.87,1.87):
                box('Paving alignment key',(x,y,.117),(.16,.012,.006),basalt,.001)
                box('Key gold end',(x+.087,y,.117),(.012,.012,.006),gold,.001)
    o=export(name,[width,4,.12])
    manifest[-1]['nominal_dimensions_m']=list(o.dimensions)
    manifest[-1]['collision']='None; deployed over the retained flat floor proxy with top at world Z0'
    manifest[-1]['surface_top_metres']=max(v.co.z for v in o.data.vertices)
    return o

route=paving('SM_Aurelion_KIT_PavingRoute_4m',4,True)
side=paving('SM_Aurelion_KIT_PavingIvory_4m',4)
edge=paving('SM_Aurelion_KIT_PavingEdge_3x4',3)
# A complete 26 m cross-section; each source FBX remains at its origin.
for obj,x in ((route,0),(side,-4),(edge,-11.5)): obj.location.x=x
for source,x in ((side,4),(side,-8),(side,8),(edge,11.5)):
    o=source.copy(); o.data=source.data; scene.collection.objects.link(o); o.location.x=x
scene.world=bpy.data.worlds.new('Paving studio'); scene.world.color=(.1,.1,.1)
for pos,energy,size in (((0,-3,9),2400,9),((-8,1,7),1500,6),((8,1,7),1500,6)):
    data=bpy.data.lights.new('Paving softbox','AREA'); data.energy=energy; data.size=size
    o=bpy.data.objects.new('Paving softbox',data); scene.collection.objects.link(o); o.location=pos
    o.rotation_euler=(Vector((0,0,0))-o.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Paving review'); camera=bpy.data.objects.new('Paving review',data); scene.collection.objects.link(camera)
camera.location=(9,-13,14); camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler()
data.lens=38; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1600; scene.render.resolution_y=900; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'Paving-cross-section.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_PavingKit.blend'))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Room-fit candidate; final surface and live traversal review required',modules=manifest),indent=2))
bpy.ops.render.render(write_still=True)
