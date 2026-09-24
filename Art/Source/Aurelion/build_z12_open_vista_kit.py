"""Blender 4.5 source for sparse 3D Aurelion structures beyond the M13 berths.

The kit leaves the red/white remnant and closed Wound sightlines open. These
scenic meshes have no collision or gameplay role and must be reviewed behind
the saved dock canopy and protective glass before M13 placement.
"""
import json
import math
from pathlib import Path

import bpy
from mathutils import Vector

source = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0], str(source), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z12OpenVistaKit'
ROOT.mkdir(exist_ok=True)
black = material('M_Aurelion_BlackStone', (.024, .027, .031), .18, .46)
steel = material('M_Aurelion_DarkSteel', (.055, .061, .067), .72, .38)
lens = material('M_Aurelion_LumenLens', (.83, .69, .43), .12, .22)


def octagon(radius_x, radius_y):
    return [(-radius_x*.72, -radius_y), (radius_x*.72, -radius_y),
            (radius_x, -radius_y*.72), (radius_x, radius_y*.72),
            (radius_x*.72, radius_y), (-radius_x*.72, radius_y),
            (-radius_x, radius_y*.72), (-radius_x, -radius_y*.72)]


def tapered_shaft(name, rings, mat):
    vertices = [(x, y, z) for z, rx, ry in rings for x, y in octagon(rx, ry)]
    faces = [tuple(reversed(range(8))), tuple(range((len(rings)-1)*8, len(rings)*8))]
    for band in range(len(rings)-1):
        for i in range(8):
            nxt = (i+1) % 8
            faces.append((band*8+i, band*8+nxt, (band+1)*8+nxt, (band+1)*8+i))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(obj)
    return finish(obj, mat, 0)


def spire(name, height, offset):
    # Black-stone monolith with unequal upper facets; no ecclesiastical cap.
    rings = [(0, 1.82, 1.58), (.50, 1.74, 1.50), (2.65, 1.20, 1.10),
             (height*.38, .92, .84), (height*.70, .53, .48),
             (height*.94, .27, .25), (height, .075, .07)]
    tapered_shaft('Faceted load-bearing spire', rings, black)
    for z, wx, wy, thick, mat in (
        (.15, 4.45, 3.96, .30, steel), (.36, 4.24, 3.76, .14, stone),
        (.70, 3.55, 3.10, .16, steel), (2.66, 2.65, 2.42, .16, stone),
        (2.85, 2.45, 2.25, .10, steel),
        (height*.38, 1.99, 1.85, .16, stone),
        (height*.70, 1.16, 1.09, .11, steel)):
        box('Keyed structural collar', (0, 0, z), (wx, wy, thick), mat, .018)
    for side in (-1, 1):
        x = side * 1.34
        box('Splayed stone footing', (x, 0, 1.28), (.48, 2.30, 2.18), stone, .022)
        box('Footing inset black field', (x + side*.245, -.78, 1.35),
            (.048, .36, 1.46), black, .006)
        for z in (.97, 1.47, 1.97):
            box('Footing witness seam', (x + side*.25, -.81, z),
                (.019, .30, .017), gold, .003)
    # Two face conductors taper with the actual shaft rather than floating in
    # front of it. Deliberately thin: information traces, not neon decoration.
    for side in (-1, 1):
        points = []
        for z, rx, ry in rings[1:-1]:
            points.append((side*rx*.47, -ry-.022, z))
        path('Conductor in shaft reveal', points, .045, .023, gold, .004)
        for z in (height*.38, height*.70):
            rx = .92 if z < height*.5 else .53
            ry = .84 if z < height*.5 else .48
            box('Removable observation key', (side*rx*.44, -ry-.060, z+.20),
                (.13, .045, .32), stone, .007)
    for z in (4.2, 6.7, 9.2):
        box('Lower repair cassette', (0, -1.02+z*.018, z),
            (.77, .08, .68), steel, .009)
        box('Cassette latch', (0, -1.083+z*.018, z),
            (.36, .025, .038), gold, .003)
    for side in (-1, 1):
        # Bridge socket intentionally wraps the shaft instead of making a
        # traversable platform. The final span is wholly scenic.
        box('Flying-bridge ivory socket', (0, side*.91, 14.0),
            (2.60, .42, 1.12), stone, .026)
        box('Socket machined throat', (0, side*1.15, 14.0),
            (1.91, .075, .48), steel, .009)
        for x in (-.67, .67):
            box('Socket captive key', (x, side*1.20, 14.0),
                (.080, .030, .32), gold, .005)
    asset = 'SM_Aurelion_KIT_Z12Vista' + name
    obj = export(asset, [4.45, 3.96, height+.075])
    manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in obj.dimensions]
    manifest[-1].update(collision='None: exterior scenic silhouette, no player route',
                        preserve_fallback_geometry=True, story_role='Framing; central remnant/Wound view remains open')
    return obj


tall = spire('SpireTall', 23.5, 0)
tall.hide_render = True
short = spire('SpireShort', 18.5, 0)
short.hide_render = True


def arch_height(x):
    return .62 * (1 - (x/6.15)**2)


# Twelve-metre, lightly rising arc. An independent visual object lets the
# designer omit the span wherever it competes with the stellar remnant.
for i in range(24):
    x0 = -6.15 + i*12.3/24
    x1 = -6.15 + (i+1)*12.3/24
    z0 = arch_height(x0)
    z1 = arch_height(x1)
    center = Vector(((x0+x1)*.5, 0, (z0+z1)*.5))
    length = math.hypot(x1-x0, z1-z0)
    for dz, depth, h, mat in ((0, 1.13, .43, stone),
                              (-.30, .94, .17, steel),
                              (-.44, .60, .075, black),
                              (-.49, .18, .025, gold)):
        beam = box('Segmented flying-bridge rib', center + Vector((0, 0, dz)),
                   (length+.012, depth, h), mat, .008)
        beam.rotation_euler.y = -math.atan2(z1-z0, x1-x0)
    if i % 4 == 0:
        x = x0
        box('Expansion joint clasp', (x, 0, z0-.14),
            (.055, 1.28, .76), black, .006)
        box('Clasp gold witness', (x, -.67, z0-.17),
            (.017, .023, .30), gold, .003)
for x in (-6.0, 0, 6.0):
    box('Under-span pendant key', (x, 0, arch_height(x)-1.04),
        (.25, .28, .85), steel, .009)
    box('Key end light contact', (x, -.16, arch_height(x)-1.41),
        (.11, .026, .16), lens, .004)
span = export('SM_Aurelion_KIT_Z12VistaFlyingBridge', [12.31, 1.31, 1.8])
manifest[-1]['nominal_dimensions_m'] = [round(v, 5) for v in span.dimensions]
manifest[-1].update(collision='None: distant visual bridge, never player traversable',
                    preserve_fallback_geometry=True,
                    story_role='Sparse connector; can be omitted to keep remnant clear')

bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / 'Aurelion-Open-Vista-Kit.blend'))
(ROOT / 'manifest.json').write_text(json.dumps(dict(
    source='Blender 4.5 procedural editable kit',
    design_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Open-Berth-Vistas.png',
    modules=manifest, dock_envelope_m=[28, 18],
    story_invariant='Open red remnant / closed Wound / white remnant view; no sealed rear wall',
), indent=2))
print('Z12_OPEN_VISTA_SOURCE_PASS', [(m['asset'], m['triangles']) for m in manifest])
