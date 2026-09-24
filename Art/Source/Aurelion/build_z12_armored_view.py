"""Author the paired-bay Z12 armored view insert as separate frame and glazing.

Metres; bottom-centre pivot; local X is the 8 m wall run. This remains an art
candidate until actual M13 wall, retained collision and player-eye tests pass.
"""
from pathlib import Path

builder=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(builder.read_text().split('# Four metre bay:')[0],str(builder),'exec'))
ROOT=Path(__file__).resolve().parent/'Z12ArmoredView'
ROOT.mkdir(exist_ok=True)

black=material('M_Aurelion_ViewBlackStone',(.022,.027,.032),.18,.30)
light=material('M_Aurelion_ViewWarmConduit',(.76,.48,.19),.25,.20)
glass=material('M_Aurelion_ViewGlass',(.31,.39,.42),.06,.08)
light.node_tree.nodes.get('Principled BSDF').inputs['Emission Color'].default_value=(.95,.68,.32,1)
light.node_tree.nodes.get('Principled BSDF').inputs['Emission Strength'].default_value=1.5
gp=glass.node_tree.nodes.get('Principled BSDF')
gp.inputs['Transmission Weight'].default_value=.92
gp.inputs['Alpha'].default_value=.24
glass.surface_render_method='DITHERED'

# Stable, load-bearing perimeter; the clear aperture is 6.4 x 4.6 m. Stone
# cassettes and conductor lines are independent pieces, not a planar image.
for x in (-3.70,3.70):
    box('Deep black pier core',(x,0,3.0),(.60,.50,6.0),black,.012)
    for z,h,w in ((.28,.56,.60),(1.04,.74,.57),(2.07,.72,.57),
                  (3.10,.72,.57),(4.13,.72,.57),(5.18,.72,.57),(5.77,.46,.60)):
        box('Jointed ivory pier course',(x,0,z),(w,.62,h),stone,.015)
    for face in (-1,1):
        for offset in (-.175,.175):
            box('Pier dark machined reveal',(x+offset,face*.325,3.0),(.048,.018,5.64),dark,.003)
            box('Pier conductor',(x+offset,face*.342,3.0),(.013,.012,5.47),gold,.002)
        for z in (.44,1.54,2.64,3.74,4.84,5.63):
            box('Stone locking collar',(x,face*.324,z),(.60,.036,.075),black,.005)
            box('Collar gold witness',(x,face*.347,z),(.52,.010,.011),gold,.001)

for z,h,mat in ((.16,.32,black),(.50,.34,stone),(5.58,.36,stone),(5.87,.26,black)):
    box('Continuous horizontal bearing',(0,0,z),(8,.60,h),mat,.012)
for face in (-1,1):
    y=face*.315
    for z,w in ((.19,.09),(.72,.045),(5.41,.045),(5.84,.09)):
        box('Sill or lintel line',(0,y,z),(7.97,.026,w),dark,.004)
        box('Inset metallic register',(0,y+face*.019,z),(7.71,.010,.013),gold,.001)
    # Black substrate and precisely segmented ivory cells across the lower
    # breastwork keep the safety barrier legible from either side.
    box('Lower parapet shadow',(0,y,.43),(6.41,.038,.61),black,.004)
    for index in range(7):
        x=(index-3)*.90
        box('Replaceable basalt cassette',(x,y+face*.025,.44),(.83,.026,.46),black,.008)
        box('Cassette top lock',(x,y+face*.044,.685),(.78,.012,.019),gold,.002)
        for end in (-1,1):
            box('Cassette retaining pin',(x+end*.36,y+face*.045,.44),(.025,.012,.025),gold,.003)
    # Two-level ceiling coffer seen from player height. The emitter is inset
    # behind a narrow gold flange, not exposed as a broad light strip.
    for index in range(8):
        x=(index-3.5)*.88
        box('Soffit dark coffer',(x,y,5.69),(.82,.040,.27),dark,.007)
        box('Honed coffer face',(x,y+face*.023,5.70),(.76,.022,.19),stone,.006)
        box('Warm functional seam',(x,y+face*.041,5.57),(.46,.011,.009),light,.001)
    for x in (-3.21,3.21):
        box('Inner glazing seal',(x,y,3.10),(.055,.028,4.60),black,.004)
        box('Retained gold contact',(x,y+face*.020,3.10),(.010,.010,4.37),gold,.001)
    # Angled mechanical corner knees stiffen the frame yet leave a wide clear
    # centre. Repeat on the scenic side, with no handed faction ornament.
    for side in (-1,1):
        for high in (False,True):
            z0=5.38 if high else .78
            z1=4.99 if high else 1.17
            points=[(side*3.17,y,z0),(side*2.80,y,z1)]
            path('Oblique black bearing',points,.12,.038,black,.004)
            path('Oblique gold contact',[(x,yy+face*.041,z) for x,yy,z in points],.021,.011,gold,.001)

frame=export('SM_Aurelion_KIT_Z12ArmoredViewFrame_8x6',[8,.73,6])
manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in frame.dimensions],
                    convex_hulls=0,collision='None; retained native wall is the physical barrier',
                    position_precision=10,preserve_fallback_geometry=True)

# The two panes leave a 1.12 m gap around the retained native central rib.
# Their translucency is authored in Unreal; the native wall still blocks
# movement and attacks across the full side boundary.
for side in (-1,1):
    box('Armored transparent panel',(side*1.91,0,3.08),(2.56,.035,4.60),glass,.003)
pane=export('SM_Aurelion_KIT_Z12ArmoredViewPane_8x6',[6.38,.035,4.60])
manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in pane.dimensions],
                    convex_hulls=0,collision='None; visual glass over retained wall blocker',
                    nanite_enabled=False)

# The old visible vendor column is replaced by this two-sided custom mullion.
# It wraps, rather than replaces, the hidden Z12_Rib_1 native collider at each
# side wall. Local +Y is the room-facing direction when the paired views turn.
box('Deep mullion core',(0,.64,3),(.98,1.30,6),black,.014)
for face,y in (('Scenic',-.05),('Room',1.34)):
    box(face+' broad black bed',(0,y,3),(.99,.085,5.87),dark,.010)
    for row in range(6):
        z=.49+row*.98
        box(face+' dressed ivory cassette',(0,y+(.055 if y>0 else -.055),z),
            (.82,.068,.89),stone,.012)
        box(face+' inset longitudinal rail',(0,y+(.094 if y>0 else -.094),z),
            (.14,.012,.74),black,.004)
        box(face+' fine gold spine',(0,y+(.105 if y>0 else -.105),z),
            (.018,.010,.66),gold,.001)
    for z in (.10,1.97,3.93,5.88):
        box(face+' load collar',(0,y,z),(1.12,.10,.105),black,.008)
        box(face+' collar trace',(0,y+(.060 if y>0 else -.060),z),
            (1.00,.013,.015),gold,.001)
for z,h in ((.15,.30),(5.83,.34)):
    box('Full-depth mullion end shoe',(0,.64,z),(1.20,1.43,h),stone,.015)
mullion=export('SM_Aurelion_KIT_Z12ArmoredViewMullion_6m',[1.2,1.55,6])
manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in mullion.dimensions],
                    convex_hulls=0,collision='None; hidden native Z12_Rib_1 retains collision',
                    position_precision=10,preserve_fallback_geometry=True)

(ROOT/'manifest.json').write_text(json.dumps(dict(
    status='Source candidate; M13 side-wall fit and material review pending',
    references=['Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Departure-Concourse.png',
                'Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Armored-View-Module.png'],
    dimensions_m=[8,.73,6],clear_aperture_m=[6.4,4.6],
    pane_layout='Two transparent panels split by an authored 1.2 m mullion',
    modules=manifest),indent=2))

# Neutral studio source preview; scene-only lights and backdrop never export.
scene.world=bpy.data.worlds.new('Z12 armored view studio')
scene.world.color=(.09,.10,.12)
for position,energy,size in (((-7,-5,8),2600,6),((7,-3,6),1700,5),((0,5,7),1800,5)):
    bpy.ops.object.light_add(type='AREA',location=position)
    lamp=bpy.context.object;lamp.data.energy=energy;lamp.data.size=size
    lamp.rotation_euler=(Vector((0,0,3))-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(11,-17,7))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,3.0))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=11.5;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=1200
scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'armored-view-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z12-Armored-View.blend'))
bpy.ops.render.render(write_still=True)
print('Z12_ARMORED_VIEW_SOURCE_PASS')
