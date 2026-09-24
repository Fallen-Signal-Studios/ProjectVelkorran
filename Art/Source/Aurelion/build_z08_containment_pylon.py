"""Author a separate clean Z08 containment pylon and removable Eclipse growth.

The accepted Z08 reference is a composition guide, not a dimensional model.
Units are metres, front faces -Y, pivot is at the floor centre. This is a
noninteractive wall-side module; it does not replace the Crucible's routes.
"""
from pathlib import Path
from mathutils import noise

helper = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0], str(helper), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z08ContainmentPylon'
ROOT.mkdir(exist_ok=True)

blackstone = material('M_Aurelion_PylonBlackStone', (.027, .029, .033), .12, .34)
infected = material('M_Eclipse_PylonTissue', (.004, .005, .008), .10, .24)
scar = material('M_Eclipse_PylonScar', (.036, .014, .049), .14, .29)
light = material('M_Aurelion_PylonWarmConduit', (.78, .48, .15), .2, .22)
violet = material('M_Eclipse_PylonInternalPulse', (.16, .035, .25), .08, .19)
for mat, color, power in ((light, (.95, .57, .19, 1), 1.8),
                          (violet, (.24, .025, .45, 1), 2.8)):
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Emission Color'].default_value = color
    shader.inputs['Emission Strength'].default_value = power


def bolt(name, x, y, z, radius=.018, mat=gold):
    bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=radius, depth=.009,
                                        location=(x, y, z), rotation=(math.pi/2, 0, 0))
    finish(bpy.context.object, mat, .001)
    bpy.context.object.name = name


def front_slot(name, x, z, h, outer=.070, inner=.023):
    box(name+' shadow', (x, -.925, z), (outer, .024, h), dark, .004)
    box(name+' conductor', (x, -.943, z), (inner, .014, h-.08), gold, .002)
    box(name+' illuminated seam', (x, -.955, z), (.006, .006, h-.18), light, .001)


# Grounded black plinth, stepped stone isolation base, and continuous rear body.
for z, width, depth, height, mat in (
        (.11, 3.94, 2.04, .22, blackstone),
        (.27, 3.82, 1.93, .12, stone),
        (.40, 3.63, 1.82, .15, gold),
        (.54, 3.52, 1.75, .18, blackstone)):
    box('Foundation course', (0, 0, z), (width, depth, height), mat, .014)
box('Structural rear monolith', (0, .20, 4.02), (3.22, 1.20, 6.88), blackstone, .025)
box('Inset ivory instrument well', (0, -.51, 4.07), (2.86, .21, 6.45), stone, .018)
box('Deep central service recess', (.31, -.657, 4.02), (1.43, .048, 4.94), dark, .009)
box('Honed central instrument plate', (.31, -.700, 4.02), (1.29, .057, 4.80), stone, .014)

# Split masonry courses keep the vertical silhouette architectural rather than
# a single stretched box. The left edges deliberately remain clean beneath a
# separately removable, localized organic overlay.
for side in (-1, 1):
    x = side*1.585
    box('Outer engaged pier core', (x, -.18, 4.10), (.45, 1.40, 6.85), stone, .016)
    box('Pier black reveal', (side*1.275, -.83, 4.05), (.066, .022, 6.44), dark, .003)
    box('Pier gold arris', (side*1.285, -.853, 4.05), (.017, .012, 6.32), gold, .002)
    for row in range(7):
        z = 1.13 + row*.90
        box('Dressed pier course', (x, -.84, z), (.42, .13, .86), stone, .010)
        box('Ashlar course seam', (x, -.913, z+.427), (.39, .009, .018), dark, .002)
        bolt('Course retaining pin', x, -.918, z-.34, .012)
    for z in (.82, 7.24):
        box('Pier collar', (x, -.25, z), (.60, 1.58, .16), stone, .012)
        box('Collar conductor', (x, -.997, z), (.51, .017, .025), gold, .002)

# Recessed serial courses, precision interface and the large gold annulus.
for row in range(11):
    z = 1.02 + row*.56
    box('Instrument plate register', (.31, -.738, z), (1.12, .013, .012),
        dark if row%3 else gold, .001)
for row in range(7):
    z = 1.51+row*.70
    # Shallow nested stone cassettes give the broad clean face an authored
    # course rhythm visible at player height, not only thin painted lines.
    for x, width in ((-.43,.35),(.31,.64),(.95,.25)):
        if x == .31 and 2.55 < z < 5.25:
            continue  # preserve readable annular instrument silhouette
        box('Inset instrument cassette shadow',(x,-.756,z),(width,.022,.61),dark,.005)
        box('Honed raised cassette',(x,-.775,z),(width-.045,.023,.56),stone,.009)
        for end in (-1,1):
            bolt('Cassette gold keeper',x+end*(width*.40),-.790,z-.22,.009)
for z, width in ((1.06, .94), (6.95, .94)):
    box('Instrument heel or shoulder', (.31, -.80, z), (width, .18, .22), blackstone, .012)
for x in (-.28, .90):
    front_slot('Parallel power rail', x, 4.02, 4.83)
for radius, thickness, material, y in ((.91, .16, blackstone, -.97),
                                        (.82, .055, gold, -1.006),
                                        (.67, .030, light, -1.044)):
    ring('Concentric instrument bezel', .31, y, 4.16, radius, thickness, material)
bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=.43, depth=.14,
                                    location=(.31, -1.035, 4.16),
                                    rotation=(math.pi/2, 0, 0))
finish(bpy.context.object, blackstone, .018)
ring('Core inner clockwork register', .31, -1.124, 4.16, .34, .030, gold)
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=1,
                                     location=(.31, -1.15, 4.16))
core = bpy.context.object
core.scale = (.20, .045, .20)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
finish(core, light, .002)
for i in range(16):
    a = i*math.tau/16
    x, z = .31+math.sin(a)*.96, 4.16+math.cos(a)*.96
    bolt('Bezel radial fastener', x, -1.012, z, .018)

# The instrument grows out of a traced mechanism rather than sitting like a
# decorative ring on a flat wall. Dark carved beds and separate fine gold
# conductors fork around its circumference and reconnect at the crown.
for side in (-1, 1):
    route = [(.31+side*.33,-.846,.82),
             (.31+side*.42,-.846,2.40),
             (.31+side*1.13,-.846,3.28),
             (.31+side*1.13,-.846,5.04),
             (.31+side*.42,-.846,5.92),
             (.31+side*.33,-.846,6.76)]
    path('Deep instrument conductor bed',route,.115,.035,dark,.004)
    path('Inset gold conductor',[(x,y-.039,z) for x,y,z in route],.026,.016,gold,.002)
    path('Warm active conductor',[(x,y-.056,z) for x,y,z in route],.006,.009,light,.001)
    # Secondary return through the stone flank follows the same engineering
    # language without mirroring every small mark perfectly.
    flank=[(.31+side*1.18,-.853,1.06),
           (.31+side*1.18,-.853,2.70),
           (.31+side*1.43,-.853,3.12),
           (.31+side*1.43,-.853,5.18),
           (.31+side*1.18,-.853,5.62),
           (.31+side*1.18,-.853,6.86)]
    path('Outer isolated circuit',flank,.055,.028,dark,.003)
    path('Outer circuit gold line',[(x,y-.032,z) for x,y,z in flank],.011,.011,gold,.001)
for i in range(24):
    angle=i*math.tau/24
    first=(.31+math.sin(angle)*.998,-1.018,4.16+math.cos(angle)*.998)
    second=(.31+math.sin(angle)*1.095,-1.018,4.16+math.cos(angle)*1.095)
    path('Annulus engraved radial index',[first,second],.018,.008,gold,.001)

# Crown is layered, with a restrained central beacon. The outer shoulders are
# asymmetrical only in their surface damage, not their structural support.
for z, width, depth, height, mat in (
        (7.44, 3.76, 1.86, .16, blackstone),
        (7.57, 3.88, 1.93, .14, stone),
        (7.72, 3.62, 1.82, .16, gold),
        (7.85, 3.36, 1.72, .15, stone)):
    box('Machined pylon crown', (0, -.08, z), (width, depth, height), mat, .015)
box('Beacon channel shadow', (.31, -1.000, 7.18), (.28, .020, 1.05), dark, .004)
box('Beacon light', (.31, -1.020, 7.18), (.078, .016, .91), light, .003)
for x in (-1.37, -1.10, 1.10, 1.37):
    box('Crown separation kerf', (x, -1.013, 7.69), (.013, .008, .14), dark, .001)

clean = export('SM_Aurelion_KIT_Z08ContainmentPylon', [3.94, 2.215, 7.925])
# Explicit simple collision for an eventual wall-adjacent placement. It must
# still be visually fitted and checked against the real route before use.
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -.0175, 3.9625))
hull = bpy.context.object
hull.name = 'UCX_SM_Aurelion_KIT_Z08ContainmentPylon_00'
hull.dimensions = (3.94, 2.215, 7.925)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
hull.hide_render = True
hull.display_type = 'WIRE'
bpy.ops.object.select_all(action='DESELECT')
clean.select_set(True)
hull.select_set(True)
bpy.context.view_layer.objects.active = clean
bpy.ops.export_scene.fbx(filepath=str(ROOT/(clean.name+'.fbx')),
                         use_selection=True, object_types={'MESH'},
                         axis_forward='-Y', axis_up='Z', apply_unit_scale=True,
                         bake_anim=False, add_leaf_bones=False,
                         mesh_smooth_type='FACE')
manifest[-1].update(convex_hulls=1, collision='One fitted solid UCX envelope')


def growth(name, coordinates, thickness, mat=infected):
    """Tapered physical strand; no screen-space decal or floating fog."""
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = 12
    curve.bevel_depth = thickness
    curve.bevel_resolution = 3
    spline = curve.splines.new('POLY')
    spline.points.add(len(coordinates)-1)
    for index, (x, y, z) in enumerate(coordinates):
        spline.points[index].co = (x, y, z, 1)
        spline.points[index].radius = max(.16, 1-index/(len(coordinates)-.5))
    obj = bpy.data.objects.new(name, curve)
    scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.convert(target='MESH')
    return finish(bpy.context.object, mat, 0)


def nodule(name, x, y, z, size, mat):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=1,
                                          location=(x, y, z))
    obj = bpy.context.object
    obj.name = name
    obj.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    # Distort the tissue at source-mesh resolution so the infection reads as
    # bonded, asymmetrical material rather than faceted fantasy gemstones.
    for vertex in obj.data.vertices:
        p = vertex.co.copy()
        displacement = 1+.18*noise.noise(p*9+Vector((x*3,y*3,z*3)))
        vertex.co *= displacement
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    finish(obj, mat, 0)


# A wound climbing only the left third of the device. Clean annulus and right
# pier remain readable, so the infection can be removed without a hole in the
# production architecture or a forced rewrite of gameplay navigation.
for x, y, z, size in (
        (-1.61, -1.00, 1.00, (.27, .15, .57)),
        (-1.50, -1.01, 2.12, (.31, .18, .72)),
        (-1.58, -.99, 3.30, (.30, .16, .76)),
        (-1.68, -.97, 4.50, (.26, .15, .63)),
        (-1.53, -.97, 5.55, (.31, .15, .80)),
        (-1.35, -.95, 6.52, (.28, .15, .56))):
    nodule('Eclipse bonded tissue lobe', x, y, z, size, infected)
for index in range(7):
    z = 1.2+index*.82
    nodule('Fractured ceramic edge', -1.24-.08*(index%2), -.974, z,
           (.13, .075, .17+.03*(index%3)), blackstone)
for i, route in enumerate((
        [(-1.65,-1.16,.49),(-1.48,-1.15,1.6),(-1.65,-1.17,3.0),(-1.44,-1.15,4.35),(-1.62,-1.18,5.8),(-1.22,-1.12,7.4)],
        [(-1.52,-1.18,1.9),(-1.86,-1.14,2.65),(-1.98,-1.05,3.35),(-1.78,-1.08,4.15)],
        [(-1.53,-1.17,3.1),(-1.16,-1.19,3.54),(-.90,-1.15,4.19),(-.79,-1.11,4.61)],
        [(-1.45,-1.18,4.85),(-1.90,-1.13,5.15),(-2.13,-1.04,5.67),(-2.04,-.99,6.18)],
        [(-1.37,-1.13,6.25),(-1.08,-1.09,6.67),(-.70,-1.04,7.02)],
        [(-1.55,-1.11,2.24),(-1.04,-1.16,2.05),(-.60,-1.09,1.62)])):
    growth('Eclipse tensile root '+str(i), route, .082 if i==0 else .044)
    if i in (0, 2, 4):
        offset = [(x+.022, y-.024, z) for x,y,z in route[1:-1]]
        growth('Subdermal violet fissure '+str(i), offset, .010, scar)
for x, y, z, radius in ((-1.58,-1.22,2.14,.055),(-1.32,-1.23,3.98,.047),
                         (-1.75,-1.18,5.61,.063),(-1.15,-1.16,6.47,.039)):
    nodule('Deep violet activity node', x, y, z,
           (radius,radius*.62,radius*1.25), violet)

# Hairline attachment roots bind the broad tissue masses to their host stone.
# They vary in direction and length, so the silhouette reads as a living breach
# without spreading the infection across the clean instrument or right pier.
for index in range(12):
    z=1.02+index*.51
    outward=index%3==0
    root=(-1.54,-1.21,z)
    mid=(-1.68 if outward else -1.25,-1.185,z+(.16 if index%2 else -.13))
    tip=(-1.92 if outward else -.94,-1.075,z+(.39 if index%2 else -.31))
    growth('Eclipse bonded branch '+str(index),[root,mid,tip],.022 if outward else .017)
    if index in (2,5,8,11):
        growth('Eclipse branch subdermal vein '+str(index),
               [(x+.015,y-.016,z) for x,y,z in (root,mid,tip)],.004,scar)

overlay = export('SM_Eclipse_KIT_Z08PylonGrowth', [1.5546, .46271, 7.00023])
manifest[-1]['collision'] = 'None; visual overlay bonded to structural pylon'

(ROOT/'manifest.json').write_text(json.dumps(dict(
    status='Source candidate; exported mesh, lighting and live route acceptance pending',
    reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z08-Breached-Containment-Pylon-Higgsfield-Reference.png',
    guide_dimensions_m=[3.94,2.04,8.0], modules=manifest), indent=2))

# Small neutral studio scene; objects remain individually editable in the .blend.
scene.world = bpy.data.worlds.new('Z08 pylon review world')
scene.world.color = (.12, .12, .12)
for position, energy, size in (((-5,-6,9),2100,5),((5,-3,6),1500,4),
                               ((0,4,7),2300,5)):
    bpy.ops.object.light_add(type='AREA', location=position)
    lamp = bpy.context.object
    lamp.data.energy = energy
    lamp.data.size = size
    lamp.rotation_euler = (Vector((0,0,4))-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(8,-17,8.0))
camera = bpy.context.object
camera.rotation_euler = (Vector((0,0,4.1))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 9.6
scene.camera = camera
scene.render.engine = 'CYCLES'
scene.cycles.samples = 48
scene.cycles.use_denoising = True
scene.render.resolution_x = 1050
scene.render.resolution_y = 1400
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = str(ROOT/'pylon-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z08-Containment-Pylon.blend'))
bpy.ops.render.render(write_still=True)
print('Z08_CONTAINMENT_PYLON_SOURCE_PASS')
