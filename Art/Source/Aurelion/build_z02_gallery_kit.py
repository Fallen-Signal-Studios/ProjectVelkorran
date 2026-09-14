"""Custom relief panels and branching stone piers for Survivor Bend."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z02GalleryKit';ROOT.mkdir(exist_ok=True)

def slab(name,outline,y,depth,mat):
    n=len(outline);verts=[(x,py,z) for py in (y,y+depth) for x,z in outline]
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,n*2))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update();o=bpy.data.objects.new(name,m);scene.collection.objects.link(o);return finish(o,mat,0 if depth<=.025 else .003)

w=3.539034;h=5.253586;depth=.233493
outline=[(-w/2,0),(w/2,0),(w/2,3.51),(1.22,4.47),(0,h),(-1.22,4.47),(-w/2,3.51)]
slab('Relief cut stone backing',outline,-.045,.16,stone)
inner=[(-1.53,.24),(1.53,.24),(1.53,3.46),(1.05,4.26),(0,4.98),(-1.05,4.26),(-1.53,3.46)]
slab('Recessed dark field',inner,-.072,.023,dark)
field=[(-1.42,.34),(1.42,.34),(1.42,3.43),(.96,4.16),(0,4.81),(-.96,4.16),(-1.42,3.43)]
slab('Honed inner stone',field,-.084,.010,stone)
path('Continuous dressed perimeter',[(x,-.108,z) for x,z in inner+[inner[0]]],.11,.030,stone)
path('Perimeter gold fillet',[(x*.97,-.117,z*.98+.04) for x,z in inner+[inner[0]]],.018,.008,gold,bevel=0)
# Nested split diamonds give the relief a focal point; no borrowed faction mark.
for scale,mat,width,y in ((1,dark,.13,-.089),(.92,stone,.082,-.104),(.80,gold,.018,-.117)):
    shape=[(0,3.89),(.78,2.77),(0,1.54),(-.78,2.77),(0,3.89)]
    path('Nested sovereign geometry',[(x*scale,y,2.77+(z-2.77)*scale) for x,z in shape],width,.008,mat,bevel=0)
for side in (-1,1):
    for i in range(7):
        box('Incised register',(side*(.97+.022*i),-.097,.68+i*.27),(.16,.012,.022),dark,.001)
for z in (.21,1.15):
    box('Relief transverse moulding',(0,-.095,z),(3.12,.035,.052),stone,.005)
box('Relief footing',(0,0,.033),(w,depth,.154),stone,.009)
panel=export('SM_Aurelion_KIT_Z02ReliefPanel',[w,depth,h]);manifest[-1]['nominal_dimensions_m']=list(panel.dimensions);manifest[-1]['collision']='None; original relief mesh collision retained';panel.hide_render=True

pw=4.116105;ph=9.812163;pd=.835283
box('Pier core',(0,0,ph/2),(.52,.53,ph),stone,.022)
for x in (-.30,0,.30):
    box('Pier longitudinal flute',(x,-.29,ph/2),(.13,.14,ph-.04),stone,.014)
    for z in (1.2,3.6,6):box('Flute joint',(x,-.366,z),(.10,.012,.024),dark,.001)
for z,height in ((.10,.20),(2.5,.12),(5.0,.12),(7.15,.18),(ph-.12,.24)):
    box('Pier collar',(0,0,z),(.84,pd,height),stone,.012)
    box('Collar front inlay',(0,-pd/2-.003,z),(.72,.006,.024),gold,0)
for side in (-1,1):
    pts=[(side*.26,0,6.45),(side*.43,0,7.24),(side*.87,0,8.16),(side*1.44,0,9.03),(side*(pw/2-.16),0,ph-.17)]
    for dx in (-.10,.10):
        path('Rising stone branch',[(x+dx,-.30,z) for x,y,z in pts],.17,.49,stone)
    path('Branch central reveal',[(x,-.308,z) for x,y,z in pts],.045,.014,dark,bevel=0)
    path('Branch conductor',[(x,-.324,z) for x,y,z in pts],.015,.009,gold,bevel=0)
pier=export('SM_Aurelion_KIT_Z02BranchPier',[pw,pd,ph]);manifest[-1]['nominal_dimensions_m']=list(pier.dimensions)
# The original imported columns have no simple collision. Author convex pieces
# for the shaft, collars and branches, leaving the space below the forks open.
hulls=[]
def hull_box(loc,size):
    o=box(f'UCX_{pier.name}_{len(hulls):02}',loc,size,dark,0);hulls.append(o)
hull_box((0,-.0475,ph/2),(.73,.625,ph))
for z,height in ((.10,.20),(2.5,.12),(5.0,.12),(7.15,.18),(ph-.12,.24)):hull_box((0,0,z),(.84,pd,height))
for side in (-1,1):
    pts=[(side*.26,-.30,6.45),(side*.43,-.30,7.24),(side*.87,-.30,8.16),(side*1.44,-.30,9.03),(side*(pw/2-.16),-.30,ph-.17)]
    for a,b in zip(pts,pts[1:]):hulls.append(path(f'UCX_{pier.name}_{len(hulls):02}',[a,b],.37,.49,dark,bevel=0))
bpy.ops.object.select_all(action='DESELECT')
for o in hulls:o.select_set(True);o.hide_render=True;o.display_type='WIRE'
bpy.context.view_layer.objects.active=hulls[0];bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
pier.select_set(True);bpy.context.view_layer.objects.active=pier
bpy.ops.export_scene.fbx(filepath=str(ROOT/(pier.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
parts.clear();manifest[-1].update(convex_hulls=14,collision='Authored shaft, five collars and eight branch segments',pier_collision_samples=[[0,.1,True],[0,2.5,True],[0,5,True],[0,9.7,True],[.87,8.16,True],[-.87,8.16,True],[1.65,9.31,True],[1,1,False],[.7,4,False],[1.8,7.2,False]])
panel.hide_render=False;panel.location.x=-4.7
scene.world=bpy.data.worlds.new('Gallery studio');scene.world.color=(.15,.15,.15)
for pos in ((-7,-9,10),(7,-6,12)):
    d=bpy.data.lights.new('Gallery softbox','AREA');d.energy=2500;d.size=7;o=bpy.data.objects.new('Gallery softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((-2,0,4))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Gallery review');camera=bpy.data.objects.new('Gallery review',d);scene.collection.objects.link(camera);camera.location=(10,-23,10);camera.rotation_euler=(Vector((-2,0,4.5))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=45;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'Gallery-assembly.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Custom gallery source candidate; final art and live collision correspondence pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_Z02GalleryKit.blend'));bpy.ops.render.render(write_still=True)
