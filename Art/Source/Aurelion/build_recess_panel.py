"""Blender 4.5 source art for the layout plan's white/gold survivor shelters.

Run with Blender --background --python this_file.py. Outputs stay beside this
script. No Unreal assets, collision settings or map instances are edited.
"""
import bpy
import hashlib
import json
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
NAME = 'SM_Aurelion_RecessPanel_2m'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0

def material(name, color, metal, roughness, emission=0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Metallic'].default_value = metal
    p.inputs['Roughness'].default_value = roughness
    if emission:
        p.inputs['Emission Color'].default_value = (*color, 1)
        p.inputs['Emission Strength'].default_value = emission
    return m

stone = material('M_Aurelion_IvoryStone', (.72, .69, .61), .12, .32)
gold = material('M_Aurelion_AncientGold', (.52, .30, .075), .8, .28)
dark = material('M_Aurelion_ChannelShadow', (.025, .035, .042), .55, .29)
light = material('M_Aurelion_WarmInformation', (1, .64, .22), .2, .3, 2)
parts = []

def box(name, location, size, mat, bevel=.008, export=True):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if mat:
        obj.data.materials.append(mat)
    if bevel:
        mod = obj.modifiers.new('Manufactured edge', 'BEVEL')
        mod.width = bevel
        mod.segments = 2
        bpy.ops.object.modifier_apply(modifier=mod.name)
        obj.modifiers.new('Face normals', 'WEIGHTED_NORMAL')
    if export:
        parts.append(obj)
    return obj

# A 2 x 0.4 x 3 metre envelope; bottom-centre pivot. All relief stays inside it.
box('Structural stone', (0, .025, 1.5), (1.86, .35, 2.84), stone, .022)
box('Lower plinth', (0, 0, .08), (2, .4, .16), stone, .012)
box('Upper lintel', (0, 0, 2.94), (2, .4, .12), stone, .012)
for side in (-1, 1):
    box('Edge pier', (side*.94, -.055, 1.51), (.12, .29, 2.7), stone, .012)
    box('Recessed channel', (side*.82, -.157, 1.5), (.095, .014, 2.57), dark, .003)
    box('Gold inner edge', (side*.856, -.172, 1.5), (.018, .018, 2.53), gold, .003)
    box('Information inlay', (side*.802, -.17, 1.5), (.009, .01, 2.47), light, .002)
    box('Inset face', (side*.395, -.168, 1.54), (.71, .026, 2.35), stone, .018)
box('Centre reveal', (0, -.155, 1.54), (.03, .012, 2.36), dark, .002)
for z in (.23, 2.80):
    box('Horizontal gold seam', (0, -.179, z), (1.72, .014, .022), gold, .003)

bpy.ops.object.select_all(action='DESELECT')
for obj in parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
mesh = bpy.context.object
mesh.name = NAME
scene.cursor.location = (0, 0, 0)
bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(island_margin=.02)
bpy.ops.object.mode_set(mode='OBJECT')

collision = box('UCX_' + NAME + '_00', (0, 0, 1.5), (2, .4, 3), None, 0, False)
collision.hide_render = True
collision.display_type = 'WIRE'
bpy.ops.object.select_all(action='DESELECT')
mesh.select_set(True)
collision.select_set(True)
bpy.context.view_layer.objects.active = mesh
bpy.ops.export_scene.fbx(filepath=str(ROOT/(NAME+'.fbx')), use_selection=True,
    object_types={'MESH'}, axis_forward='-Y', axis_up='Z', apply_unit_scale=True,
    bake_anim=False, add_leaf_bones=False, mesh_smooth_type='FACE')

# Preview-only studio; excluded from the FBX selection above.
floor = material('Preview floor', (.035, .044, .055), .25, .28)
box('Preview ground', (0, 0, -.055), (200, 200, .1), floor, 0, False)
scene.world = bpy.data.worlds.new('Preview world')
scene.world.color = (.1, .1, .1)
for name, position, energy, size in (
    ('Key', (1, -3, 5), 850, 4), ('Fill', (-3, -1, 2.8), 500, 3),
    ('Rim', (1, 2, 4), 650, 2)):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy, data.shape, data.size = energy, 'DISK', size
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = position
    obj.rotation_euler = (Vector((0, 0, 1.5))-obj.location).to_track_quat('-Z', 'Y').to_euler()
data = bpy.data.cameras.new('Preview camera')
camera = bpy.data.objects.new('Preview camera', data)
scene.collection.objects.link(camera)
camera.location = (3.5, -6, 3.4)
camera.rotation_euler = (Vector((0, 0, 1.45))-camera.location).to_track_quat('-Z', 'Y').to_euler()
data.lens = 55
scene.camera = camera
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x = scene.render.resolution_y = 1100
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = str(ROOT/(NAME+'-preview.png'))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/(NAME+'.blend')))
bpy.ops.render.render(write_still=True)
mesh.data.calc_loop_triangles()
report = dict(asset=NAME, status='source_art_preview_only_not_imported_or_gameplay_qualified',
    reference='Aurelion_Level_Layout_Plan.pdf pages 20 and 23; no heraldry or faction ownership',
    envelope_metres=[2, .4, 3], pivot='bottom centre', triangles=len(mesh.data.loop_triangles),
    collision='one UCX box; final Unreal scale and collision require inspection',
    materials=[m.name for m in mesh.data.materials],
    fbx_sha256=hashlib.sha256((ROOT/(NAME+'.fbx')).read_bytes()).hexdigest())
(ROOT/(NAME+'.json')).write_text(json.dumps(report, indent=2), encoding='utf8')
