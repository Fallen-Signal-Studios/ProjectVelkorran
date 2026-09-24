"""Measured Z11 window-pier cladding; visual shell around the retained 1 x 1.2 x 6 m rib.

The mesh is not structural and owns no collision. In Unreal it must be placed at
Z11_Rib_1_-1's exact pivot after a player-height preview.
"""
from pathlib import Path
import json

from mathutils import Vector

source = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0], str(source), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z11ObservationPier'
ROOT.mkdir(exist_ok=True)

black = material('M_Aurelion_ObservationBlackStone', (.021,.025,.031), .10, .32)

base_box = box
def box(name, location, size, mat=stone, bevel=.006):
    return base_box(name, location, size, mat, min(bevel, min(size)*.4))


# An opaque skin and shallow relief close to the native rib keep the authored physical
# plane and the nearby stellar window surround readable. The stock cube remains
# inside as the sole collision owner; its six-metre height is not changed.
box('Complete fitted ivory skin', (0,0,2.965), (1.08,1.28,5.93), stone, .013)
box('Front inset black stone spine', (.555,0,3.01), (.035,.275,5.55), black, .006)
box('Back inset black stone spine', (-.555,0,3.01), (.035,.275,5.55), black, .006)

for face in (-1,1):
    x = face*.555
    # Broad age-cut stone courses, separated by true shadow joints rather than
    # painted surface lines. The front has the same language as the rear.
    for course in range(5):
        z = .69+course*1.10
        for side in (-1,1):
            box('Fitted pier course', (x,side*.348,z), (.048,.373,1.048), stone, .010)
            box('Course keyed arris', (x+face*.029,side*.527,z), (.015,.035,.99), dark, .002)
        box('Horizontal course witness', (x+face*.029,0,z+.537),
            (.014,1.06,.012), dark, .0015)
        for side in (-1,1):
            box('Captive course pin', (x+face*.042,side*.510,z+.455),
                (.007,.025,.025), gold, .001)
    for side in (-1,1):
        box('Fine conductor shadow', (x+face*.033,side*.153,3.0),
            (.014,.024,5.43), dark, .001)
        box('Functional gold conductor', (x+face*.042,side*.153,3.0),
            (.009,.010,5.38), gold, .001)
    # Two rare contacts break the run. They are mechanical keys, not glowing
    # emblems, so the red/Wound/white exterior remains the visual focus.
    for z in (2.17,4.34):
        box('Pier keyed latch bed', (x+face*.035,0,z),
            (.035,.40,.110), dark, .003)
        box('Pier ivory latch', (x+face*.046,0,z),
            (.045,.36,.060), stone, .006)
        for side in (-1,1):
            box('Latch gold contact', (x+face*.059,side*.145,z),
                (.007,.036,.022), gold, .001)

# Side elevations: smaller relief so this remains one quiet conversation-room
# support when seen obliquely from the doorway.
for face in (-1,1):
    y = face*.655
    for side in (-1,1):
        box('Side recessed black flute', (side*.295,y,3.03),
            (.055,.033,5.50), black, .004)
        box('Side stone edge leaf', (side*.455,y+face*.014,3.03),
            (.075,.040,5.50), stone, .006)
    box('Side axial hairline', (0,y+face*.022,3.02),
        (.018,.009,5.39), gold, .001)
    for z in (1.32,2.42,3.52,4.62):
        box('Side keyed seam', (0,y+face*.028,z),
            (.75,.008,.012), dark, .001)

# Bearing is stepped, but never more than eight centimetres beyond the native
# rib footprint. These ends visually lock into the new 5.6 m coffer soffit.
for z,w,d,h,mat in ((.075,1.16,1.36,.15,black),
                    (.22,1.13,1.33,.14,stone),
                    (.37,1.10,1.30,.08,stone),
                    (5.54,1.11,1.31,.09,stone),
                    (5.73,1.14,1.34,.17,black),
                    (5.875,1.16,1.36,.12,stone)):
    box('Fitted bearing collar', (0,0,z), (w,d,h), mat, .009)
for face in (-1,1):
    for z in (.28,5.805):
        box('Bearing narrow gold datum', (face*.584,0,z),
            (.010,1.10,.009), gold, .001)

module = export('SM_Aurelion_KIT_Z11ObservationPierCladding', [1.25,1.38,5.935])
manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in module.dimensions],
                   collision='None: existing Z11_Rib_1_-1 cube remains sole structural collision',
                   measured_native_envelope_m=[1.0,1.2,6.0],
                   authored_max_footprint_m=[1.25,1.38],
                   preserve_fallback_geometry=True)
(ROOT/'manifest.json').write_text(json.dumps(dict(
    reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z11-Observation-Pier.png',
    survey='Saved/Validation/Aurelion/Z11PierSurveyWide-20260924-053530-80677523/z11-pier-survey.json',
    placement_status='Source candidate: Unreal fit and player-height review required',
    modules=manifest),indent=2),encoding='utf-8')

import bpy
scene.world=bpy.data.worlds.new('Z11 observation pier studio')
scene.world.color=(.075,.075,.08)
for pos,power,color in (((4,-6,7),1800,(1,.70,.50)),
                        ((-4,5,5),1500,(.55,.75,1)),
                        ((2,3,8),900,(1,.94,.78))):
    bpy.ops.object.light_add(type='AREA',location=pos)
    light=bpy.context.object;light.data.energy=power;light.data.size=3
    light.data.color=color
    light.rotation_euler=(Vector((0,0,3))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(8,-10,6))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,3))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=8
scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1050;scene.render.resolution_y=1450
scene.render.filepath=str(ROOT/'pier-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z11-Observation-Pier.blend'))
bpy.ops.render.render(write_still=True)
print('Z11_OBSERVATION_PIER_SOURCE_COMPLETE')
