"""Full-size approach bridge candidate: fitted deck and articulated arch ribs."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(globals().get('KIT_ROOT',Path(__file__).resolve().parent/'ApproachBridgeKit'));ROOT.mkdir(parents=True,exist_ok=True)
profile=json.loads((Path(__file__).resolve().parent/'ApproachBridgeKit/measured-profile.json').read_text())
W=14.52963488;L=float(globals().get('SPAN_LENGTH',21.29522470));TOP=.11133914
mesh_prefix=globals().get('MESH_PREFIX','Approach')
hulls=[]

def prism(name,x,width,y0,y1,z0,z1,top,mat=stone,bevel=.006):
    verts=[(px,y,z) for y,bottom in ((y0,z0),(y1,z1)) for z in (bottom,top) for px in (x-width/2,x+width/2)]
    faces=[(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3),(0,1,3,2),(4,6,7,5)]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);return finish(o,mat,bevel)

def export_collision(visual,specs,samples):
    objects=[]
    for i,(loc,size) in enumerate(specs):
        o=box('UCX_'+visual.name+'_'+str(i).zfill(2),loc,size,dark,0)
        bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        o.hide_render=True;o.display_type='WIRE';objects.append(o)
    parts.clear();bpy.ops.object.select_all(action='DESELECT');visual.select_set(True)
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=visual
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(visual.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    manifest[-1].update(convex_hulls=len(objects),collision='One deck slab and two solid outer parapets; central route guards are separate retained actors',bridge_surface_samples=samples)

# Structural slab beneath the visible wearing course, with one continuous collision top.
box('Continuous structural deck',(0,0,TOP-.202),(W,L,.396),stone,.01)
for ix in range(8):
    x=-W/2+(ix+.5)*W/8
    for iy in range(12):
        y=-L/2+(iy+.5)*L/12
        box('Honed paving course',(x,y,TOP-.022),(W/8-.006,L/12-.006,.044),stone,.002)
for x in (-2.72,2.72):
    box('Route inlay bed',(x,0,TOP-.003),(.05,L-.04,.006),dark,0)
    box('Narrow route conductor',(x,0,TOP-.001),(.013,L-.05,.002),gold,0)
for side in (-1,1):
    x=side*(W/2-.16)
    # The solid guard deliberately makes collision visually legible along its full length.
    box('Outer parapet core',(x,0,TOP+.65),(.19,L-.32,1.10),stone,.008)
    for z,width,height in ((.10,.32,.20),(1.19,.32,.22)):
        box('Parapet dressed course',(x,0,TOP+z),(width,L-.32,height),stone,.01)
    for y in (-L/2+.08,L/2-.08):
        box('Parapet terminal pier',(x,y,TOP+.65),(.32,.16,1.30),stone,.008)
    for i in range(12):
        y=-L/2+(i+.5)*L/12
        for face in (-1,1):
            fx=x+face*.106
            box('Recessed parapet panel',(fx,y,TOP+.65),(.022,L/12-.20,.72),dark,.002)
            box('Raised panel stone',(fx+face*.016,y,TOP+.65),(.016,L/12-.28,.64),stone,.003)
            box('Panel axial inlay',(fx+face*.027,y,TOP+.65),(.004,.016,.29),gold,0)
    # Profiled deck edge and corbels remain below the walking surface.
    for z,width,height in ((-.10,.37,.13),(-.26,.29,.11),(-.40,.23,.12)):
        box('Dressed bridge fascia',(side*(W/2-.19),0,TOP+z),(width,L-.02,height),stone,.008)
    for i in range(13):
        y=-L/2+(i+.5)*L/13
        box('Fascia corbel',(side*(W/2-.22),y,TOP-.53),(.35,.24,.22),stone,.018)
deck=export('SM_Aurelion_KIT_'+mesh_prefix+'Deck',[]);manifest[-1]['nominal_dimensions_m']=list(deck.dimensions)
rail=W/2-.16
specs=[((0,0,TOP-.20),(W,L,.40))]+[((x,0,TOP+.65),(.32,L,1.30)) for x in (-rail,rail)]
samples=[(x,y,TOP if abs(x)<7 else TOP+1.30) for x in (-rail,-6,-2,0,2,6,rail) for y in (-min(10,L/2-.1),-5,0,5,min(10,L/2-.1))]
export_collision(deck,specs,samples)
deck.hide_render=True

# Use the sampled original arch curve, extending measured ends to the deck footprint.
curve=sorted((v['y']*L/21.29522470,v['z']) for v in profile['underside'] if abs(v['x'])<.01)
assert len(curve)==41
curve=[(-L/2,-49.14)]+curve+[(L/2,-49.14)]
rib_x=(-6.45,-2.15,2.15,6.45)
joint_vertices=[];joint_faces=[]
for x in rib_x:
    for i,((y0,z0),(y1,z1)) in enumerate(zip(curve,curve[1:])):
        # Independent voussoir segments give the full support an authored construction rhythm.
        gap=.008;slope=(z1-z0)/(y1-y0)
        a=y0+gap;b=y1-gap;za=min(z0+gap*slope,-.50);zb=min(z1-gap*slope,-.50)
        prism('Arch rib voussoir',x,1.14,a,b,za,zb,TOP-.39,stone,.006)
        # Narrow carved archivolt follows the underside on both visible rib faces.
        for face in (-1,1):
            o=path('Archivolt edge',[(a,0,za+.13),(b,0,zb+.13)],.14,.045,stone,0)
            o.rotation_euler[2]=math.pi/2;o.location.x=x+face*.59
    # Horizontal coursing across the spandrels breaks up otherwise fifty-metre strips.
    for z in (-1.5-1.5*j for j in range(31)):
        for (y0,z0),(y1,z1) in zip(curve,curve[1:]):
            if max(z0,z1)>z-.2:continue
            for face in (-1,1):
                j=len(joint_vertices);fx=x+face*.572
                joint_vertices.extend([(fx,y0+.008,z-.006),(fx,y1-.008,z-.006),(fx,y1-.008,z+.006),(fx,y0+.008,z+.006)]);joint_faces.append((j,j+1,j+2,j+3))
    for y in (-L/2+.40,L/2-.40):
        box('Abutment recessed face',(x,y,-24.9),(1.02,.80,47.4),stone,.015)
        for z in (-47,-39,-31,-23,-15,-7):box('Abutment collar',(x,y,z),(1.35,.85,.22),stone,.012)
for y in (-L/2+.44,L/2-.44):
    for z in (-48.9,-.63):box('Cross-span tie beam',(0,y,z),(13.95,.88,.45),stone,.015)
joint_mesh=bpy.data.meshes.new('Spandrel coursing');joint_mesh.from_pydata(joint_vertices,[],joint_faces);joint_mesh.update();joint_object=bpy.data.objects.new('Spandrel coursing',joint_mesh);scene.collection.objects.link(joint_object);finish(joint_object,dark,0)
support=export('SM_Aurelion_KIT_'+mesh_prefix+'ArchSupports',[]);manifest[-1]['nominal_dimensions_m']=list(support.dimensions)
support_hulls=[]
for x in rib_x:
    for (y0,z0),(y1,z1) in zip(curve,curve[1:]):
        h=prism('UCX_'+support.name+'_'+str(len(support_hulls)).zfill(3),x,1.14,y0,y1,min(z0,-.50),min(z1,-.50),TOP-.39,dark,0)
        support_hulls.append(h)
    for y in (-L/2+.40,L/2-.40):
        h=box('UCX_'+support.name+'_'+str(len(support_hulls)).zfill(3),(x,y,-24.9),(1.02,.80,47.4),dark,0);support_hulls.append(h)
for y in (-L/2+.44,L/2-.44):
    for z in (-48.9,-.63):
        h=box('UCX_'+support.name+'_'+str(len(support_hulls)).zfill(3),(0,y,z),(13.95,.88,.45),dark,0);support_hulls.append(h)
parts.clear();bpy.ops.object.select_all(action='DESELECT')
for h in support_hulls:
    h.select_set(True);bpy.context.view_layer.objects.active=h;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);h.select_set(False);h.hide_render=True;h.display_type='WIRE'
support.select_set(True)
for h in support_hulls:h.select_set(True)
bpy.context.view_layer.objects.active=support
bpy.ops.export_scene.fbx(filepath=str(ROOT/(support.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
manifest[-1].update(convex_hulls=len(support_hulls),collision='Segmented arch ribs, abutments and cross ties; open space between ribs retained',support_lateral_samples=[(y,z,hit) for y,z,hit in [(0,-.40,True),(0,-2,False),(8,-15,True),(8,-35,False),(-8,-15,True),(-8,-35,False)]])
deck.hide_render=False
scene.world=bpy.data.worlds.new('Approach bridge studio');scene.world.color=(.16,.16,.16)
for pos,power,size in (((-20,-20,30),28000,20),((20,15,-5),18000,18)):
    d=bpy.data.lights.new('Bridge softbox','AREA');d.energy=power;d.size=size;o=bpy.data.objects.new('Bridge softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,-8))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Approach bridge review');camera=bpy.data.objects.new('Approach bridge review',d);scene.collection.objects.link(camera);scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
for name,pos,target,lens in [('Bridge', (60,-70,8),(0,0,-24),40),('Deck',(18,-23,11),(0,0,0),36)]:
    camera.location=pos;camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=lens;scene.render.filepath=str(ROOT/(name+'.png'));bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/('Aurelion_'+mesh_prefix+'BridgeKit.blend')))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Full-scale source candidate; Unreal fit, underbridge collision and live route acceptance pending',modules=manifest),indent=2))
