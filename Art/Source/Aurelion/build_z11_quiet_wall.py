"""Blender source for the double-faced Z11 observation-gallery wall bay.

This is quiet architectural dressing over the retained native 4.5 x .5 x 6 m
walls. Its shallow relief cannot change the door, window, route, or collision.
"""
import json
from pathlib import Path

import bpy
from mathutils import Vector

source = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0], str(source), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z11QuietWall'
ROOT.mkdir(exist_ok=True)
black = material('M_Aurelion_BlackStone', (.013, .019, .025), .12, .27)
base_box = box
def box(name, loc, size, mat=stone, bevel=.008):
    # Small gold inlays are thinner than the generic kit's edge radius.
    # Capping the bevel prevents collapsed/zero-area FBX faces.
    return base_box(name, loc, size, mat, min(bevel, min(size)*.40))

# A solid ancient substrate, with dressed ivory courses and a person-height
# black-stone register. Details are shallow: the transparent wall is the focus.
box('Continuous load-bearing stone', (0, 0, 3), (4.48, .40, 5.96), stone, .012)
for face in (-1, 1):
    y = face * .218
    outer = face * .238
    # Two large dressed stones per course keep a human-scale rhythm without a
    # monumental emblem or a console in this private conversation room.
    for z, h in ((1.78, 1.05), (2.88, 1.05), (3.98, 1.05), (5.08, 1.05)):
        for x in (-1.08, 1.08):
            box('Honed upper ashlar', (x, y, z), (2.09, .036, h), stone, .018)
            box('Shallow keyed reveal', (x, outer, z), (1.84, .006, h-.17),
                dark, .003)
            box('Inset ivory tablet', (x, outer+face*.0035, z),
                (1.80, .005, h-.22), stone, .009)

    box('Dado shadow setback', (0, y, .65), (4.42, .042, 1.24), dark, .009)
    for x in (-1.64, -.55, .55, 1.64):
        box('Polished black lower stone', (x, outer-face*.008, .66),
            (1.045, .026, 1.13), black, .016)
        box('Dado top bevel', (x, outer+face*.006, 1.185),
            (.91, .009, .025), gold, .003)
        for dx in (-.37, .37):
            box('Dado captive fixing', (x+dx, outer+face*.006, .18),
                (.022, .008, .022), gold, .002)

    # Dark end stiles and a very fine continuous conductor use light as
    # information; they carry no faction device or glowing mural.
    for x in (-2.17, 2.17):
        box('End stile', (x, y, 3.02), (.10, .052, 5.72), black, .008)
        box('Stile arris', (x, outer, 3.12), (.024, .009, 5.46), gold, .003)
        for z in (.30, 1.33, 3.0, 4.9, 5.79):
            box('Stile bearing key', (x, outer+face*.004, z),
                (.055, .008, .065), stone, .004)

    # One built-in service register per bay, small enough to read as a wall
    # fitting rather than a new story interaction.
    box('Service register surround', (0, outer, .52), (.46, .012, .24),
        gold, .008)
    box('Recessed service glass', (0, outer+face*.007, .52),
        (.39, .008, .17), black, .005)
    for i in range(5):
        box('Service breath slot', ((i-2)*.068, outer+face*.010, .52),
            (.026, .004, .085), dark, .002)
    box('Lower conductor', (0, outer, 1.31), (4.32, .009, .025),
        gold, .003)
    box('Upper cornice shadow', (0, outer, 5.78), (4.31, .012, .060),
        dark, .004)
    box('Upper cornice fine gold', (0, outer+face*.006, 5.815),
        (4.25, .007, .011), gold, .002)

# Cap and foot have independent stepped stone profiles so the view remains
# convincing from the entry as well as from the main observation position.
for z, width, depth, height, mat in (
    (.085, 4.49, .49, .17, black),
    (.222, 4.47, .47, .085, stone),
    (5.80, 4.46, .47, .12, black),
    (5.907, 4.49, .49, .17, stone),
):
    box('Full-depth stone course', (0, 0, z), (width, depth, height), mat, .009)

mesh = export('SM_Aurelion_KIT_Z11QuietWall_4p5x6', [4.49, .50, 5.992])
manifest[-1].update(
    nominal_dimensions_m=[round(v, 5) for v in mesh.dimensions],
    collision='None: visual wall over preserved native structural collision',
    preserve_fallback_geometry=True,
    position_precision=10,
)
(ROOT/'manifest.json').write_text(json.dumps(dict(
    source='Editable Blender 4.5 procedural source',
    design_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z11-Observation-Gallery.png',
    narrative_reference='Manuscript chapter 26 and Aurelion level layout page 15',
    modules=manifest,
    placement=dict(
        wall_x_cm=[-975, -525, 525, 975],
        wall_y_cm=[42300, 43900],
        bottom_z_cm=0,
        south_central_doorway_unchanged=True,
        observation_window_unchanged=True,
        native_wall_collision_unchanged=True,
    ),
), indent=2), encoding='utf-8')

scene.world=bpy.data.worlds.new('Z11 quiet wall studio')
scene.world.color=(.03,.035,.042)
for pos,power,size,color in (
    ((-5,-7,8),1950,6,(1,.83,.65)),
    ((5,3,6),1400,5,(.69,.81,1)),
):
    bpy.ops.object.light_add(type='AREA', location=pos)
    lamp=bpy.context.object
    lamp.data.energy,lamp.data.size,lamp.data.color=power,size,color
    lamp.rotation_euler=(Vector((0,0,3))-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(6.3,-10.0,5.0))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,3))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type,camera.data.ortho_scale='ORTHO',8.1
scene.camera=camera
scene.render.engine='CYCLES'
scene.cycles.samples=32
scene.cycles.use_denoising=True
scene.render.resolution_x,scene.render.resolution_y=1400,1100
scene.render.filepath=str(ROOT/'quiet-wall-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Quiet-Wall.blend'))
bpy.ops.render.render(write_still=True)
print('Z11_QUIET_WALL_SOURCE_PASS')
