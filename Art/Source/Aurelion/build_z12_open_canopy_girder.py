"""Blender 4.5 source for a sparse, visual-only M13 berth side girder.

The girder runs between existing bearing feet and carries the existing pendant
at the mid-bay. Repeating it along both side walls keeps the central red/white
remnant vista unobstructed. Collision/navigation remain owned by the map.
"""
import json
import math
from pathlib import Path

import bpy
from mathutils import Vector

source = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0], str(source), 'exec'))
ROOT = Path(__file__).resolve().parent/'Z12OpenCanopyGirder'
ROOT.mkdir(exist_ok=True)
black = material('M_Aurelion_BlackStone', (.024, .027, .031), .18, .46)
steel = material('M_Aurelion_DarkSteel', (.055, .061, .067), .72, .32)

def sloping_bar(name, first, last, depth, height, mat, bevel=.01):
    a, b = Vector(first), Vector(last)
    delta = b-a
    o = box(name, (a+b)*.5, (delta.length, depth, height), mat, bevel)
    o.rotation_euler.y = -math.atan2(delta.z, delta.x)
    return o

# A continuous dark core, ivory edge stones and layered underside expose the
# working structure rather than filling the entire canopy with opaque cassettes.
box('Longitudinal steel web', (0, 0, .33), (.69, 4.15, .50), steel, .021)
box('Shadow soffit channel', (0, 0, .071), (.54, 4.11, .066), black, .008)
box('Soffit illuminated register', (0, 0, .028), (.23, 4.10, .018), gold, .004)
for side in (-1, 1):
    x = side*.365
    box('Cut ivory edge rail', (x, 0, .54), (.105, 4.15, .23), stone, .012)
    box('Edge gold fillet', (x+side*.054, 0, .371), (.026, 4.13, .025), gold, .004)
    box('Upper dark key line', (x, 0, .685), (.055, 4.12, .020), dark, .003)
    # Balanced cantilevers permit the same owned mesh on either side of the
    # berth. The inward shoe aligns with the saved pendant 0.65 m off the foot.
    for y in (-1.72, 0, 1.72):
        sloping_bar('Splayed bracket', (side*.28, y, .36),
                    (side*.70, y, .035), .22, .16, black, .010)
        sloping_bar('Bracket ivory arris', (side*.30, y-.035, .425),
                    (side*.685, y-.035, .112), .055, .044, stone, .006)
        box('Bracket captive bolt', (side*.69, y-.13, .05),
            (.045, .04, .045), gold, .004)
    box('Pendant load shoe', (side*.65, 0, .017), (.39, .58, .09), black, .012)
    box('Pendant shoe inset', (side*.65, 0, -.036), (.29, .47, .019), gold, .003)
    for y in (-1.86, -1.20, -.54, .54, 1.20, 1.86):
        box('Web removable service cassette', (side*.355, y, .32),
            (.045, .41, .24), black, .005)
        box('Cassette ivory edge', (side*.386, y, .32),
            (.015, .37, .20), stone, .003)
        box('Cassette gold index', (side*.397, y-.13, .32),
            (.008, .042, .125), gold, .002)
    for y in (-1.935, 1.935):
        box('End keyed joint plate', (side*.35, y, .36),
            (.09, .23, .48), black, .012)
        for z in (.16, .57):
            box('Joint pin', (side*.409, y, z), (.035, .045, .035), gold, .003)

for y in (-2.035, 2.035):
    box('Ivory end bearing cap', (0, y, .35), (.89, .08, .62), stone, .009)
    box('Dark expansion joint', (0, y+math.copysign(.048, y), .35),
        (.69, .028, .47), black, .003)
    for x in (-.27, .27):
        box('Terminal gold pin', (x, y+math.copysign(.066, y), .33),
            (.034, .015, .11), gold, .003)

module = export('SM_Aurelion_KIT_Z12OpenCanopySideGirder', [1.79, 4.23, .74])
manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in module.dimensions]
manifest[-1].update(
    collision='None: visual side girder; existing physical dock remains authoritative',
    preserve_fallback_geometry=True,
    purpose='Join existing bearing feet and carry existing pendant without covering remnant vista',
)
(ROOT/'manifest.json').write_text(json.dumps(dict(
    source='Blender 4.5 editable procedural module',
    design_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-24/Z12-Open-Canopy-Reference.png',
    modules=manifest,
    placement=dict(stations_per_side=4, station_span_m=4.15,
                   bearing_x_m=[-13.2, 13.2], pendant_offset_x_m=.65,
                   base_z_m=7.5, visual_only=True),
), indent=2), encoding='utf-8')

scene.world = bpy.data.worlds.new('Z12 open canopy studio')
scene.world.color = (.025, .031, .048)
for pos, power, size, color in [
    ((-5, -7, 8), 1800, 7, (1, .82, .62)),
    ((5, 2, 7), 1600, 7, (.70, .82, 1)),
]:
    bpy.ops.object.light_add(type='AREA', location=pos)
    lamp = bpy.context.object
    lamp.data.energy, lamp.data.size, lamp.data.color = power, size, color
    lamp.rotation_euler = (Vector((0, 0, .2))-lamp.location).to_track_quat('-Z', 'Y').to_euler()
bpy.ops.object.camera_add(location=(3.7, -5.4, 2.1))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, .2))-camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type, camera.data.ortho_scale = 'ORTHO', 5.6
scene.camera = camera
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x, scene.render.resolution_y = 1200, 800
scene.render.filepath = str(ROOT/'open-canopy-girder-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Open-Canopy-Girder.blend'))
bpy.ops.render.render(write_still=True)
