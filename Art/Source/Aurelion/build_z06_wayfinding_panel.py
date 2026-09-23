"""Aurelion Z06 wall cassette, authored from the Z06 reference before placement.

The asset is visual-only. Its black-stone face is left blank for native mission text.
Run with Blender 4.5 in background/factory-startup mode.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Vector

base = Path(__file__).resolve().parent
exec(compile((base / 'build_architecture_kit.py').read_text().split('# Four metre bay:')[0],
             'aurelion_kit_helpers', 'exec'), globals())
ROOT = base / 'Z06WayfindingPanel'
ROOT.mkdir(exist_ok=True)
raw_box = box


def box(name, location, size, mat=stone, bevel=.004):
    return raw_box(name, location, size, mat, min(bevel, min(size)*.2))


def octagon(width, height, center_z, y, cut):
    w = width/2
    z0, z1 = center_z-height/2, center_z+height/2
    return [(-w+cut,y,z0),(-w,y,z0+cut),(-w,y,z1-cut),
            (-w+cut,y,z1),(w-cut,y,z1),(w,y,z1-cut),
            (w,y,z0+cut),(w-cut,y,z0),(-w+cut,y,z0)]


def seam(name, width, height, z, y, cut, thickness, depth, mat):
    path(name, octagon(width,height,z,y,cut),thickness,depth,mat,min(.003,thickness*.15))


# Rear plate, damped mounting cassette, stepped ivory skin and charcoal face.
# The front is local -Y; the exact exterior dimensions are measured after join.
box('Continuous rear cassette',(0,.030,.55),(4.05,.120,1.00),dark,.015)
box('Black stone information field',(0,-.049,.57),(3.76,.040,.78),dark,.006)
for width,height,y,cut,thick,depth,mat,name in (
    (4.20,1.10,-.066,.115,.105,.035,stone,'Machined ivory primary arris'),
    (4.04,.94,-.078,.095,.018,.012,gold,'Inlaid gold perimeter conductor'),
    (3.91,.82,-.084,.075,.010,.006,stone,'Cut inner optical arris'),
    (3.67,.67,-.091,.055,.006,.004,gold,'Fine inner register')):
    seam(name,width,height,.55,y,cut,thick,depth,mat)

# Two continuous upper/lower read rails and repeating scale marks. They are
# physically recessed, never emissive; the face stays quiet during combat.
for z in (.198,.906):
    box('Inset horizontal shadow kerf',(0,-.089,z),(3.46,.008,.018),dark,.001)
    box('Gold read rail',(0,-.095,z),(3.34,.006,.005),gold,.0008)
for side in (-1,1):
    x=side*1.98
    box('Machined vertical cheek',(x,-.092,.55),(.08,.018,.74),stone,.004)
    box('Shoulder black separator',(side*1.84,-.084,.55),(.014,.010,.65),dark,.001)
    for z in (.32,.55,.78):
        box('Cheek socket',(x,-.103,z),(.025,.004,.025),dark,.002)
        box('Retaining pin',(x,-.106,z),(.009,.003,.009),gold,.0008)
    for i in range(7):
        z=.325+i*.075
        box('Height registration nick',(side*1.737,-.094,z),(.032,.004,.004),gold,.0007)

# Raised identifier plinth and restrained route chevron occupy separate bays.
# Native TextRender can be placed in the broad central blank field in Unreal.
box('Route identifier shadow bed',(-1.60,-.099,.55),(.16,.009,.23),dark,.003)
box('Route identifier ivory chip',(-1.60,-.106,.55),(.115,.008,.185),stone,.003)
for z in (.505,.55,.595):
    box('Identifier three-stroke index',(-1.60,-.112,z),(.054,.004,.007),gold,.0007)
chevron=[(1.49,-.104,.41),(1.66,-.104,.55),(1.49,-.104,.69)]
path('Wayfinding shadow chevron',chevron,.085,.010,dark,.003)
path('Wayfinding inset gold chevron',[(x,y-.009,z) for x,y,z in chevron],.026,.005,gold,.002)

# Rear stone bedding and visible mechanical standoffs survive oblique views.
for x in (-1.55,0,1.55):
    box('Rear replaceable cassette',(x,.101,.55),(1.13,.020,.70),stone,.009)
    for dx in (-.50,.50):
        for z in (.26,.84):
            box('Rear countersunk socket',(x+dx,.113,z),(.032,.004,.032),dark,.002)
            box('Rear pin head',(x+dx,.116,z),(.012,.003,.012),gold,.001)
    for i in range(5):
        box('Rear heat relief incision',(x,.116,.42+i*.065),(.54,.004,.008),dark,.0008)
for x in (-1.73,1.73):
    for z in (.28,.82):
        box('Isolated rear wall shoe',(x,.118,z),(.17,.044,.15),dark,.008)
        box('Bearing stud',(x,.145,z),(.07,.012,.07),gold,.004)

name='SM_Aurelion_KIT_Z06WayfindingPanel'
points=[o.matrix_world@Vector(c) for o in parts for c in o.bound_box]
dims=[max(p[i] for p in points)-min(p[i] for p in points) for i in range(3)]
obj=export(name,dims)
manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,
                    collision='None; wall visual only',
                    native_text='Existing TextRenderActor_113 should be resized and mounted over the blank field',
                    design_reference='Art/References/Aurelion/Z06Wayfinding/Z06-Wayfinding-Panel-Concept.png')
(ROOT/'manifest.json').write_text(json.dumps({'status':'source authored; Unreal placement acceptance pending',
                                              'modules':manifest},indent=2))

scene.world=bpy.data.worlds.new('Aurelion sign studio')
scene.world.color=(.09,.09,.09)
for location,power,size in (((-2,-4,4),1200,3),((2,-3,2),650,2),((0,2,3),800,3)):
    bpy.ops.object.light_add(type='AREA',location=location)
    lamp=bpy.context.object
    lamp.data.energy=power
    lamp.data.size=size
    lamp.rotation_euler=(Vector((0,0,.55))-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2.2,-7,2.3))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,.55))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO'
camera.data.ortho_scale=5.1
scene.camera=camera
scene.render.engine='CYCLES'
scene.cycles.samples=40
scene.cycles.use_denoising=True
scene.render.resolution_x=1600
scene.render.resolution_y=900
scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'Z06-Wayfinding-Panel-Studio.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z06-Wayfinding.blend'))
bpy.ops.render.render(write_still=True)
