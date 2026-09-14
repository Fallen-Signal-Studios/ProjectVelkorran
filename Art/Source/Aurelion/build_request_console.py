"""Authored sloped Aurelion request lecterns inside the measured interaction footprint."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'RequestConsoleKit';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.92,.85,.70),0,.22)
p=lens.node_tree.nodes.get('Principled BSDF');p.inputs['Emission Color'].default_value=(.92,.85,.70,1);p.inputs['Emission Strength'].default_value=2
theta=math.atan(.4)
for extra,suffix in [(0,''),(.2,'RaisedBase')]:
    bottom=-.45-extra-(.02 if not extra else 0)
    box('Grounding socle',(0,0,(bottom-.37)/2),(.50,.70,-.37-bottom),stone,.012)
    box('Inset pedestal',(0,0,-.055),(.31,.43,.63),stone,.012)
    for y in (-.221,.221):
        box('Service recess',(0,y,-.08),(.20,.012,.45),dark,.003)
        box('Vertical conductor',(0,y*1.04,-.08),(.025,.008,.40),gold,.002)
    box('Foot collar',(0,0,-.34),(.40,.54,.06),stone,.009)
    box('Upper bearing',(0,0,.22),(.38,.57,.065),stone,.009)
    verts=[(x,y,z+.4*x) for z in (.23,.35) for y in (-.35,.35) for x in (-.25,.25)]
    faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
    data=bpy.data.meshes.new('Sloped optical tray');data.from_pydata(verts,[],faces);data.update();o=bpy.data.objects.new('Sloped optical tray',data);scene.collection.objects.link(o);finish(o,stone,.007)
    # All screen fields lie on the same inclined plane and stay inside the tray envelope.
    def field(name,x,y,sx,sy,mat,z=.355,depth=.006):
        o=box(name,(x,y,z+.4*x),(sx,sy,depth),mat,min(depth*.25,.002));o.rotation_euler.y=-theta;return o
    field('Recessed glass',0,0,.39,.59,dark)
    for y in (-.30,.30):field('Optical retaining rail',0,y,.41,.014,gold,z=.36)
    for x in (-.20,.20):field('Optical end rail',x,0,.012,.60,gold,z=.36)
    for y in (-.18,0,.18):
        field('Touch field',-.07,y,.15,.12,stone,z=.362)
        field('Touch glyph',-.07,y,.025,.045,gold,z=.368,depth=.004)
    for i in range(5):field('Information register',.11,-.18+i*.09,.028,.060,lens,z=.362,depth=.004)
    # Keep the narrower original visual footprint beside the rescue guide jamb.
    for piece in parts:piece.location.y*=.7;piece.scale.y*=.7
    o=export('SM_Aurelion_KIT_RequestConsole'+suffix,[.5,.49,.9+extra]);manifest[-1].update(nominal_dimensions_m=list(o.dimensions),collision='None; existing request Body remains authoritative',bottom_m=bottom)
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; interaction and scene fit pending',modules=manifest),indent=2))
for i,o in enumerate(modules):o.location.y=(i-.5)*1.0
scene.world=bpy.data.worlds.new('Console studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((-3,-3,4),500,3),((3,2,2),400,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(-2.6,-3.4,2));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,-.1))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=2.4;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'request-consoles.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-RequestConsoles.blend'));bpy.ops.render.render(write_still=True)
print('REQUEST_CONSOLE_BUILD_PASS')
