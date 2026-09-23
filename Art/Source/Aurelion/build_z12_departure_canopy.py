"""Blender 4.5 source for a modular, noncolliding M13 departure-dock canopy.

The 28 x 18 m berth, 5 m unobstructed middle lane and existing shuttles remain
owned by the M13 map. This produces architectural dress only; engine placement
and material/lighting acceptance are separate steps.
"""
import json
import math
from pathlib import Path

from mathutils import Vector

source = Path(__file__).with_name("build_architecture_kit.py")
exec(compile(source.read_text().split("# Four metre bay:")[0], str(source), "exec"))
ROOT = Path(__file__).resolve().parent / "Z12DepartureCanopy"
ROOT.mkdir(exist_ok=True)
black = material("M_Aurelion_BlackStone", (.024, .027, .031), .18, .46)
steel = material("M_Aurelion_DarkSteel", (.055, .061, .067), .72, .32)
lens = material("M_Aurelion_LumenLens", (.83, .69, .43), .12, .22)
lens_bsdf = lens.node_tree.nodes.get('Principled BSDF')
lens_bsdf.inputs['Emission Color'].default_value = (.83, .69, .43, 1)
lens_bsdf.inputs['Emission Strength'].default_value = 3.5


def dressed_bar(name, first, last, depth, height, mat, bevel=.014):
    """Bevelled voussoir in the X/Z vault profile, with Y as bay depth."""
    a, b = Vector(first), Vector(last)
    middle = (a + b) * .5
    delta = b - a
    obj = box(name, middle, (delta.length, depth, height), mat, bevel)
    obj.rotation_euler.y = -math.atan2(delta.z, delta.x)
    return obj


def vault_height(x):
    # Shallow king-era vault: 7.5 m at the bearing, 8.7 m at the crown.
    return 7.5 + 1.2 * (1.0 - (x / 13.2) ** 2)


# One real-size 28 m portal rib, designed to repeat at 4.15 m bay spacing.
# Separate deep backing, cut ivory voussoirs, metal bearing and light channel
# make the structural reading work from both the concourse and side dock.
for index in range(28):
    x0 = -13.2 + index * 26.4 / 28
    x1 = -13.2 + (index + 1) * 26.4 / 28
    a = (x0, 0, vault_height(x0))
    b = (x1, 0, vault_height(x1))
    dressed_bar("Rib steel spine %02d" % index, a, b, .86, .69, steel, .021)
    dressed_bar("Cut ivory vault stone %02d" % index,
                (x0, -.02, a[2] - .085), (x1, -.02, b[2] - .085),
                .67, .40, stone, .018)
    dressed_bar("Underside shadow groove %02d" % index,
                (x0, -.015, a[2] - .312), (x1, -.015, b[2] - .312),
                .50, .044, dark, .005)
    dressed_bar("White gold service trace %02d" % index,
                (x0, -.019, a[2] - .338), (x1, -.019, b[2] - .338),
                .27, .027, gold, .004)
    if index % 4 == 0 and index:
        x = x0
        z = vault_height(x)
        box("Vault movement joint %02d" % index, (x, 0, z - .05),
            (.065, .93, .75), black, .008)
        box("Joint bearing pin %02d" % index, (x, -.49, z - .05),
            (.048, .065, .23), gold, .004)

for side in (-1, 1):
    x = side * 13.2
    z = vault_height(x)
    box("Rib end bearing", (x, 0, z - .31), (1.05, 1.10, .70), black, .022)
    box("Ivory bearing shoe", (x, 0, z - .62), (.94, .98, .16), stone, .012)
    for offset in (-.38, .38):
        box("Pinned bearing lug", (x + offset, -.55, z - .29),
            (.12, .08, .20), gold, .006)

rib = export("SM_Aurelion_KIT_Z12CanopyVaultRib", [27.45, 1.11, 1.38])
manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in rib.dimensions]
manifest[-1].update(
    design_span_m=26.4, design_bearing_height_m=7.5,
    design_crown_height_m=8.7,
    collision="None: dock floor and any structural map proxies remain authoritative",
    preserve_fallback_geometry=True,
)

# Special entrance rib: the repeated silhouette is retained, with a shallow
# mechanical crown key. This is a separate named module, never a logo slab.
copy = rib.copy()
copy.data = rib.data.copy()
scene.collection.objects.link(copy)
parts.append(copy)
box("Terminal crown socket", (0, -.52, 8.42), (1.34, .15, .43), black, .019)
box("Terminal cut ivory key", (0, -.63, 8.42), (1.14, .08, .29), stone, .013)
for x in (-.42, -.21, 0, .21, .42):
    box("Fivefold key register", (x, -.685, 8.41 + .13 * (1 - abs(x) / .42)),
        (.055, .032, .12), gold, .005)
# The reused joined rib already carries UV0/UV1. Export projects a fresh pair;
# remove the inherited layers so the special module has the same two-channel
# contract as every other kit piece.
for part in parts:
    for layer in list(part.data.uv_layers):
        part.data.uv_layers.remove(layer)
end_rib = export("SM_Aurelion_KIT_Z12CanopyTerminalRib", [27.45, 1.20, 1.38])
manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in end_rib.dimensions]
manifest[-1].update(
    design_span_m=26.4, design_bearing_height_m=7.5,
    design_crown_height_m=8.7,
    collision="None: visual entrance rib; no dock collision change",
    preserve_fallback_geometry=True,
)

# Independent 7.5 m feet permit exact fitting around the existing shuttle.
# Nothing projects into the 5 m central path or beyond the dock's 28 m width.
for z, width, depth, height, mat in (
        (.13, 1.55, 1.44, .26, black), (.31, 1.39, 1.29, .19, stone),
        (.52, 1.18, 1.13, .21, steel), (3.88, .94, .91, 6.52, black),
        (7.08, 1.17, 1.10, .33, steel), (7.34, 1.41, 1.31, .21, stone),
        (7.51, 1.56, 1.44, .15, black)):
    box("Bearing and stepped capital", (0, 0, z), (width, depth, height), mat, .019)
for side in (-1, 1):
    for z in (.90, 2.25, 3.60, 4.95, 6.30):
        box("Fitted foot ashlar", (side * .49, 0, z), (.13, .76, 1.24), stone, .015)
        box("Ashlar witness joint", (side * .566, 0, z + .60),
            (.024, .70, .017), dark, .003)
    box("Vertical service recess", (side * .58, -.33, 3.72),
        (.046, .13, 5.56), dark, .008)
    box("Recessed conductor", (side * .611, -.33, 3.72),
        (.017, .035, 5.45), gold, .003)
for face in (-1, 1):
    y = face * .459
    for z in (1.04, 2.53, 4.02, 5.51):
        box("Foot removable service cassette", (0, y, z),
            (.67, .075, 1.34), stone, .018)
        box("Cassette inset field", (0, y + face * .043, z),
            (.51, .025, 1.08), steel, .009)
        for dx in (-.21, .21):
            box("Cassette longitudinal arris", (dx, y + face * .061, z),
                (.026, .021, .92), gold, .003)
        for dz in (-.47, .47):
            for dx in (-.22, .22):
                box("Service captive fastener", (dx, y + face * .064, z + dz),
                    (.037, .021, .037), gold, .003)
    for z in (6.65, 6.76, 6.87):
        box("Capital ventilation blade", (0, y + face * .052, z),
            (.70, .025, .034), gold, .004)
foot = export("SM_Aurelion_KIT_Z12CanopyBearingFoot", [1.56, 1.44, 7.585])
manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in foot.dimensions]
manifest[-1].update(
    collision="None: visual foot only, sited outside existing shuttle and route",
    preserve_fallback_geometry=True,
)

# Four-metre ceiling cassette: three-by-three machined coffer cells. The low
# ivory lip surrounds a higher dark recess; the panel has two exported UV sets.
box("Cassette upper steel carrier", (0, 0, .31), (3.98, 4.13, .18), steel, .013)
for x in (-1.94, 1.94):
    box("Long ivory perimeter", (x, 0, .12), (.10, 4.13, .23), stone, .012)
    box("Long gold register", (x - math.copysign(.064, x), 0, .014),
        (.021, 4.06, .012), gold, .002)
for y in (-2.015, 2.015):
    box("End ivory perimeter", (0, y, .12), (3.93, .10, .23), stone, .012)
    box("End gold register", (0, y - math.copysign(.064, y), .014),
        (3.85, .021, .012), gold, .002)
for i in range(3):
    for j in range(3):
        x = (i - 1) * 1.27
        y = (j - 1) * 1.31
        box("Coffer dark reveal %d-%d" % (i, j), (x, y, .193),
            (1.13, 1.17, .045), black, .011)
        for sx in (-1, 1):
            box("Coffer ivory cheek", (x + sx * .575, y, .105),
                (.105, 1.23, .17), stone, .009)
            box("Cheek root seam", (x + sx * .514, y, .022),
                (.014, 1.13, .012), dark, .002)
        for sy in (-1, 1):
            box("Coffer ivory cross rail", (x, y + sy * .600, .105),
                (1.15, .085, .17), stone, .009)
        box("Coffer conductor index", (x, y, .164),
            (.36, .030, .010), gold, .002)
        for sx in (-1, 1):
            box("Coffer captive pin", (x + sx * .42, y + .43, .092),
                (.035, .035, .016), gold, .002)
cassette = export("SM_Aurelion_KIT_Z12CanopyCoffer_4x4", [3.98, 4.13, .40])
manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in cassette.dimensions]
manifest[-1].update(
    design_cell_count=9,
    collision="None: overhead visual cassette, existing dock collision retained",
    preserve_fallback_geometry=True,
)

# Slender Sovereign light standard, hung from the underside at each side-bay.
# The 2.3 m pendant stays at least 4.9 m above the scenic dock and outside the
# shuttle wings; it uses the project light-lens material when imported.
box("Pendant broad ceiling mount", (0, 0, -.08), (.70, .49, .16), stone, .015)
box("Pendant black socket", (0, 0, -.22), (.50, .36, .16), black, .012)
for x in (-.18, .18):
    box("Pendant tension rod", (x, 0, -1.17), (.055, .055, 1.72), gold, .006)
for z in (-.33, -2.03):
    box("Pendant collar", (0, 0, z), (.59, .36, .12), steel, .009)
    box("Pendant collar inset", (0, -.192, z), (.43, .027, .045), gold, .004)
for face in (-1, 1):
    y = face * .178
    box("Pendant continuous lens", (0, y, -1.18), (.37, .044, 1.58), lens, .014)
    for x in (-.235, .235):
        box("Pendant lens guard", (x, y + face * .033, -1.18),
            (.032, .046, 1.69), steel, .004)
    for z in (-1.88, -.48):
        box("Pendant captive register", (0, y + face * .032, z),
            (.47, .035, .026), gold, .003)
box("Pendant machined drop", (0, 0, -2.17), (.43, .29, .18), black, .013)
fixture = export("SM_Aurelion_KIT_Z12CanopyPendant", [.70, .50, 2.3])
manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in fixture.dimensions]
manifest[-1].update(
    collision="None: overhead scenic fixture, no dock collision or gameplay light",
    preserve_fallback_geometry=True,
)

(ROOT / "manifest.json").write_text(json.dumps(dict(
    source="Blender 4.5 procedural editable kit",
    design_reference="Art/References/Aurelion/Z12DepartureCanopy/Z12-Departure-Canopy-Concept.png",
    modules=manifest,
    dock_assembly=dict(berth_m=[28, 18], rib_stations_y_m=[-8.3, -4.15, 0, 4.15, 8.3],
                       foot_x_m=[-13.2, 13.2], center_clear_lane_m=5.0,
                       coffer_x_m=[-12, -8, -4, 0, 4, 8, 12],
                       coffer_y_m=[-6.225, -2.075, 2.075, 6.225],
                       pendant_x_m=[-12.55, 12.55],
                       pendant_y_m=[-6.225, -2.075, 2.075, 6.225])), indent=2), encoding="utf-8")

# Source-studio assembly. The exported modules stay hidden at the origin;
# linked preview instances can be moved without changing any FBX source.
for module in modules:
    module.hide_render = True
for y in (-8.3, -4.15, 0, 4.15, 8.3):
    source_rib = end_rib if abs(y + 8.3) < .01 else rib
    inst = source_rib.copy()
    inst.data = source_rib.data
    scene.collection.objects.link(inst)
    inst.name = "PREVIEW portal station %+.2f" % y
    inst.location.y = y
    inst.hide_render = False
    for x in (-13.2, 13.2):
        support = foot.copy()
        support.data = foot.data
        scene.collection.objects.link(support)
        support.name = "PREVIEW bearing %+.2f %+.2f" % (x, y)
        support.location = (x, y, 0)
        support.hide_render = False
for x in (-12, -8, -4, 0, 4, 8, 12):
    for y in (-6.225, -2.075, 2.075, 6.225):
        panel = cassette.copy()
        panel.data = cassette.data
        scene.collection.objects.link(panel)
        panel.name = "PREVIEW coffer %+.1f %+.3f" % (x, y)
        panel.location = (x, y, vault_height(x) + .13)
        panel.rotation_euler.y = math.atan(2.4 * x / (13.2 ** 2))
        panel.hide_render = False
for x in (-12.55, 12.55):
    for y in (-6.225, -2.075, 2.075, 6.225):
        pendant = fixture.copy()
        pendant.data = fixture.data
        scene.collection.objects.link(pendant)
        pendant.name = "PREVIEW pendant %+.2f %+.3f" % (x, y)
        pendant.location = (x, y, vault_height(x) - .1)
        pendant.hide_render = False

box("PREVIEW nonexported basalt berth", (0, 0, -.16), (28, 18, .32), black, .018).hide_render = False
for x in (-2.52, 2.52):
    box("PREVIEW lane boundary", (x, 0, .008), (.028, 17.8, .017), gold, .002).hide_render = False
for obj in parts:
    obj.hide_render = False
parts.clear()

import bpy
scene.world = bpy.data.worlds.new("Z12 canopy studio")
scene.world.color = (.025, .031, .048)
for pos, power, size, color in [
        ((-17, -12, 19), 4100, 15, (1.0, .80, .57)),
        ((15, 8, 16), 3300, 14, (.67, .78, 1.0)),
        ((0, 1, 2), 2200, 9, (1.0, .91, .70))]:
    bpy.ops.object.light_add(type="AREA", location=pos)
    light = bpy.context.object
    light.data.energy = power
    light.data.size = size
    light.data.color = color
    light.rotation_euler = (Vector((0, 0, 4)) - light.location).to_track_quat("-Z", "Y").to_euler()
bpy.ops.object.camera_add(location=(17, -25, 4.5))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, 5.8)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 36
scene.camera = camera
scene.render.engine = "CYCLES"
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x = 1800
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.filepath = str(ROOT / "departure-canopy-source.png")
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / "Aurelion-Departure-Canopy.blend"))
bpy.ops.render.render(write_still=True)
