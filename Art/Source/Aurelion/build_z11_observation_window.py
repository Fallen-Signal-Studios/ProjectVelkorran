"""Blender source for Z11's open, non-colliding King-era observation surround.

The 15 m surround keeps the red remnant, Wound, and white remnant visible. Native
Z11 walls/railings continue to own collision; this is architectural dress only.
"""
import json
import math
from pathlib import Path

from mathutils import Vector

source = Path(__file__).with_name("build_architecture_kit.py")
exec(compile(source.read_text().split("# Four metre bay:")[0], str(source), "exec"))
ROOT = Path(__file__).resolve().parent / "Z11ObservationWindow"
ROOT.mkdir(exist_ok=True)
black = material("M_Aurelion_BlackStone", (.022, .025, .030), .04, .54)


def bar_between(name, start, end, depth, thickness, mat, bevel=.009):
    a, b = Vector(start), Vector(end)
    midpoint = (a + b) * .5
    direction = b - a
    piece = box(name, midpoint, (depth, thickness, direction.length), mat, bevel)
    piece.rotation_euler = direction.to_track_quat("Z", "X").to_euler()
    return piece


# The visible reveal is hollow. Its structural dark backing sits just in front
# of the retained level wall, while the center stays completely open.
for side in (-1, 1):
    y = side * 7.45
    box("Deep edge pier", (-.13, y, 3.0), (.63, .58, 5.92), black, .021)
    box("Ivory outer return", (.02, y + side * .275, 3.0), (.39, .085, 5.91), stone, .013)
    box("Dressed inner arris", (.19, y - side * .24, 3.0), (.12, .075, 5.78), stone, .009)
    box("Recessed channel shadow", (.205, y - side * .177, 3.0), (.018, .052, 5.67), dark, .003)
    box("Conductor spine", (.222, y - side * .177, 3.0), (.025, .021, 5.50), gold, .002)
    for z in (.49, 1.88, 3.27, 4.66):
        box("Fitted pier face", (.245, y, z), (.11, .385, 1.27), black, .014)
        box("Fine horizontal witness", (.306, y, z + .606), (.008, .343, .011), gold, .002)
        for offset in (-.14, .14):
            box("Cut stone pin", (.31, y + offset, z + .49), (.008, .018, .018), gold, .002)
    for z in (.14, 5.65):
        box("Pier socket collar", (.032, y, z), (.71, .76, .19), stone, .017)
        box("Socket reveal", (.245, y, z + .11), (.028, .68, .026), dark, .004)

# A shallow segmented crown recalls the living terminal rings without
# masking the black central Wound or creating a second emblem in the image.
arc = []
for i in range(25):
    y = -7.22 + i * (14.44 / 24)
    z = 5.46 + .35 * math.cos((y / 7.22) * math.pi * .5)
    arc.append(Vector((-.01, y, z)))
for i, (a, b) in enumerate(zip(arc[:-1], arc[1:])):
    bar_between("Crown black stone segment %02d" % i, a, b, .57, .28, black, .011)
    front_a, front_b = a.copy(), b.copy()
    front_a.x = front_b.x = .30
    front_a.z -= .025
    front_b.z -= .025
    bar_between("Crown dressed ivory face %02d" % i, front_a, front_b, .11, .18, stone, .006)
    gold_a, gold_b = front_a.copy(), front_b.copy()
    gold_a.x = gold_b.x = .366
    gold_a.z -= .105
    gold_b.z -= .105
    bar_between("Crown gold conductor %02d" % i, gold_a, gold_b, .022, .017, gold, .002)
    if i % 4 == 0 and i not in (0, 20):
        seam = (a + b) * .5
        box("Crown joint shadow", (.296, seam.y, seam.z), (.025, .028, .24), dark, .003)

box("Lower continuous black sill", (-.12, 0, .19), (.69, 15.34, .36), black, .018)
box("Inset lower reveal", (.247, 0, .395), (.037, 15.10, .034), dark, .004)
box("Sill gold datum", (.27, 0, .377), (.018, 15.05, .013), gold, .002)
for i in range(9):
    y = -6.82 + i * 1.705
    box("Sill dressed stone leaf %02d" % i, (.245, y, .221), (.075, 1.58, .174), black, .008)
    if i < 8:
        box("Sill cut joint %02d" % i, (.286, y + .853, .22), (.016, .028, .225), dark, .003)
for side in (-1, 1):
    y = side * 7.20
    for z in (.71, 2.09, 3.47, 4.85):
        box("Inner ivory bearing", (.225, y, z), (.17, .24, .075), stone, .01)
        box("Bearing gold line", (.316, y, z + .043), (.01, .19, .008), gold, .001)

module = export("SM_Aurelion_KIT_Z11ObservationWindowSurround", [0.82, 17.88, 5.93])
manifest[-1].update(
    nominal_dimensions_m=[round(value, 6) for value in module.dimensions],
    collision="None: native Z11 wall and rail actors remain collision owners",
    position_precision=10,
    preserve_fallback_geometry=True,
)
(ROOT / "manifest.json").write_text(json.dumps(dict(modules=manifest), indent=2))

# An oblique studio render exposes the depth, fitted joints and open aperture.
import bpy

scene.world = bpy.data.worlds.new("Observation surround studio")
scene.world.color = (.065, .065, .07)
for location, power, size, color in [
    ((-5, -9, 7), 1800, 7, (1.0, .8, .62)),
    ((4, 9, 6), 1400, 7, (.65, .78, 1.0)),
]:
    bpy.ops.object.light_add(type="AREA", location=location)
    lamp = bpy.context.object
    lamp.data.energy = power
    lamp.data.size = size
    lamp.data.color = color
    lamp.rotation_euler = (Vector((0, 0, 3)) - lamp.location).to_track_quat("-Z", "Y").to_euler()
bpy.ops.object.camera_add(location=(15, -18, 9))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, 3)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 21
scene.camera = camera
scene.render.engine = "CYCLES"
scene.cycles.samples = 48
scene.cycles.use_denoising = True
scene.render.resolution_x = 1600
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.filepath = str(ROOT / "observation-window-surround.png")
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / "Aurelion-Observation-Window.blend"))
bpy.ops.render.render(write_still=True)
