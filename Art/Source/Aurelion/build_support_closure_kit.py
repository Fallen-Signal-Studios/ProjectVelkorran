"""Detailed cache roof and two gates fitted to the native support envelopes."""
from pathlib import Path
base=Path(__file__).resolve().parent
exec(compile((base/'build_architecture_kit.py').read_text().split('# Four metre bay:')[0],'kit_helpers','exec'),globals())
ROOT=base/'SupportClosureKit';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.45,.65,.68),.1,.25)
raw_box=box
def box(name,loc,size,mat=stone,bevel=.005):return raw_box(name,loc,size,mat,min(bevel,min(size)*.2))
def reexport(o,hull=None):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True)
    if hull:hull.select_set(True)
    bpy.context.view_layer.objects.active=o
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
# The roof replaces the native 2.8 x 2.9 x 0.2 m solid, with its centre pivot retained.
box('Continuous sealed roof core',(0,0,0),(2.76,2.86,.15),dark,.003)
for sign in (-1,1):
    box('East west edge beam',(sign*1.35,0,0),(.1,2.9,.2),stone,.006)
    box('North south edge beam',(0,sign*1.40,0),(2.60,.1,.2),stone,.006)
    for x in (-.86,0,.86):
        box('Roof face cassette',(x,0,sign*.083),(.844,2.674,.034),stone,.004)
        for y in (-1.21,1.21):
            box('Cassette pin socket',(x,y,sign*.101),(.052,.035,.004),dark,.001)
            box('Cassette retaining pin',(x,y,sign*.104),(.014,.017,.002),gold,.0003)
    for x in (-.434,.434):
        box('Recessed roof seam',(x,0,sign*.077),(.008,2.64,.004),dark,.0005)
        for y in (-1.08,1.08):box('Service light',(x,y,sign*.081),(.007,.12,.004),lens,.0005)
roof=export('SM_Aurelion_KIT_CacheRoof',[2.8,2.9,.21])
bpy.ops.mesh.primitive_cube_add(size=1);h=bpy.context.object;h.name='UCX_'+roof.name+'_00';h.dimensions=(2.8,2.9,.2)
bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);h.hide_render=True;h.display_type='WIRE';reexport(roof,h)
manifest[-1].update(convex_hulls=1,solid_bounds_m=[[-1.4,-1.45,-.1],[1.4,1.45,.1]],position_precision=10,preserve_fallback_geometry=True,collision='Exact original roof solid; retaining pins extend 5 mm beyond surface')
roof.hide_render=True
for name,width,height,leaves in [('WestCacheGate',2.4,2.8,2),('EastFlankGate',9,4.5,6)]:
    depth=.4;w=width/leaves
    box('Sealed gate backing',(0,0,0),(width-.08,.28,height-.08),dark,.008)
    for x in (-width/2+.05,width/2-.05):box('Protective gate stile',(x,0,0),(.10,depth,height),stone,.008)
    for z in (-height/2+.05,height/2-.05):box('Gate closure rail',(0,0,z),(width-.20,depth,.10),stone,.008)
    for side in (-1,1):
        for i in range(leaves):
            x=-width/2+(i+.5)*w
            for j in range(3):
                z=-height/2+.15+(j+.5)*(height-.30)/3
                panel_h=(height-.30)/3-.025
                box('Individual ceramic gate cassette',(x,side*.16,z),(w-.13,.04,panel_h),stone,.006)
                for xx in (x-w/2+.12,x+w/2-.12):
                    for zz in (z-panel_h/2+.07,z+panel_h/2-.07):
                        box('Cassette fastener socket',(xx,side*.183,zz),(.032,.006,.032),dark,.001)
                        box('Cassette captive pin',(xx,side*.188,zz),(.009,.004,.009),gold,.0005)
            box('Recessed closure register',(x,side*.184,height/2-.30),(w-.35,.008,.105),dark,.003)
            for k in (-1,0,1):box('Isolated status lens',(x+k*.075,side*.190,height/2-.30),(.034,.004,.023),lens,.001)
            for z in (-height/2+.22,-height/2+.29):box('Lower service grille',(x,side*.183,z),(w-.30,.004,.015),dark,.001)
        for i in range(1,leaves):
            x=-width/2+i*w
            box('Interleaf compression seal',(x,side*.149,0),(.025,.006,height-.28),dark,.002)
            for z in (-height*.27,height*.27):
                box('Gate locking socket',(x,side*.166,z),(.086,.028,.16),dark,.003)
                box('Gate locking bridge',(x,side*.19,z),(.11,.02,.038),gold,.002)
    o=export('SM_Aurelion_KIT_'+name,[width,depth,height])
    # Runtime deliberately scales a centred 100 cm basis by the native box extent / 50.
    # Author details in final metres first, then normalize only the exported geometry.
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    o.scale=(1/width,1/depth,1/height);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    reexport(o)
    manifest[-1].update(nominal_dimensions_m=[1,1,1],authored_dimensions_m=[width,depth,height],runtime_scale=[width,depth,height],position_precision=10,preserve_fallback_geometry=True,collision='Visual only; normalized native SupportPresentation basis, physical barrier remains authoritative')
    o.scale=(width,depth,height);o.location=(0,0,0) if name=='WestCacheGate' else (0,2.5,1.0);o.hide_render=name=='EastFlankGate'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source authored; engine and live gate review pending',modules=manifest),indent=2))
roof.hide_render=False;roof.location=(0,0,1.5)
scene.world=bpy.data.worlds.new('Support closure studio');scene.world.color=(.15,.15,.15)
for pos,power in [((3,-5,6),1500),((-4,-2,3),1000),((2,3,5),1400)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=4;a.rotation_euler=(Vector((0,0,.3))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(5,-7,4));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.2))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=5.5;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'support-closure.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Support-Closures.blend'));bpy.ops.render.render(write_still=True)
print('SUPPORT_CLOSURE_SOURCE_PASS')
