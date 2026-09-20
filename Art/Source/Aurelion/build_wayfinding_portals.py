"""Detailed floor-supported Aurelion sign housings, with open segmented collision.
Run Blender with --background --factory-startup to avoid loading user add-ons.
"""
from pathlib import Path
base=Path(__file__).resolve().parent
exec(compile((base/'build_architecture_kit.py').read_text().split('# Four metre bay:')[0],'kit_helpers','exec'),globals())
ROOT=base/'WayfindingPortals';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.45,.65,.68),.1,.25)
raw_box=box
def box(name,loc,size,mat=stone,bevel=.006):return raw_box(name,loc,size,mat,min(bevel,min(size)*.2))
def outline(w,h,z,y):
    c=.12
    return [(-w/2+c,y,z-h/2),(-w/2,y,z-h/2+c),(-w/2,y,z+h/2-c),
        (-w/2+c,y,z+h/2),(w/2-c,y,z+h/2),(w/2,y,z+h/2-c),
        (w/2,y,z-h/2+c),(w/2-c,y,z-h/2),(-w/2+c,y,z-h/2)]
def collision_box(name,loc,size):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o.hide_render=True;o.display_type='WIRE';return o
for suffix,center in [('Gallery',2.64),('Chamber',3.64)]:
    bottom=center-.40
    # Layered ceramic cheeks, bronze inlays and a deep, non-emissive lettering bed.
    box('Recessed sign backplane',(0,.075,center),(3.62,.15,.68),dark,.028)
    path('Ceramic octagonal perimeter',outline(3.68,.76,center,-.008),.064,.19,stone)
    path('Bronze perimeter inlay',outline(3.62,.70,center,-.022),.016,.025,gold,.002)
    path('Inner optical bevel',outline(3.45,.58,center,-.014),.010,.025,gold,.001)
    box('Lower lighting recess',(0,-.015,center-.33),(2.80,.016,.023),dark,.002)
    for x in (-1.25,-.75,-.25,.25,.75,1.25):
        box('Recessed orientation lens',(x,-.026,center-.33),(.34,.008,.006),lens,.001)
    # Rear service panels and recessed fasteners remain inspectable from departure.
    for x in (-1.18,0,1.18):
        box('Rear removable cassette',(x,.163,center),(1.12,.024,.49),stone,.008)
        for dx in (-.49,.49):
            for z in (center-.19,center+.19):
                box('Rear fastener pocket',(x+dx,.180,z),(.025,.012,.025),dark,.003)
                box('Rear retaining pin',(x+dx,.188,z),(.009,.004,.009),gold,.001)
        for z in (center-.09,center-.045,center,center+.045,center+.09):
            box('Cassette ventilation slot',(x,.179,z),(.45,.010,.014),dark,.001)
    for side in (-1,1):
        x=side*1.81
        box('Floor isolation shoe',(x,.055,.025),(.26,.30,.05),dark,.006)
        box('Ceramic pedestal',(x,.055,.115),(.23,.27,.13),stone,.012)
        box('Bronze base collar',(x,.055,.19),(.19,.22,.035),gold,.004)
        box('Fluted upright',(x,.065,(.21+bottom+.17)/2),(.13,.17,bottom+.17-.21),stone,.012)
        box('Front recessed channel',(x,-.028,(.25+bottom)/2),(.030,.025,bottom-.25),dark,.002)
        box('Bronze channel spine',(x,-.043,(.25+bottom)/2),(.006,.006,bottom-.25),gold,.001)
        for z in (.31,bottom*.48,bottom-.11):
            box('Upright joint sleeve',(x,.065,z),(.15,.19,.055),dark,.004)
            box('Sleeve fillet',(x,-.033,z+.025),(.13,.014,.006),gold,.001)
        box('Head attachment saddle',(x,.08,bottom+.16),(.18,.24,.30),gold,.010)
        for z in (bottom+.09,bottom+.23):
            box('Saddle pin socket',(x,-.049,z),(.032,.016,.032),dark,.004)
            box('Saddle pin',(x,-.060,z),(.012,.006,.012),gold,.001)
    name='SM_Aurelion_KIT_Wayfinding'+suffix
    # Exact extents from authored geometry, avoiding nominal-size import assumptions.
    points=[o.matrix_world@Vector(c) for o in parts for c in o.bound_box]
    dims=[max(p[i] for p in points)-min(p[i] for p in points) for i in range(3)]
    obj=export(name,dims)
    hulls=[collision_box('UCX_'+name+'_00',(-1.81,.055,bottom/2),(.26,.30,bottom)),
        collision_box('UCX_'+name+'_01',(1.81,.055,bottom/2),(.26,.30,bottom)),
        collision_box('UCX_'+name+'_02',(0,.075,center),(3.75,.24,.80))]
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
    for h in hulls:h.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},
        axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    manifest[-1].update(convex_hulls=3,collision='Two post hulls plus lintel; centre opening retained',
        opening_width_m=3.36,opening_height_m=bottom,position_precision=10,preserve_fallback_geometry=True)
    if suffix=='Gallery':
        obj.hide_render=True
        for h in hulls:h.hide_render=True
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Wayfinding studio');scene.world.color=(.13,.13,.13)
for pos,power in [((2,-4,6),1100),((-3,-2,3),900),((2,3,5),1300)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=4
    a.rotation_euler=(Vector((0,0,2))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(5,-9,5));camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,2))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=5.7;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1200;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'wayfinding-portal.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Wayfinding-Portals.blend'))
bpy.ops.render.render(write_still=True)
