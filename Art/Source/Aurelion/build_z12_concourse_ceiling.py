"""Measured two-variant Z12 concourse roof kit; six-metre bays, visual only.

Blender 4.5. Each bay spans 6 x 6 m. Its lowest soffit is the local origin;
placing at Z=560 cm leaves 5.6 m clear under the original Z12 room envelope.
"""
from pathlib import Path
import math

source=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0],str(source),'exec'))
ROOT=Path(__file__).resolve().parent/'Z12ConcourseCeiling'
ROOT.mkdir(exist_ok=True)

black=material('M_Aurelion_ViewBlackStone',(.026,.030,.034),.16,.38)
ivory=material('M_Aurelion_ViewIvory',(.69,.66,.59),.04,.31)
light=material('M_Aurelion_CeilingContact',(.74,.51,.21),.32,.21)
emission=light.node_tree.nodes.get('Principled BSDF')
emission.inputs['Emission Color'].default_value=(.94,.65,.30,1)
emission.inputs['Emission Strength'].default_value=1.2


def segment(name,a,b,z,width,height,mat,bevel=.003):
    x0,y0=a;x1,y1=b
    dx=x1-x0;dy=y1-y0
    obj=box(name,((x0+x1)/2,(y0+y1)/2,z),(math.hypot(dx,dy),width,height),mat,bevel)
    obj.rotation_euler.z=math.atan2(dy,dx)
    return obj


def cassette(kind):
    # The shell covers the sky. Half-width edge beams become 28 cm members
    # where adjacent 6 m modules meet, while the outer halves meet the wall.
    box('Continuous engineered roof backing',(0,0,.66),(6,6,.28),black,.012)
    for side in (-1,1):
        box('Long ivory half beam',(side*2.93,0,.245),(.14,6,.49),ivory,.007)
        box('Cross ivory half beam',(0,side*2.93,.245),(6,.14,.49),ivory,.007)
        box('Beam underside black reveal',(side*2.75,0,.215),(.085,5.62,.055),dark,.002)
        box('Crossbeam underside black reveal',(0,side*2.75,.215),(5.62,.085,.055),dark,.002)
        box('Functional long gold register',(side*2.75,0,.182),(.018,5.48,.015),gold,.001)
        box('Functional cross gold register',(0,side*2.75,.182),(5.48,.018,.015),gold,.001)
        for index in range(6):
            p=-2.43+index*.972
            box('Beam keyed clamp',(side*2.93,p,.045),(.104,.16,.056),black,.003)
            box('Crossbeam keyed clamp',(p,side*2.93,.045),(.16,.104,.056),black,.003)
    # Deep two-step coffer leaves a real cavity in its visible underside.
    box('Inset coffer shadow',(0,0,.485),(5.32,5.32,.07),dark,.004)
    box('Honed black ceiling field',(0,0,.433),(5.02,5.02,.045),black,.008)
    for side in (-1,1):
        box('Coffer ivory inner sill',(side*2.55,0,.285),(.105,5.20,.19),stone,.005)
        box('Coffer ivory inner sill',(0,side*2.55,.285),(5.20,.105,.19),stone,.005)
        box('Shadow machined inside sill',(side*2.47,0,.207),(.024,4.88,.025),dark,.001)
        box('Shadow machined inside sill',(0,side*2.47,.207),(4.88,.024,.025),dark,.001)
    for ix in (-1,1):
        for iy in (-1,1):
            # Four stone panels with real separation and fine dowel heads.
            x=ix*1.23;y=iy*1.23
            box('Replaceable basalt cassette',(x,y,.382),(2.27,2.27,.08),black,.010)
            for dx in (-.97,.97):
                for dy in (-.97,.97):
                    box('Stone retaining key',(x+dx,y+dy,.331),(.10,.10,.025),gold,.002)
            for offset in (-.91,.91):
                box('Subtle stone score',(x+offset,y,.333),(.011,1.60,.010),dark,.001)
    # Physical information route changes orientation in the alternating bay.
    if kind=='A':
        segment('Main east-west data channel',(-2.42,0),(2.42,0),.305,.061,.020,dark)
        segment('Recessed white gold line',(-2.42,0),(2.42,0),.291,.018,.012,light,.001)
        for side in (-1,1):
            segment('Branch toward rim',(side*.70,0),(side*1.30,side*.56),.305,.055,.018,dark)
            segment('Branch gold contact',(side*.70,0),(side*1.30,side*.56),.291,.016,.012,gold,.001)
    else:
        segment('Main north-south data channel',(0,-2.42),(0,2.42),.305,.061,.020,dark)
        segment('Recessed white gold line',(0,-2.42),(0,2.42),.291,.018,.012,light,.001)
        for side in (-1,1):
            segment('Branch toward rim',(0,side*.70),(side*.56,side*1.30),.305,.055,.018,dark)
            segment('Branch gold contact',(0,side*.70),(side*.56,side*1.30),.291,.016,.012,gold,.001)
    # Central contact is an inspectable mechanism, not a faction emblem.
    box('Central black contact well',(0,0,.307),(1.12,1.12,.12),dark,.011)
    for side in (-1,1):
        box('Contact ivory support',(side*.54,0,.235),(.10,1.19,.14),stone,.005)
        box('Contact ivory support',(0,side*.54,.235),(1.19,.10,.14),stone,.005)
        box('Contact gold witness',(side*.365,0,.233),(.027,.70,.016),gold,.001)
        box('Contact gold witness',(0,side*.365,.233),(.70,.027,.016),gold,.001)
    box('Luminous service contact',(0,0,.239),(.32,.32,.018),light,.004)
    for x in (-2.60,2.60):
        for y in (-2.60,2.60):
            box('Four-way beam bearing',(x,y,.09),(.33,.33,.18),black,.006)
            box('Bearing gold pin',(x,y,.004),(.048,.048,.008),gold,.001)
    obj=export('SM_Aurelion_KIT_Z12ConcourseCeiling_'+kind,[6,6,.80])
    manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in obj.dimensions],
        collision='None: roof art only, native walls and all route collision retained',
        convex_hulls=0,position_precision=10,preserve_fallback_geometry=True,
        design_clearance_m=5.6,variation=kind)
    obj.location.x=-3.4 if kind=='A' else 3.4
    return obj


for variation in ('A','B'):
    cassette(variation)
(ROOT/'manifest.json').write_text(json.dumps(dict(
    status='Source candidate: editor fit and player-eye review required',
    design_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Concourse-Ceiling.png',
    layout=[7,4],room_dimensions_m=[42,24],lowest_z_m=5.6,
    modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Aurelion ceiling studio')
scene.world.color=(.12,.12,.12)
target=Vector((0,0,.28))
for pos,power,color,size in [((-7,-8,-6),3600,(1,.68,.43),7),((7,-7,-5),4200,(.70,.85,1),7),((0,8,-4),3100,(1,.92,.76),8)]:
    bpy.ops.object.light_add(type='AREA',location=pos)
    lamp=bpy.context.object;lamp.data.energy=power;lamp.data.color=color;lamp.data.size=size
    lamp.rotation_euler=(target-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(8,-15,-11))
camera=bpy.context.object
camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=17;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'ceiling-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z12-Concourse-Ceiling.blend'))
bpy.ops.render.render(write_still=True)
print('Z12_CONCOURSE_CEILING_SOURCE_COMPLETE')
