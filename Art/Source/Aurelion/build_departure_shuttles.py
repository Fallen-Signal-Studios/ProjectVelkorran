"""Author two faction shuttles; Blender 4.5 CLI. Metres, bottom pivot, nose -Y.

Static exterior dressing only: no interior, flight rig or gameplay collision.
Layout plan p20 governs faction palette and silhouette, not approved final kit.
"""
import bpy
import json
import hashlib
import math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parent

def build(faction):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    dominion = faction == 'Dominion'
    name = 'SM_Aurelion_' + faction + '_Shuttle'
    specs = {
        'Armor': ((.095,.015,.020) if dominion else (.023,.039,.064), .65, .39, 0),
        'Trim': ((.38,.205,.066) if dominion else (.26,.32,.37), .82, .28, 0),
        'Structure': ((.025,.031,.038), .65, .42, 0),
        'Glass': ((.012,.028,.039), .78, .16, 0),
        'Signal': ((1,.40,.07) if dominion else (.33,.7,1), .2, .3, 2),
    }
    mats = {}
    for key,(rgb,metal,rough,emission) in specs.items():
        m=bpy.data.materials.new('M_'+faction+'_'+key)
        m.diffuse_color=(*rgb,1); m.use_nodes=True
        shader=m.node_tree.nodes.get('Principled BSDF')
        for socket,value in [('Base Color',(*rgb,1)),('Metallic',metal),('Roughness',rough),('Emission Color',(*rgb,1)),('Emission Strength',emission)]:
            shader.inputs[socket].default_value=value
        mats[key]=m
    parts=[]
    def finish(obj,label,mat,bevel=.04):
        obj.name=label; obj.data.materials.append(mats[mat])
        if bevel:
            mod=obj.modifiers.new('Armor edge','BEVEL'); mod.width=bevel; mod.segments=2
            bpy.context.view_layer.objects.active=obj
            bpy.ops.object.modifier_apply(modifier=mod.name)
            obj.modifiers.new('Weighted face normals','WEIGHTED_NORMAL')
        parts.append(obj); return obj
    def box(label,loc,size,mat,bevel=.04):
        bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
        obj=bpy.context.object; obj.dimensions=size
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        return finish(obj,label,mat,bevel)
    def cylinder(label,loc,radius,depth,mat,rotation=(0,0,0),vertices=16):
        bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc,rotation=rotation)
        return finish(bpy.context.object,label,mat,.008)
    def loft(label,sections,mat):
        # Eight-sided cross sections (Y, half-width, bottom, top).
        verts=[]
        for y,w,b,t in sections:
            h=t-b
            verts += [(-w*.65,y,b), (w*.65,y,b), (w,y,b+h*.24), (w,y,b+h*.72), (w*.62,y,t),(-w*.62,y,t),(-w,y,b+h*.72),(-w,y,b+h*.24)]
        faces=[tuple(reversed(range(8))),tuple(range(len(verts)-8,len(verts)))]
        for i in range(len(sections)-1):
            for j in range(8): faces.append((i*8+j,i*8+(j+1)%8,(i+1)*8+(j+1)%8,(i+1)*8+j))
        mesh=bpy.data.meshes.new(label); mesh.from_pydata(verts,[],faces); mesh.update()
        obj=bpy.data.objects.new(label,mesh); scene.collection.objects.link(obj)
        return finish(obj,label,mat,.035)
    def prism(label,outline,z,thickness,mat):
        verts=[(x,y,z+d) for d in (0,thickness) for x,y in outline]; n=len(outline)
        faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
        faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        mesh=bpy.data.meshes.new(label); mesh.from_pydata(verts,[],faces); mesh.update()
        obj=bpy.data.objects.new(label,mesh); scene.collection.objects.link(obj)
        return finish(obj,label,mat)
    if dominion:
        loft('Armored troop cabin',[(-7.72,.18,1.87,2.18),(-7.3,.55,1.67,2.48),(-4.1,2.15,1.1,4.2),(3.5,2.3,1.1,4.3),(6,1.8,1.35,3.5)],'Armor')
        loft('Recessed command canopy',[(-6.68,.44,2.91,3.15),(-5.15,1.02,3.46,3.76),(-4.02,1.39,4.04,4.42),(-2.8,1.43,4.11,4.49)],'Glass')
        box('Bronze command spine',(0,.9,4.37),(.44,7.3,.18),'Trim')
    else:
        loft('Slender transport capsule',[(-7.86,.15,1.83,2.06),(-7.44,.34,1.60,2.25),(-4.1,1.55,1.0,3.2),(3.9,1.65,1.1,3.6),(6.2,1.2,1.4,3.1)],'Armor')
        loft('Continuous pilot canopy',[(-6.92,.32,2.57,2.78),(-5.12,.71,3.12,3.41),(-3.78,1.08,3.34,3.65),(-1.8,1.08,3.38,3.76)],'Glass')
        box('Equipment spine',(0,2.4,3.67),(.8,5.1,.22),'Trim')
    # The initial flat prow looked like an uncapped graybox from dock level.
    # A narrower hull cap and visible inset instrument band give it a real nose.
    prow_y=-7.76 if dominion else -7.90
    box('Prow sensor shadow',(0,prow_y-.024,2.02),(.30,.035,.13),'Structure',.014)
    box('Prow sensor lens',(0,prow_y-.047,2.02),(.19,.012,.045),'Signal',.005)
    for prow_side in (-1,1):
        box('Prow armored cheek',(prow_side*.38,prow_y+.19,2.21),(.21,.33,.065),'Trim',.013)
    for side in (-1,1):
        if dominion:
            outline=[(side*1.8,-2.6),(side*4.2,-2),(side*8.3,3.5),(side*8.3,5.4),(side*2,4.5)]
            prism('Swept armored wing',outline,2.0,.38,'Armor')
            prism('Bronze wing shoulder',[(side*2,-1.7),(side*2.5,-1.5),(side*7.8,4.5),(side*7.1,4.5)],2.40,.07,'Trim')
            px=5.6; front=-1.6; rear=6.6
        else:
            outline=[(side*1.3,-3.3),(side*2.4,-3.1),(side*7.3,1),(side*7.7,3.8),(side*1.5,2.6)]
            prism('Swept modular outrigger',outline,1.7,.24,'Trim')
            px=5.9; front=-3; rear=5.8
            for y in (-1.7,.1,1.9):
                box('Exposed outrigger coupling',(side*3.05,y,2.0),(2.0,.14,.18),'Structure')
        pod=loft('Independent engine nacelle',[(front,.55,1.35,2.7),(front+1, .92,1.1,3.3),(rear-.6,.92,1.1,3.3),(rear,.7,1.35,2.9)],'Armor')
        pod.location.x=side*px
        for y in (front+1.3,rear-1.0):
            box('Nacelle service band',(side*px,y,2.17),(1.88,.19,2.12),'Trim',.08)
        box('Dark thruster throat',(side*px,rear+.08,2.15),(1.35,.22,1.35),'Structure',.12)
        box('Recessed thruster aperture',(side*px,rear+.21,2.15),(.88,.035,.78),'Signal',.10)
        box('Forward running light',(side*px,front-.025,2.13),(.42,.05,.08),'Signal',.01)
        box('Nacelle forward sensor shadow',(side*px,front-.064,2.14),(.81,.038,.55),'Structure',.055)
        box('Nacelle forward nested register',(side*px,front-.088,2.14),(.62,.02,.37),'Trim',.042)
        box('Nacelle forward lens',(side*px,front-.102,2.14),(.40,.012,.075),'Signal',.01)
        # Separated pads leave an obvious landed silhouette.
        for y in (-3,3.5):
            box('Landing strut',(side*1.4,y,.63),(.18,.32,1.18),'Structure')
            box('Landing pad',(side*1.4,y,.09),(.7,1.15,.18),'Trim')
        for y in (-.8,1,2.8):
            x=2.16 if dominion else 1.61
            box('Cabin armor divider',(side*x,y,2.8),(.10,.075,1.25),'Trim',.01)
        x=2.31 if dominion else 1.67
        box('Passenger access hatch',(side*x,2.7,2.25),(.06,1.2,1.95),'Structure',.05)
        box('Access handle',(side*(x+.055),2.38,2.3),(.055,.08,.32),'Trim',.015)
        box('Access signal',(side*(x+.06),2.7,3.15),(.03,.75,.045),'Signal',.005)
        # Paired aft stabilizers keep the access hatch clear.
        box('Tail fin',(side*(1.7 if dominion else 1.35),4.8,4.1 if dominion else 3.8),(.16,2.1,1.65),'Trim',.09)
        # Each broad read is split into serviceable armor, with a dark reveal
        # beneath it.  These pieces are visible from the protected berth glass.
        cabin_x=2.33 if dominion else 1.68
        for station_y in (-2.15,-.65,.85,2.35):
            for height in (1.82,3.11):
                box('Cabin panel shadow',(side*(cabin_x+.012),station_y,height),(.055,1.33,.84),'Structure',.024)
                box('Cabin ceramic cassette',(side*(cabin_x+.046),station_y,height),(.057,1.18,.69),'Armor',.035)
                box('Cassette register',(side*(cabin_x+.081),station_y-.39,height+.29),(.027,.29,.025),'Trim',.006)
            for z in (1.55,2.84,3.40):
                cylinder('Recessed cassette lock',(side*(cabin_x+.087),station_y+.45,z),.032,.026,'Trim',(0,math.pi/2,0),12)
        for y in (-3.0,-1.55,-.1,1.35,2.8):
            box('Split dorsal armor',(side*.76,y,4.36 if dominion else 3.70),(1.24,1.30,.09),'Armor',.025)
            box('Dorsal armor edge',(side*.76,y+.58,4.425 if dominion else 3.765),(1.11,.035,.025),'Trim',.005)
        # A readable closed service hatch; the scenic craft remains non-boardable.
        hatch_x=x+.095
        for edge_y in (2.09,3.31):
            box('Hatch machined jamb',(side*hatch_x,edge_y,2.25),(.095,.095,2.08),'Trim',.015)
            for z in (1.43,2.27,3.05):
                cylinder('Hatch captive fastener',(side*(hatch_x+.055),edge_y,z),.027,.024,'Structure',(0,math.pi/2,0),12)
        for edge_z in (1.21,3.28):
            box('Hatch machined sill',(side*hatch_x,2.70,edge_z),(.095,1.30,.085),'Trim',.013)
        box('Hatch nested panel',(side*(hatch_x+.035),2.70,2.25),(.052,.89,1.50),'Armor',.02)
        box('Hatch lock bay',(side*(hatch_x+.075),2.34,2.21),(.025,.18,.26),'Structure',.01)
        # Nacelle skins and line-of-thrust details are paired, but the palettes
        # and upstream wing structures keep the factions distinct.
        for band_y in (front+1.65,front+3.45,front+5.25,rear-1.65):
            if band_y >= rear-.55: continue
            box('Nacelle upper dark gasket',(side*px,band_y,3.315),(1.50,.92,.04),'Structure',.016)
            box('Nacelle upper armor cassette',(side*px,band_y,3.355),(1.37,.78,.065),'Armor',.025)
            box('Nacelle cassette index',(side*px,band_y-.34,3.401),(1.10,.027,.012),'Trim',.003)
            box('Nacelle flank recess',(side*(px+.91),band_y,2.45),(.055,.78,.44),'Structure',.01)
            box('Nacelle flank rail',(side*(px+.947),band_y,2.45),(.025,.55,.035),'Signal',.005)
        for rib_y in (front+1.05,front+3.95,rear-.85):
            box('Nacelle structural collar',(side*px,rib_y,2.19),(1.98,.13,2.18),'Trim',.04)
            for rib_x in (-.67,.67):
                cylinder('Collar service stud',(side*px+rib_x,rib_y-.073,3.20),.035,.025,'Structure',(math.pi/2,0,0),12)
        box('Thruster black aperture',(side*px,rear+.255,2.15),(1.04,.055,.94),'Structure',.10)
        for offset_x in (-.37,0,.37):
            box('Thruster radial vane',(side*px+offset_x,rear+.294,2.15),(.045,.018,.73),'Trim',.005)
        box('Thruster inner glow',(side*px,rear+.314,2.15),(.73,.012,.58),'Signal',.025)
        # Layered mechanical landing gear replaces the bare single-bar read.
        for gear_y in (-3,3.5):
            cylinder('Landing hydraulic ram',(side*1.4,gear_y,.59),.105,.94,'Trim',vertices=20)
            cylinder('Landing ram shield',(side*1.4,gear_y,.66),.145,.31,'Structure',vertices=20)
            box('Landing foot sacrificial sole',(side*1.4,gear_y,.035),(.84,1.33,.07),'Structure',.028)
            for dx in (-.29,.29):
                for dy in (-.47,.47):
                    cylinder('Landing foot keeper',(side*1.4+dx,gear_y+dy,.195),.035,.019,'Structure',vertices=10)
        if dominion:
            for y in (-1.1,.3,1.7,3.1):
                box('Dominion wing armor step',(side*4.85,y,2.445),(.70,1.01,.085),'Armor',.02)
                box('Dominion wing bronze register',(side*4.85,y+.45,2.498),(.57,.026,.013),'Trim',.003)
            box('Dominion command crest',(side*1.52,-2.85,4.35),(.31,1.28,.12),'Trim',.025)
        else:
            for y in (-1.9,-.4,1.1,2.6):
                box('Reformation outrigger shield',(side*4.20,y,1.991),(.65,1.07,.085),'Armor',.02)
                box('Reformation outrigger white index',(side*4.20,y+.46,2.042),(.52,.03,.015),'Signal',.004)
            for y in (-1.4,.4,2.2):
                box('Reformation exposed spar sleeve',(side*3.06,y,2.13),(1.58,.22,.14),'Trim',.018)
    # Apply normals, combine only exported components and pack UV0.
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts:
        bpy.context.view_layer.objects.active=obj
        for modifier in list(obj.modifiers): bpy.ops.object.modifier_apply(modifier=modifier.name)
        obj.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]; bpy.ops.object.join()
    mesh=bpy.context.object; mesh.name=name
    scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.uv.smart_project(island_margin=.015)
    bpy.ops.object.mode_set(mode='OBJECT')
    # One packed UV set is kept for surface mapping and a second independent
    # channel is present for Unreal's static-lighting/Nanite fallback workflow.
    if len(mesh.data.uv_layers)==1:
        lightmap=mesh.data.uv_layers.new(name='LightmapUV')
        source=mesh.data.uv_layers[0]
        for polygon_loop in mesh.data.loops:
            lightmap.data[polygon_loop.index].uv=source.data[polygon_loop.index].uv
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    mesh.data.calc_loop_triangles()
    report=dict(asset=name,triangles=len(mesh.data.loop_triangles),dimensions_metres=list(mesh.dimensions),pivot='bottom centre',nose='-Y',collision='none; static off-route dressing',materials={mats[k].name:dict(rgb=list(v[0]),metal=v[1],rough=v[2],emission=v[3]) for k,v in specs.items()},fbx_sha256=hashlib.sha256((ROOT/(name+'.fbx')).read_bytes()).hexdigest())
    (ROOT/(name+'.json')).write_text(json.dumps(report,indent=2),encoding='utf8')
    # Preview stage is excluded from export.
    box('Studio floor',(0,0,-.13),(200,200,.2),'Structure',0)
    scene.world=bpy.data.worlds.new('Studio'); scene.world.color=(.22,.22,.22)
    for pos,energy,size in [((3,-10,17),12000,12),((-12,-2,9),9000,10),((5,12,12),15000,8)]:
        data=bpy.data.lights.new('Softbox','AREA'); data.energy=energy; data.size=size
        obj=bpy.data.objects.new('Softbox',data); scene.collection.objects.link(obj); obj.location=pos
        obj.rotation_euler=(Vector((0,0,2))-obj.location).to_track_quat('-Z','Y').to_euler()
    data=bpy.data.cameras.new('Camera'); camera=bpy.data.objects.new('Camera',data); scene.collection.objects.link(camera)
    camera.location=(19,-25,16); camera.rotation_euler=(Vector((0,0,2))-camera.location).to_track_quat('-Z','Y').to_euler(); data.lens=48; scene.camera=camera
    scene.render.engine='CYCLES'; scene.cycles.samples=24; scene.cycles.use_denoising=True
    scene.render.resolution_x=1400; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/(name+'-preview.png'))
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/(name+'.blend'))); bpy.ops.render.render(write_still=True)

for faction in ('Dominion','Reformation'): build(faction)
