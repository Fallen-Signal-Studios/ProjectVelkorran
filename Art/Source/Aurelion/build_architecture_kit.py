"""Aurelion custom kit: editable detailed architectural modules, metres, bottom pivots.

Blender 4.5 CLI. Source/prototype stage; in-engine materials and placement require review.
"""
import bpy
import json
import math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parent / 'ArchitectureKit'
ROOT.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
def material(name, color, metal, rough):
    m = bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Metallic'].default_value=metal; p.inputs['Roughness'].default_value=rough
    return m
stone=material('M_Aurelion_IvoryStone',(.64,.61,.53),0,.37)
gold=material('M_Aurelion_AncientGold',(.46,.25,.065),.85,.29)
dark=material('M_Aurelion_ChannelShadow',(.018,.025,.031),.6,.3)
# Studio material variation; not represented as an exported texture claim.
p=stone.node_tree.nodes.get('Principled BSDF'); n=stone.node_tree.nodes.new('ShaderNodeTexNoise')
n.inputs['Scale'].default_value=95; n.inputs['Detail'].default_value=3
b=stone.node_tree.nodes.new('ShaderNodeBump'); b.inputs['Strength'].default_value=.16; b.inputs['Distance'].default_value=.001
stone.node_tree.links.new(n.outputs['Fac'],b.inputs['Height']); stone.node_tree.links.new(b.outputs['Normal'],p.inputs['Normal'])
parts=[]; manifest=[]; modules=[]
def finish(obj,mat,bevel=.008):
    obj.data.materials.append(mat)
    if bevel:
        mod=obj.modifiers.new('Edge highlight radius','BEVEL'); mod.width=bevel; mod.segments=3
        bpy.context.view_layer.objects.active=obj; bpy.ops.object.modifier_apply(modifier=mod.name)
        mod=obj.modifiers.new('Weighted face normals','WEIGHTED_NORMAL'); mod.keep_sharp=True
        bpy.ops.object.modifier_apply(modifier=mod.name)
    parts.append(obj); return obj
def box(name,loc,size,mat=stone,bevel=.008):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); o=bpy.context.object; o.name=name; o.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    return finish(o,mat,bevel)
def path(name,points,width,depth,mat,bevel=None):
    # Joined mitered polygonal moulding in X/Z, extruded along Y.
    closed=(Vector(points[0])-Vector(points[-1])).length<1e-6
    if closed: points=points[:-1]
    front=[]; back=[]
    for i,(x,y,z) in enumerate(points):
        prev=Vector(points[(i-1)%len(points) if closed else max(i-1,0)])
        nxt=Vector(points[(i+1)%len(points) if closed else min(i+1,len(points)-1)])
        d=(nxt-prev).normalized(); normal=Vector((-d.z,0,d.x))
        for sign in (-1,1):
            v=Vector((x,y,z))+normal*width*.5*sign
            front.append(tuple(v)); back.append((v.x,v.y+depth,v.z))
    count=len(front); vertices=front+back; faces=[]
    for i in range(len(points) if closed else len(points)-1):
        a=2*i; b=2*((i+1)%len(points))
        faces += [(a,b,b+1,a+1),(a+count+1,b+count+1,b+count,a+count),
            (a,a+count,b+count,b),(a+1,b+1,b+count+1,a+count+1)]
    if not closed: faces += [(0,1,count+1,count),(count-2,2*count-2,2*count-1,count-1)]
    mesh=bpy.data.meshes.new(name); mesh.from_pydata(vertices,[],faces); mesh.update()
    o=bpy.data.objects.new(name,mesh); scene.collection.objects.link(o)
    return finish(o,mat,min(width*.15,.006) if bevel is None else bevel)
def ring(name,x,y,z,radius,width,mat):
    points=[(x+math.sin(a)*radius,y,z+math.cos(a)*radius) for a in [i*2*math.pi/96 for i in range(97)]]
    path(name,points,width,.024,mat)
def export(name,dimensions,collision_points=None):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]; bpy.ops.object.join(); o=bpy.context.object; o.name=name
    scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.uv.smart_project(island_margin=.008); bpy.ops.object.mode_set(mode='OBJECT')
    # UV0 uses one tile per metre on every face. UV1 keeps unique packed islands.
    uv=o.data.uv_layers.active; uv.name='Surface_1m'
    packed=o.data.uv_layers.new(name='Lightmap_Unique',do_init=True)
    for poly in o.data.polygons:
        axis=max(range(3),key=lambda i: abs(poly.normal[i]))
        axes=((1,2),(0,2),(0,1))[axis]
        for loop in poly.loop_indices:
            co=o.data.vertices[o.data.loops[loop].vertex_index].co
            uv.data[loop].uv=(co[axes[0]],co[axes[1]])
    o.data.uv_layers.active_index=0
    collision=[]
    if collision_points:
        # One convex hull per frame segment leaves the actual doorway open.
        for i in range(len(collision_points)-1):
            verts=[]
            for j in (i,i+1):
                p=Vector(collision_points[j]); prev=Vector(collision_points[max(j-1,0)])
                nxt=Vector(collision_points[min(j+1,len(collision_points)-1)])
                d=(nxt-prev).normalized(); n=Vector((-d.z,0,d.x))*.5
                verts.extend([tuple(p-n),tuple(p+n),tuple(p-n+Vector((0,1,0))),tuple(p+n+Vector((0,1,0)))])
            data=bpy.data.meshes.new('Convex frame segment'); data.from_pydata(verts,[],[(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3),(0,1,3,2),(4,6,7,5)])
            hull=bpy.data.objects.new('UCX_'+name+'_'+str(i).zfill(2),data); scene.collection.objects.link(hull)
            hull.hide_render=True; hull.display_type='WIRE'; hull.select_set(True); collision.append(hull)
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},
        axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    o.data.calc_loop_triangles()
    manifest.append(dict(asset=name,triangles=len(o.data.loop_triangles),nominal_dimensions_m=dimensions,
        materials=[m.name for m in o.data.materials],uv_layers=len(o.data.uv_layers),
        convex_hulls=len(collision),collision='Segmented frame hulls' if collision else 'Not authored; no gameplay placement until collision is verified'))
    modules.append(o); parts.clear(); return o

# Four metre bay: deep stone assembly, fitted masonry, inset perimeter, functional central channel.
box('Continuous structural backing',(0,.29,3.26),(3.98,.16,6.28),stone,.006)
for x in (-1.02,1.02):
    for j in range(4):
        box('Cut ashlar face', (x,.02,.9+j*1.5),(1.9,.5,1.48),stone,.018)
for x in (-1.92,1.92):
    box('Outer seam shadow',(x,-.27,3.3),(.14,.045,6.27),dark)
    box('Stone arris',(x,-.32,3.3),(.075,.11,6.3),stone)
for z,w,d,h in ((.08,4,.7,.16),(.22,3.932,.62,.1),(6.38,3.96,.62,.12),(6.53,4,.78,.18),(6.69,3.98,.72,.12),(6.86,4,.85,.22)):
    box('Stepped entablature' if z>6 else 'Stepped socle',(0,0,z),(w,d,h),stone,.015)
for side in (-1,1):
    pts=[(side*1.72,-.3,.42),(side*1.72,-.3,4.75),(side*.95,-.3,5.56),(side*.95,-.3,6.23)]
    path('Deep oblique reveal',pts,.18,.05,dark)
    path('Functional gold channel',[(x,y-.04,z) for x,y,z in pts],.042,.03,gold)
    for j in range(9):
        box('Recessed service louver',(side*1.48,-.287,.56+j*.047),(.26,.04,.017),gold,.003)
    for z in (1.35,3.6):
        box('Inset stone plaque',(side*.87,-.27,z),(1.31,.075,1.83),stone,.025)
        for dx in (-.59,.59):
            box('Plaque incision',(side*.87+dx,-.312,z),(.012,.008,1.63),dark,.002)
box('Axial mechanism channel',(0,-.295,3.28),(.16,.07,5.88),dark)
for x in (-.065,.065): box('Axial rail',(x,-.348,3.28),(.016,.03,5.88),gold,.003)
# Save a plain companion bay before adding the rare mechanism register.
original_parts=list(parts); parts.clear()
for original in original_parts:
    duplicate=original.copy(); duplicate.data=original.data.copy(); scene.collection.objects.link(duplicate); parts.append(duplicate)
plain=export('SM_Aurelion_KIT_WallPlain_4x7',[4,.85,7]); plain.hide_render=True
parts.extend(original_parts)
for radius,width,mat in ((.57,.11,stone),(.48,.035,gold),(.405,.025,dark),(.31,.024,gold)):
    ring('Concurrence register',0,-.39,5.37,radius,width,mat)
for i in range(12):
    a=i*math.pi/6; x=math.sin(a)*.48; z=5.37+math.cos(a)*.48
    o=box('Register index',(x,-.429,z),(.022,.03,.072),gold,.003); o.rotation_euler[1]=a
wall=export('SM_Aurelion_KIT_WallBay_4x7',[4,.85,7])

# Independent buttressed pier; layered side returns read in oblique gameplay views.
for z,w,d,h in ((.11,1.2,1.02,.22),(.31,1.1,.92,.18),(3.35,.76,.68,5.9),(6.39,.94,.82,.18),(6.58,1.08,.95,.2),(6.84,1.2,1.04,.32)):
    box('Pier stone course',(0,0,z),(w,d,h),stone,.018)
for x in (-.32,.32):
    box('Pier recessed flute',(x,-.352,3.3),(.072,.035,5.65),dark)
    box('Pier gold spine',(x,-.38,3.3),(.022,.035,5.6),gold,.004)
for z in (.65,2.4,4.3,6.03):
    box('Pier collar',(0,-.04,z),(.81,.76,.085),stone,.009)
    box('Collar metal edge',(0,-.43,z),(.66,.025,.018),gold,.003)
pier=export('SM_Aurelion_KIT_Pier_1x7',[1.2,1.04,7])

# Separate cornice and floor modules permit straight, corner and tiered compositions.
for z,w,d,h in ((.08,4,.46,.16),(.22,4,.6,.12),(.38,4,.76,.2),(.53,4,.68,.1)):
    box('Cornice stone step',(0,0,z),(w,d,h),stone,.012)
box('Cornice gold seam',(0,-.388,.38),(3.98,.025,.028),gold,.004)
for i in range(20): box('Cornice recessed dentil',(-1.9+i*.2,-.285,.205),(.07,.1,.085),dark,.005)
cornice=export('SM_Aurelion_KIT_Cornice_4m',[4,.785,.58])
for x in (-1,1):
    for y in (-1,1): box('Floor ashlar',(x,y,.06),(1.988,1.988,.12),stone,.006)
for x in (-1.78,1.78):
    box('Floor channel',(x,0,.122),(.075,3.92,.008),dark,.002)
    box('Floor inlay',(x,0,.128),(.022,3.9,.006),gold,.001)
floor=export('SM_Aurelion_KIT_Floor_4m',[4,4,.131])

portal_points=[(-3.5,-.5,0),(-3.5,-.5,5.2),(-2.4,-.5,6.3),(0,-.5,7.3),
               (2.4,-.5,6.3),(3.5,-.5,5.2),(3.5,-.5,0)]
path('Portal structural voussoirs',portal_points,1,1,stone)
path('Portal inset reveal',[(x,y-.012,z) for x,y,z in portal_points],.36,.035,dark)
path('Portal functional rail',[(x,y-.052,z) for x,y,z in portal_points],.045,.035,gold)
for x in (-3.5,3.5):
    box('Portal foot',(x,0,.12),(1,1.3,.24),stone,.018)
    for z in (.36,2.2,4.2):
        box('Portal collar',(x,0,z),(1,1.14,.12),stone,.014)
        box('Portal collar inlay',(x,-.581,z),(.82,.025,.022),gold,.003)
portal=export('SM_Aurelion_KIT_Portal_6m',[8,1.3,7.8],portal_points)
portal.hide_render=True

# Assembly preview only; FBX exports above remain individual origin-centred assets.
wall.location.x=-2; plain.location.x=2; plain.hide_render=False
for x in (-4,0,4):
    o=pier.copy(); o.data=pier.data; scene.collection.objects.link(o); o.location=(x,-.15,0)
pier.hide_render=True; cornice.hide_render=True
floor.location=(-2,-2,0); o=floor.copy(); o.data=floor.data; scene.collection.objects.link(o); o.location=(2,-2,0)
scene.world=bpy.data.worlds.new('Studio'); scene.world.color=(.09,.09,.09)
for pos,energy,size in (((-5,-6,10),2300,7),((5,-3,6),1300,5),((0,3,9),1800,4)):
    data=bpy.data.lights.new('Studio area','AREA'); data.energy=energy; data.shape='DISK'; data.size=size
    o=bpy.data.objects.new('Studio area',data); scene.collection.objects.link(o); o.location=pos
    o.rotation_euler=(Vector((0,0,3))-o.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Architectural review'); camera=bpy.data.objects.new('Architectural review',data); scene.collection.objects.link(camera)
camera.location=(10,-16,7.2); camera.rotation_euler=(Vector((0,0,3.1))-camera.location).to_track_quat('-Z','Y').to_euler(); data.lens=48; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=40; scene.cycles.use_denoising=True
scene.render.resolution_x=1800; scene.render.resolution_y=1400; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'ArchitectureKit-assembly.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_ArchitectureKit.blend'))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='source prototype; quality and in-engine acceptance pending',modules=manifest),indent=2))
bpy.ops.render.render(write_still=True)
