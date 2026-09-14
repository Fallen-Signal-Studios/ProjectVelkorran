"""Measured Aurelion stone table and seat modules with authored convex collision."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z02FurnitureKit';ROOT.mkdir(exist_ok=True)

def cylinder(name,loc,radius,depth,mat=stone,vertices=8,bevel=.006):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc,rotation=(0,0,math.pi/8));o=bpy.context.object;o.name=name
    return finish(o,mat,bevel)

def export_furniture(name,hulls_spec,samples):
    o=export(name,[1,1,1]);spec=manifest[-1];spec['nominal_dimensions_m']=list(o.dimensions);hulls=[]
    for i,(kind,loc,size) in enumerate(hulls_spec):
        name_h=f'UCX_{name}_{i:02}'
        h=cylinder(name_h,loc,size[0],size[1],dark,8,0) if kind=='octagon' else box(name_h,loc,size,dark,0)
        hulls.append(h);h.hide_render=True;h.display_type='WIRE'
    bpy.ops.object.select_all(action='DESELECT')
    for h in hulls:h.select_set(True)
    bpy.context.view_layer.objects.active=hulls[0];bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    parts.clear();spec.update(convex_hulls=len(hulls),furniture_surface_samples=samples,collision='Authored base, support and top; tabletop service items are cosmetic')
    o.hide_render=True;return o

cylinder('Table dressed base',(0,0,.06),.39,.12)
cylinder('Base separation course',(0,0,.137),.345,.034,dark)
cylinder('Fluted pedestal',(0,0,.407),.26,.51)
for i in range(8):
    angle=math.pi/8+i*math.pi/4
    o=box('Pedestal carved flute',(.246*math.cos(angle),.246*math.sin(angle),.407),(.045,.025,.39),dark,.003);o.rotation_euler.z=angle-math.pi/2
cylinder('Table capital',(0,0,.667),.38,.05)
cylinder('Table undercut reveal',(0,0,.706),.615,.028,dark)
cylinder('Beveled table slab',(0,0,.7615),.68,.083)
cylinder('Recessed octagonal field',(0,0,.805),.52,.004,dark,8,.001)
cylinder('Honed service surface',(0,0,.808),.505,.004,stone,8,.001)
for i in range(8):
    angle=i*math.pi/4
    o=box('Table rim conductor',(.58*math.cos(angle),.58*math.sin(angle),.805),(.12,.012,.004),gold,0);o.rotation_euler.z=angle+math.pi/2
for x,y,angle in ((-.24,-.18,.15),(.24,.14,-.2)):
    o=box('Stone service tablet',(x,y,.828),(.23,.32,.035),stone,.009);o.rotation_euler.z=angle
    o=box('Tablet register',(x,y,.848),(.17,.24,.005),dark,.001);o.rotation_euler.z=angle
cylinder('Service vessel',(0,.29,.91),.07,.20,stone,48,.004)
cylinder('Vessel recessed cap',(0,.29,1.013),.064,.006,dark,48,.001)
cylinder('Vessel crown',(0,.29,1.021),.045,.008,gold,48,.001)
table=export_furniture('SM_Aurelion_KIT_Z02ServiceTable',[
    ('octagon',(0,0,.06),(.39,.12)),('octagon',(0,0,.417),(.29,.594)),('octagon',(0,0,.752),(.68,.116))],[[0,0,.81],[.40,0,.81],[0,.4,.81],[.9,0,None]])

box('Seat dressed footing',(0,0,.055),(.46,.48,.11),stone,.012)
box('Seat isolation joint',(0,0,.124),(.39,.41,.028),dark,.004)
box('Seat pedestal',(0,0,.26),(.32,.34,.244),stone,.014)
for side in (-1,1):
    box('Seat inset panel',(side*.162,0,.257),(.008,.23,.15),dark,.002)
    box('Seat inner field',(side*.168,0,.257),(.005,.19,.115),stone,.002)
box('Seat upper reveal',(0,0,.384),(.435,.455,.025),dark,.004)
box('Honed seat',(0,0,.4205),(.48,.50,.061),stone,.013)
for side in (-1,1):box('Seat narrow conductor',(side*.19,0,.451),(.012,.36,.004),gold,0)
seat=export_furniture('SM_Aurelion_KIT_Z02StoneSeat',[
    ('box',(0,0,.055),(.46,.48,.11)),('box',(0,0,.247),(.34,.36,.274)),('box',(0,0,.417),(.48,.50,.072))],[[0,0,.453],[.15,.15,.453],[.4,0,None]])
table.hide_render=False;seat.hide_render=False;seat.location=(-.87,0,0)
for x,y in ((.87,0),(-.35,1.22),(0,-.9)):
    o=seat.copy();o.data=seat.data;scene.collection.objects.link(o);o.location=(x,y,0)
scene.world=bpy.data.worlds.new('Furniture studio');scene.world.color=(.16,.16,.16)
for pos in ((-3,-4,6),(3,2,4)):
    d=bpy.data.lights.new('Furniture softbox','AREA');d.energy=500;d.size=4;o=bpy.data.objects.new('Furniture softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,.4))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Furniture review');camera=bpy.data.objects.new('Furniture review',d);scene.collection.objects.link(camera);camera.location=(3,-5,3.4);camera.rotation_euler=(Vector((0,0,.4))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=48;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1500;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'Furniture-assembly.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Custom modular furniture candidate; live navigation and final art pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_Z02FurnitureKit.blend'));bpy.ops.render.render(write_still=True)
