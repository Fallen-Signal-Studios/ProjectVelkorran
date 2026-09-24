"""Blender 4.5 source for the two visual-only Z12 shuttle-berth rear wall bays.

Each 4.15 m bay fits between the saved canopy rib stations. The dock floor,
shuttles, canopy, protected concourse glass and gameplay collision stay owned
by the existing M13 map. No synthetic collision is exported with these bays.
"""
import json
from pathlib import Path

source = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0], str(source), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z12BerthBulkhead'
ROOT.mkdir(exist_ok=True)

steel = material('M_Aurelion_DarkSteel', (.055, .061, .067), .72, .38)
black = material('M_Aurelion_BlackStone', (.024, .027, .031), .18, .46)
lens = material('M_Aurelion_LumenLens', (.83, .69, .43), .12, .22)


def bay(variant):
    # Front faces local -Y. Origin is the center of the bearing line at deck Z0.
    box('Solid service-wall core', (0, .12, 3.75), (4.11, .30, 7.50), black, .009)
    box('Inset center cassette', (0, -.104, 3.40), (3.56, .15, 5.88), steel, .018)
    for x in (-1.81, 1.81):
        box('Bearing arris', (x, -.205, 3.39), (.32, .43, 6.56), stone, .021)
        box('Stone inner quoin', (x * .884, -.443, 3.32), (.078, .057, 6.30), stone, .009)
        box('Captive service track', (x * .855, -.485, 3.30), (.024, .027, 5.98), gold, .003)
        for z in (.36, 1.78, 3.20, 4.62, 6.04):
            box('Masonry course witness', (x, -.438, z), (.304, .014, .018), dark, .002)
        for z in (1.14, 3.18, 5.22):
            box('Machined bearing pin', (x, -.453, z), (.065, .03, .090), gold, .006)

    for z, width, depth, height, mat in (
        (.09, 4.15, .75, .18, black),
        (.25, 4.11, .66, .14, stone),
        (.43, 4.00, .55, .14, steel),
        (6.72, 4.10, .56, .18, black),
        (6.92, 4.15, .73, .20, stone),
        (7.11, 4.10, .64, .16, steel),
        (7.33, 4.15, .67, .18, stone),
        (7.49, 4.15, .73, .16, black),
    ):
        box('Layered footing or canopy joint', (0, -.12, z),
            (width, depth, height), mat, .012)
    for x in (-1.16, 1.16):
        box('Upper stone coffer cheek', (x, -.355, 7.18), (.48, .24, .42), stone, .018)
        box('Coffer low-glare contact', (x, -.494, 7.16), (.23, .035, .055), lens, .006)
    box('Coffer deep relief', (0, -.265, 7.18), (1.78, .12, .42), black, .012)
    for x in (-.62, 0, .62):
        box('Coffer index line', (x, -.340, 7.18), (.025, .015, .27), gold, .003)

    # The panels are physically layered. Narrow conduits describe the route of
    # service power; they are not giant emissive signs or fake doors.
    for side in (-1, 1):
        x = side * 1.34
        box('Service seam shadow', (x, -.214, 3.45), (.056, .025, 5.71), dark, .004)
        box('Sealed conductor', (x, -.238, 3.45), (.017, .020, 5.48), gold, .003)
        for z in (.95, 2.42, 3.89, 5.36):
            box('Cassette stone keeper', (x - side * .23, -.278, z),
                (.34, .075, .09), stone, .008)
            box('Keeper captive fastener', (x - side * .23, -.323, z),
                (.048, .020, .037), gold, .003)

    if variant == 'Service':
        for z in (1.50, 3.48, 5.46):
            box('Removable service leaf', (0, -.253, z),
                (2.17, .084, 1.78), black, .015)
            box('Inset equipment field', (0, -.309, z),
                (1.82, .025, 1.42), steel, .009)
            for side in (-1, 1):
                box('Cassette captive rail', (side * .78, -.331, z),
                    (.026, .024, 1.26), gold, .004)
                for dz in (-.54, .54):
                    box('Recessed latch', (side * .87, -.343, z + dz),
                        (.075, .021, .11), stone, .005)
        ring('Sealed service register outer', 0, -.362, 3.48, .43, .078, stone)
        ring('Sealed service register inner', 0, -.392, 3.48, .31, .024, gold)
        box('Register protected lens', (0, -.398, 3.48),
            (.30, .022, .30), black, .004)
    else:
        for z in (1.31, 3.25, 5.19):
            box('Quiet dark wall field', (0, -.250, z),
                (2.15, .073, 1.70), black, .012)
            for side in (-1, 1):
                box('Stone panel cheek', (side * .97, -.303, z),
                    (.16, .09, 1.66), stone, .009)
            box('Sparse central witness', (0, -.306, z),
                (.45, .021, .026), gold, .003)

    asset = 'SM_Aurelion_KIT_Z12BerthBulkhead' + variant
    mesh = export(asset, [4.15, .73, 7.57])
    manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in mesh.dimensions]
    manifest[-1].update(collision='None: scenic rear wall outside the shuttle and playable concourse',
                        preserve_fallback_geometry=True)
    return mesh


plain = bay('Plain')
plain.hide_render = True
service = bay('Service')

scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.render.resolution_x = 1600
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
# Editable source includes both production bays. The preview is intentionally a
# simple source check; Unreal placement and production-lighting review decide fit.
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / 'Aurelion-Berth-Bulkhead.blend'))
(ROOT / 'manifest.json').write_text(json.dumps(dict(
    source='Blender 4.5 procedural editable kit',
    design_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Berth-Bulkhead.png',
    modules=manifest,
    bay_width_m=4.15, bay_height_m=7.57, dock_envelope_m=[28, 18],
    collision='Visual only: existing dock and protected concourse collision unchanged',
), indent=2))
print('Z12_BERTH_BULKHEAD_SOURCE_PASS', [(m['asset'], m['triangles']) for m in manifest])
