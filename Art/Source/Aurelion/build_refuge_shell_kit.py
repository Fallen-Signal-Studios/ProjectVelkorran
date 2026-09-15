"""Measured refuge shell assemblies; centre pivots and exact original solid hulls."""
from pathlib import Path
import bpy,json,math
from mathutils import Vector
base=Path(__file__).resolve().parent
exec(compile((base/'build_architecture_kit.py').read_text().split('# Four metre bay:')[0],'kit_helpers','exec'),globals())
ROOT=base/'RefugeShellKit';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.45,.65,.68),.1,.25)
canonical={m.name:m for m in (stone,gold,dark,lens)}
templates={}
for width in (1,2):
    bpy.ops.import_scene.fbx(filepath=str(base/'Z08RefugeKit'/('SM_Aurelion_KIT_RefugePanel_'+str(width)+'m.fbx')))
    imported=list(bpy.context.selected_objects)
    source=next(o for o in imported if o.type=='MESH' and not o.name.startswith('UCX_'))
    bpy.context.view_layer.objects.active=source
    bpy.ops.object.select_all(action='DESELECT');source.select_set(True)
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    for slot in source.material_slots:slot.material=canonical[slot.material.name.split('.')[0]]
    templates[width]=source.data.copy()
    for obj in imported:bpy.data.objects.remove(obj,do_unlink=True)
for length in (1,3,7,10,12):
    height=2.98 if length==10 else 2.97 if length in (7,12) else 2.96
    widths=[2]*(length//2)+([1] if length%2 else [])
    cursor=-length/2
    for width in widths:
        data=templates[width].copy()
        # Repack the joined assembly into two UV sets, avoiding inherited third layers.
        while data.uv_layers:data.uv_layers.remove(data.uv_layers[0])
        data.uv_layers.new(name='UVMap')
        for v in data.vertices:v.co.x+=cursor+width/2;v.co.z=v.co.z*height/3-1.5
        obj=bpy.data.objects.new('Fitted double-faced bay',data);scene.collection.objects.link(obj);parts.append(obj);cursor+=width
    name='SM_Aurelion_KIT_RefugeShell_'+str(length)+'m'
    o=export(name,[length,.4,height])
    bpy.ops.mesh.primitive_cube_add(size=1);h=bpy.context.object;h.name='UCX_'+name+'_00';h.dimensions=(length,.4,3)
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);h.hide_render=True;h.display_type='WIRE'
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);h.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    manifest[-1].update(convex_hulls=1,solid_bounds_m=[[-length/2,-.2,-1.5],[length/2,.2,1.5]],cap_setback_m=3-height,collision='Original centred solid enclosure; cap visual stepped below collision',position_precision=10,preserve_fallback_geometry=True)
    o.hide_render=True
base_box=box
def box(name,loc,size,mat=stone,bevel=.002):return base_box(name,loc,size,mat,min(bevel,min(size)*.2))
for length in (5,6):
    box('Recessed threshold channel',(0,0,-.0135),(length,.2,.023),dark)
    for sign in (-1,1):box('Protective ceramic edge',(0,sign*.084,-.002),(length,.032,.004),stone,.0005)
    for i in range(length*2):
        x=-length/2+.25+i*.5
        box('Inlaid conductor segment',(x,0,-.001),(.39,.012,.002),gold,.0003)
        box('Isolated route light',(x+.215,0,-.001),(.016,.046,.002),lens,.0003)
    o=export('SM_Aurelion_KIT_RefugeThreshold_'+str(length)+'m',[length,.2,.025])
    manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='Decorative embedded inlay, no collision',top_pivot=True)
    o.hide_render=True
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source authored; engine replacement pending',modules=manifest),indent=2))
for o in modules:
    if o.name.endswith('Shell_10m'):o.hide_render=False
    if o.name.endswith('Shell_3m'):o.hide_render=False;o.location=(-5,-1.5,0);o.rotation_euler.z=math.pi/2
    if o.name.endswith('Threshold_6m'):o.hide_render=False;o.location=(0,-2,-1.5)
scene.world=bpy.data.worlds.new('Refuge shell studio');scene.world.color=(.15,.15,.15)
for pos,power,size in [((1,-5,7),2300,6),((-6,-3,3),1500,5),((2,4,6),2000,5)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=size;a.rotation_euler=(Vector((0,0,0))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(9,-13,7));camera=bpy.context.object;camera.rotation_euler=(Vector((-.5,-.4,0))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=12.5;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1800;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'refuge-shell.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Refuge-Shell.blend'));bpy.ops.render.render(write_still=True)
print('REFUGE_SHELL_SOURCE_PASS')
