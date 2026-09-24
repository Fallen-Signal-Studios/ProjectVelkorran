"""Blender 4.5 source for a rare Z12 service register beside berth windows.

The August Aurelion kit sheet and Z12 armored-view study precede this module.
It replaces selected 3.99 x .249 x 2.99 m visual coffers only; native wall
collision, the berth aperture and the departure route remain separate.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Vector

helper = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0], str(helper), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z12DepartureRegister'
ROOT.mkdir(exist_ok=True)
steel = material('M_Aurelion_DarkSteel', (.055, .061, .067), .72, .38)
black = material('M_Aurelion_BlackStone', (.024, .027, .031), .18, .46)
lens = material('M_Aurelion_LumenLens', (.83, .69, .43), .12, .22)

# Several instrument traces are millimetres thick. Cap their bevel to a
# fraction of the smallest dimension so Blender does not collapse side faces.
_base_box = box
def box(name, loc, size, mat=stone, bevel=.008):
    return _base_box(name, loc, size, mat, min(bevel, min(size)*.35))

# The original coffer has a 24.9 cm visual relief. This is a rare mechanical
# service bay within that same envelope, not another surface laid on top.
box('Continuous cut-stone backing', (0, .055, 0), (3.99, .11, 2.99), stone, .009)
box('Recessed mechanical field', (0, .119, .04), (3.56, .050, 2.52), black, .011)
for side in (-1, 1):
    x = side * 1.86
    box('Load-bearing stone jamb', (x, .177, 0), (.255, .142, 2.83), stone, .014)
    box('Keyed dark jamb recess', (x-side*.151, .212, 0), (.034, .030, 2.48), dark, .004)
    box('Functional vertical trace', (x-side*.151, .231, 0), (.012, .013, 2.34), gold, .003)
    for z in (-1.13, -.37, .37, 1.13):
        box('Jamb locking key', (x-side*.07, .244, z), (.105, .009, .052), steel, .004)
for z in (-1.38, 1.38):
    box('Stone course', (0, .171, z), (3.99, .155, .23), stone, .011)
    box('Inset gold datum', (0, .246, z+(.095 if z<0 else -.095)),
        (3.68, .006, .019), gold, .002)

# A black-stone instrument cassette and four readable physical quarters create
# depth at player eye height. The seams are geometry, not a glowing picture.
box('Inset instrument surround', (0, .150, .06), (2.72, .050, 1.79), steel, .018)
box('Cropped black-stone register bed', (0, .180, .06), (2.48, .029, 1.56), black, .015)
for side in (-1, 1):
    x = side * .94
    for z in (-.51, .63):
        box('Machined quarter cassette', (x, .209, z), (.55, .066, .47), stone, .021)
        box('Quarter cassette shadow', (x, .243, z), (.43, .007, .31), dark, .005)
        box('Quarter conductor', (x, .247, z-.09), (.31, .003, .016), gold, .001)
        for key_x in (-.16, .16):
            box('Captive cassette fastener', (x+key_x, .247, z+.09),
                (.023, .002, .023), steel, .001)
    path('Angled instrument feed', [(side*1.53,.190,-1.13),
                                   (side*1.53,.190,-.78),
                                   (side*1.32,.190,-.57),
                                   (side*1.32,.190,.71),
                                   (side*1.53,.190,.92),
                                   (side*1.53,.190,1.10)],
         .068, .018, dark, .004)
    path('Gold feed contact', [(side*1.53,.214,-1.12),
                               (side*1.53,.214,-.78),
                               (side*1.32,.214,-.57),
                               (side*1.32,.214,.71),
                               (side*1.53,.214,.92),
                               (side*1.53,.214,1.09)],
         .021, .010, gold, .002)
    for i in range(7):
        box('Lower service vent', (side*1.16, .201, -1.08+i*.068),
            (.37, .012, .025), steel, .003)

# The concentric register is a local wayfinding/mechanism cue, deliberately
# unlettered so it cannot compete with objectives or imply a new interaction.
box('Register central relief', (0, .190, .06), (.79, .039, .79), steel, .019)
for radius, width, mat in ((.39,.048,stone),(.31,.014,gold),(.255,.039,dark),(.16,.011,gold)):
    ring('Nested concurrence index',0,.222,.06,radius,width,mat)
for i in range(12):
    angle=i*math.tau/12
    x=.335*math.sin(angle); z=.06+.335*math.cos(angle)
    indicator=box('Register captive index',(x,.243,z),(.019,.010,.057),gold,.002)
    indicator.rotation_euler.y=angle
box('Solid keyed center', (0,.238,.06), (.11,.018,.11), stone, .006)
for x in (-.53,.53):
    box('Low-power status lens', (x,.240,-.78), (.12,.017,.056), lens, .004)
    box('Lens safety rim', (x,.246,-.78), (.14,.006,.071), steel, .002)

# Blender-to-FBX handedness mirrors Y as on the original departure coffers.
for part in parts:
    part.location.y *= -1
    part.scale.y *= -1
mesh = export('SM_Aurelion_KIT_Z12ServiceRegister',[3.99,.249,2.99])
manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in mesh.dimensions],
                    preserve_fallback_geometry=True,
                    wall_group='Side',
                    collision='None: visual coffer replacement; native wall collision retained',
                    story_role='Rare ancient berth-side service register; no gameplay interaction')
(ROOT/'manifest.json').write_text(json.dumps(dict(
    modules=manifest,
    visual_envelope_m=[3.99,.249,2.99],
    source_references=[
        'Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Armored-View-Module.png',
        'Docs/ArtReferences/AurelionArchitecture-2026-09-23/Aurelion-Modular-Kit-Sheet.png'],
    placement='Candidate for lower side coffers immediately flanking the two berth windows'
),indent=2))

scene.world=bpy.data.worlds.new('Z12 service register studio')
scene.world.color=(.14,.14,.14)
for pos,power,size in [((-3,-5,5),1900,5),((4,-4,2),1400,4),((0,3,4),1700,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos)
    light=bpy.context.object;light.data.energy=power;light.data.size=size
    light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(3.8,-9,2.5))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=5.6;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1500;scene.render.resolution_y=1100
scene.render.filepath=str(ROOT/'service-register-source.png')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z12-Service-Register.blend'))
bpy.ops.render.render(write_still=True)
print('Z12_SERVICE_REGISTER_SOURCE_PASS', manifest[-1]['triangles'])
