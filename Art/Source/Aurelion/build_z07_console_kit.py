"""Authored sloped Aurelion request lecterns inside the measured interaction footprint."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z07ConsoleKit';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.92,.85,.70),0,.22)
p=lens.node_tree.nodes.get('Principled BSDF');p.inputs['Emission Color'].default_value=(.92,.85,.70,1);p.inputs['Emission Strength'].default_value=2
theta=math.atan(.4)
for extra,suffix in [(.31,'Priority')]:
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
    # Raise the optical tray to the priority terminal height; extend its pedestal to the floor.
    for piece in parts:piece.location.z+=.15
    o=export('SM_Aurelion_KIT_Z07PriorityConsole',[.5,.7,1.21]);manifest[-1].update(nominal_dimensions_m=list(o.dimensions),collision='None; existing priority Body remains authoritative',bottom_m=bottom+.15)
priority=o
box('Desk grounding plinth',(0,0,.10),(2,1.5,.20),stone,.014)
box('Desk recessed pedestal',(0,.09,.59),(1.74,1.12,.84),stone,.018)
for x in (-.86,.86):
    box('Desk edge seam',(x,-.477,.58),(.06,.015,.66),dark,.004)
    box('Desk conductor',(x,-.487,.58),(.018,.012,.61),gold,.002)
for x in (-.46,.46):
    box('Recessed cabinet field',(x,-.486,.56),(.68,.020,.53),dark,.006)
    box('Cabinet stone face',(x,-.502,.56),(.60,.014,.45),stone,.006)
    box('Cabinet register',(x,-.514,.70),(.17,.007,.013),gold,.001)
box('Desk upper support',(0,0,1.03),(1.93,1.40,.13),stone,.012)
box('Desk optical surround',(0,0,1.15),(2,1.5,.10),stone,.008)
box('Recessed optical bed',(0,-.04,1.202),(1.72,1.19,.004),dark,.001)
for x in (-.84,.84):box('Optical gold edge',(x,-.04,1.206),(.015,1.15,.004),gold,.001)
for y in (-.60,.52):box('Optical stone edge',(0,y,1.206),(1.68,.018,.006),stone,.001)
for x in (-.49,0,.49):
    box('Data panel',(x,-.09,1.206),(.38,.69,.005),stone,.001)
    for j in range(5):box('Data register',(x,-.31+j*.11,1.211),(.18 if j%2 else .26,.018,.003),gold,.0005)
    box('Panel information light',(x,.37,1.210),(.21,.025,.004),lens,.0005)
desk=export('SM_Aurelion_KIT_Z07ControlDesk',[2,1.5,1.2125]);manifest[-1].update(nominal_dimensions_m=list(desk.dimensions),collision='None; original central desk cube remains authoritative')
priority.location.x=-1.8;priority.location.z=.61;desk.location.x=.6
scene.world=bpy.data.worlds.new('Gallery controls studio');scene.world.color=(.16,.16,.16)
for pos in ((-3,-3,4),(3,2,3)):
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=700;o.data.size=4;o.rotation_euler=(Vector((0,0,.6))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(-4,-5,4));camera=bpy.context.object;camera.rotation_euler=(Vector((-.3,0,.6))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=4.8;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'controls.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Gallery control source; placement and native interaction qualification pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z07-Controls.blend'));bpy.ops.render.render(write_still=True)
