"""Blender 4.5 upper Crucible enclosure, above existing seven-metre walls.

Layout-plan p23 architectural proposal. No collision or gameplay edits.
Local Z0 meets existing wall tops; main floor is local Z-7m.
"""
import bpy
import json
import hashlib
from pathlib import Path
from mathutils import Vector

ROOT=Path(__file__).resolve().parent
NAME='SM_Aurelion_CrucibleVault'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene; scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1
materials={}
for name,rgb,metal,rough,emission in (
    ('Stone',(.65,.63,.57),.12,.48,0),
    ('Gold',(.48,.29,.075),.8,.32,0),
    ('Shadow',(.025,.037,.05),.4,.4,0),
    ('Inlay',(1,.69,.32),.1,.4,1.2)):
    mat=bpy.data.materials.new('M_Vault_'+name); mat.diffuse_color=(*rgb,1); mat.use_nodes=True
    shader=mat.node_tree.nodes.get('Principled BSDF')
    for key,value in [('Base Color',(*rgb,1)),('Metallic',metal),('Roughness',rough),('Emission Color',(*rgb,1)),('Emission Strength',emission)]: shader.inputs[key].default_value=value
    materials[name]=mat
parts=[]
def box(name,position,size,material,bevel=.025,export=True):
    bpy.ops.mesh.primitive_cube_add(size=1,location=position)
    obj=bpy.context.object; obj.name=name; obj.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    obj.data.materials.append(materials[material])
    if bevel:
        mod=obj.modifiers.new('Dressed stone edges','BEVEL'); mod.width=bevel; mod.segments=2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    if export: parts.append(obj)
    return obj
def beam(name,start,end,width,depth,material):
    a,b=Vector(start),Vector(end)
    obj=box(name,(a+b)/2,(width,depth,(b-a).length),material,.025)
    obj.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    return obj
# Four walls and a distant ceiling provide a closed upper volume; panel relief
# faces inward. The visual underside begins above every playable balcony.
for side in (-1,1):
    box('Side enclosure',(side*35.35,0,7.5),(.7,48.7,15),'Stone')
    box('End enclosure',(0,side*24.35,7.5),(70,.7,15),'Stone')
    # Vertical inset bays and slender double gold reveals.
    for y in (-20,-12,-4,4,12,20):
        box('Side shadow recess',(side*34.975,y,7.6),(.045,2.6,12.0),'Shadow',.005)
        for offset in (-1.42,1.42):
            box('Side reveal edge',(side*34.91,y+offset,7.6),(.06,.075,12.2),'Gold',.008)
        box('Vertical information channel',(side*34.86,y,8),(.035,.045,10.6),'Inlay',.005)
    for x in (-28,-14,0,14,28):
        box('End shadow recess',(x,side*23.975,7.6),(3.1,.045,12),'Shadow',.005)
        for offset in (-1.65,1.65):
            box('End reveal edge',(x+offset,side*23.91,7.6),(.075,.06,12.2),'Gold',.008)
        box('End information channel',(x,side*23.86,8),(.045,.035,10.6),'Inlay',.005)
box('Upper roof',(0,0,15.2),(70.7,48.7,.4),'Stone')
# Repeated angular portal ribs; lower feet meet the existing wall head.
for y in (-23,-12,0,12,23):
    path=[(-34.75,y,0),(-34.75,y,7),(-27,y,14),(27,y,14),(34.75,y,7),(34.75,y,0)]
    for a,b in zip(path,path[1:]):
        beam('Primary stone rib',a,b,.85,1.15,'Stone')
        for d in (-.61,.61):
            aa=(a[0],a[1]+d,a[2]); bb=(b[0],b[1]+d,b[2])
            beam('Gold rib edging',aa,bb,.095,.07,'Gold')
            beam('Rib light seam',(aa[0],aa[1]+(.045 if d>0 else -.045),aa[2]),(bb[0],bb[1]+(.045 if d>0 else -.045),bb[2]),.026,.028,'Inlay')
    for side in (-1,1):
        box('Rib foot capital',(side*34.7,y,.2),(1.05,1.35,.4),'Gold')
# Longitudinal roof coffers reduce the apparent broad flat span.
for x in (-21,-7,7,21):
    box('Coffer longitudinal',(x,0,14.72),(.35,48,.38),'Stone')
    box('Coffer gold line',(x,0,14.50),(.055,48,.045),'Gold',.007)

bpy.ops.object.select_all(action='DESELECT')
for obj in parts: obj.select_set(True)
bpy.context.view_layer.objects.active=parts[0]; bpy.ops.object.join()
mesh=bpy.context.object; mesh.name=NAME
scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.normals_make_consistent(inside=False)
bpy.ops.uv.smart_project(island_margin=.006); bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.export_scene.fbx(filepath=str(ROOT/(NAME+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
mesh.data.calc_loop_triangles()
report=dict(asset=NAME,triangles=len(mesh.data.loop_triangles),dimensions_metres=list(mesh.dimensions),placement_z_cm=-500,collision='none; above playable wall head',materials=[m.name for m in mesh.data.materials],status='architectural proposal requiring Unreal review',fbx_sha256=hashlib.sha256((ROOT/(NAME+'.fbx')).read_bytes()).hexdigest())
(ROOT/(NAME+'.json')).write_text(json.dumps(report,indent=2),encoding='utf8')
# Preview context only. Lower walls/floor are not exported.
box('Preview floor',(0,0,-7.15),(72,50,.3),'Shadow',0,False)
for side in (-1,1):
    box('Preview lower side',(side*35.35,0,-3.5),(.7,48.7,7),'Stone',0,False)
    box('Preview lower end',(0,side*24.35,-3.5),(70,.7,7),'Stone',0,False)
scene.world=bpy.data.worlds.new('Preview world'); scene.world.color=(.08,.08,.08)
for x in (-22,22):
    for y in (-14,10):
        data=bpy.data.lights.new('Interior softbox','AREA'); data.energy=18000; data.size=12
        obj=bpy.data.objects.new('Interior softbox',data); scene.collection.objects.link(obj); obj.location=(x,y,8)
        obj.rotation_euler=(Vector((0,y,-2))-obj.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Review camera'); camera=bpy.data.objects.new('Review camera',data); scene.collection.objects.link(camera)
camera.location=(0,-22,-5.35); camera.rotation_euler=(Vector((0,16,5))-camera.location).to_track_quat('-Z','Y').to_euler(); data.lens=22; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=24; scene.cycles.use_denoising=True
scene.render.resolution_x=1400; scene.render.resolution_y=900; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/(NAME+'-preview.png'))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/(NAME+'.blend'))); bpy.ops.render.render(write_still=True)
