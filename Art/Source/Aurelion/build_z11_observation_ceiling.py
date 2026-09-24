"""Four-metre Z11 observation ceiling cassettes, authored as visual-only roof art.

Each tile fits the measured 6 x 4 stock ceiling grid. The pivot is the lowest
soffit point; placement at Z=560 cm keeps 5.6 m of clear room height.
"""
from pathlib import Path
import json
import math

source = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0], str(source), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z11ObservationCeiling'
ROOT.mkdir(exist_ok=True)

black = material('M_Aurelion_ObservationBlackStone', (.021, .025, .031), .10, .32)
contact = material('M_Aurelion_ObservationContact', (.63, .43, .20), .30, .22)
p = contact.node_tree.nodes.get('Principled BSDF')
p.inputs['Emission Color'].default_value = (.95, .67, .35, 1)
p.inputs['Emission Strength'].default_value = .7

base_box = box
def box(name, loc, size, mat=stone, bevel=.006):
    return base_box(name, loc, size, mat, min(bevel, min(size) * .4))

def segment(name, a, b, z, width, height, mat, bevel=.0015):
    x0, y0 = a; x1, y1 = b
    obj = box(name, ((x0+x1)*.5, (y0+y1)*.5, z),
              (math.hypot(x1-x0, y1-y0), width, height), mat, bevel)
    obj.rotation_euler.z = math.atan2(y1-y0, x1-x0)
    return obj

def cassette(kind):
    # All edges remain within a 4 m nominal tile. Adjacent half-beams form a
    # continuous structural rhythm, while native route collision stays intact.
    box('Continuous black roof backing', (0,0,.37), (4,4,.08), black, .009)
    for side in (-1, 1):
        box('Load-bearing ivory half beam', (side*1.94,0,.175), (.12,4,.35), stone, .006)
        box('Load-bearing cross half beam', (0,side*1.94,.175), (4,.12,.35), stone, .006)
        box('Beam inner shadow reveal', (side*1.865,0,.055), (.025,3.73,.018), dark, .001)
        box('Crossbeam inner shadow reveal', (0,side*1.865,.055), (3.73,.025,.018), dark, .001)
        box('Narrow active data conductor', (side*1.84,0,.037), (.012,3.58,.012), gold, .001)
        box('Cross data conductor', (0,side*1.84,.037), (3.58,.012,.012), gold, .001)
        for station in (-1.46,-.73,0,.73,1.46):
            box('Captive beam key', (side*1.945,station,.008), (.085,.13,.016), black, .002)
            box('Captive crossbeam key', (station,side*1.945,.008), (.13,.085,.016), black, .002)
    for x in (-1.83,1.83):
        for y in (-1.83,1.83):
            box('Four-way bearing shoe', (x,y,.025), (.25,.25,.05), black, .004)
            box('Bearing contact pin', (x,y,-.001), (.036,.036,.005), gold, .0008)

    # The two depth steps make a real dark recess. The inset stone field is
    # split so low-angle light catches joinery rather than a painted texture.
    box('Deep coffer shadow cavity', (0,0,.305), (3.58,3.58,.060), dark, .006)
    box('Inset polished black field', (0,0,.262), (3.40,3.40,.050), black, .007)
    for side in (-1,1):
        box('Stepped ivory inner sill', (side*1.755,0,.180), (.115,3.55,.15), stone, .005)
        box('Stepped cross inner sill', (0,side*1.755,.180), (3.55,.115,.15), stone, .005)
        box('Cut back sill arris', (side*1.685,0,.103), (.012,3.28,.010), gold, .001)
        box('Cut back cross arris', (0,side*1.685,.103), (3.28,.012,.010), gold, .001)
        for pos in (-1.36,1.36):
            box('Recessed edge light contact', (side*1.69,pos,.086), (.055,.21,.012), contact, .002)
            box('Recessed cross light contact', (pos,side*1.69,.086), (.21,.055,.012), contact, .002)
    for ix in (-1,1):
        for iy in (-1,1):
            x, y = ix*.825, iy*.825
            box('Replaceable black-stone cassette', (x,y,.205), (1.55,1.55,.050), black, .009)
            for ox in (-.68,.68):
                for oy in (-.68,.68):
                    box('Inset stone retention pin', (x+ox,y+oy,.176), (.048,.048,.008), gold, .001)
            # Hairline scored seams are physical grooves below the facing.
            box('Machined score in stone', (x,y+iy*.56,.177), (1.17,.012,.009), dark, .001)

    if kind == 'A':
        # A quiet asymmetric route through the panel, with one lit witness.
        segment('Axial channel well', (-1.53,-.26),(1.53,-.26),.164,.052,.016,dark)
        segment('Fine conductor', (-1.50,-.26),(1.50,-.26),.153,.014,.010,gold)
        segment('Branch well', (.48,-.26),(.97,.44),.164,.049,.016,dark)
        segment('Branch conductor', (.48,-.26),(.97,.44),.153,.012,.010,gold)
        box('Quiet service light', (-.96,-.26,.146), (.19,.042,.012), contact, .002)
    else:
        # The rare concentric register has broken, keyed arcs rather than a
        # heraldic medallion. Its inner slot shares the A tile's circuitry.
        for radius in (.65,.85):
            for i in range(28):
                if i in (0,7,14,21):
                    continue
                a = 2*math.pi*(i+.1)/28
                b = 2*math.pi*(i+.9)/28
                segment('Interrupted service register',
                        (radius*math.cos(a), radius*math.sin(a)),
                        (radius*math.cos(b), radius*math.sin(b)),
                        .160,.011,.010,gold,.0008)
        segment('Central contact well', (-.47,0),(.47,0),.148,.105,.019,dark)
        box('Central low-glare witness', (0,0,.134), (.30,.026,.009), contact, .001)
        for s in (-1,1):
            box('Register index shoe', (s*.76,0,.146), (.10,.055,.018), stone, .002)

    obj = export('SM_Aurelion_KIT_Z11ObservationCeiling_'+kind,
                 [4.0,4.0,.418])
    manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in obj.dimensions],
        collision='None: visual-only tile replacing noncolliding stock roof art',
        preserve_fallback_geometry=True, design_clearance_m=5.6, variation=kind)
    obj.location.x = -2.3 if kind == 'A' else 2.3
    return obj

for kind in ('A','B'):
    cassette(kind)

(ROOT/'manifest.json').write_text(json.dumps(dict(
    source='Editable Blender 4.5 procedural source',
    design_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z11-Observation-Ceiling.png',
    narrative_reference='Origins chapter 26; Aurelion layout page 15; August TDD section 11.3',
    layout=[6,4], room_dimensions_m=[24,16], lowest_z_m=5.6,
    placement_status='Source candidate: Unreal player-height fit required',
    modules=manifest), indent=2), encoding='utf-8')

scene.world = bpy.data.worlds.new('Z11 observation ceiling studio')
scene.world.color = (.11,.11,.12)
target = Vector((0,0,.18))
for pos,power,color in (((-6,-7,-5),2600,(1,.66,.42)),
                        ((7,-5,-4),2600,(.66,.80,1)),
                        ((0,7,-3),2100,(1,.90,.70))):
    bpy.ops.object.light_add(type='AREA', location=pos)
    light = bpy.context.object
    light.data.energy = power; light.data.size = 5; light.data.color = color
    light.rotation_euler = (target-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(6,-10,-7))
camera = bpy.context.object
camera.rotation_euler = (target-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type = 'ORTHO'; camera.data.ortho_scale = 11
scene.camera = camera
scene.render.engine = 'CYCLES'; scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.resolution_x = 1500; scene.render.resolution_y = 900
scene.render.filepath = str(ROOT/'ceiling-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z11-Observation-Ceiling.blend'))
bpy.ops.render.render(write_still=True)
print('Z11_OBSERVATION_CEILING_SOURCE_COMPLETE')
