"""Aurelion Z12 axial oculus, fitted to one measured 6 x 6 m roof bay.

The underside is the visible face. This is visual-only ceiling cladding;
existing room and route collision remain independent.
"""
from pathlib import Path
import json
import math
from mathutils import Vector

source = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0], str(source), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z12ConcourseOculus'
ROOT.mkdir(exist_ok=True)

black = material('M_Aurelion_BlackStone', (.022, .027, .034), .16, .38)
lens = material('M_Aurelion_LumenLens', (.9, .67, .27), .25, .15)
p = lens.node_tree.nodes.get('Principled BSDF')
p.inputs['Emission Color'].default_value = (.99, .78, .40, 1)
p.inputs['Emission Strength'].default_value = 1.6

def bar(name, start, end, width, height, mat, bevel=.005):
    a, b = Vector(start), Vector(end)
    midpoint = (a + b) * .5
    obj = box(name, midpoint, ((b-a).length, width, height), mat, bevel)
    obj.rotation_euler = (b-a).to_track_quat('X', 'Z').to_euler()
    return obj

def torus(name, radius, minor, z, mat, segments=96):
    bpy.ops.mesh.primitive_torus_add(major_segments=segments, minor_segments=8,
        location=(0, 0, z), major_radius=radius, minor_radius=minor)
    obj = bpy.context.object
    obj.name = name
    return finish(obj, mat, 0)

def disk(name, radius, depth, z, mat, vertices=64):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
        depth=depth, location=(0, 0, z))
    obj = bpy.context.object
    obj.name = name
    return finish(obj, mat, min(.003, depth*.24))

# A noncolliding 80 cm roof tile. Its enclosed back keeps the star field from
# leaking through even if this replaces the original visual-only tile.
box('Continuous black-stone roof',(0,0,.63),(5.98,5.98,.34),black,.014)
box('Recessed warm-metal plenum',(0,0,.425),(5.61,5.61,.055),dark,.006)

# Deeply stepped perimeter and inset signal channels. Each return is modelled
# as geometry so that the profile reads at oblique eye-level view angles.
for psize, depth, width, mat in ((5.75,.48,.24,stone),
                                 (5.57,.386,.07,dark),
                                 (5.46,.365,.027,gold),
                                 (5.16,.348,.09,stone),
                                 (5.02,.322,.024,gold)):
    half = psize*.5
    for sign in (-1,1):
        box('Stepped east-west frame',(0,sign*half,depth),
            (psize,width,.105 if mat==stone else .025),mat,.006)
        box('Stepped north-south frame',(sign*half,0,depth),
            (width,psize,.105 if mat==stone else .025),mat,.006)

# Four clipped-corner cassette leaves. Actual bevelled edge geometry, dark
# stone, indexing tracks and captive fasteners replace the old flat lamp.
for sx in (-1,1):
    for sy in (-1,1):
        x,y=sx*1.31,sy*1.31
        box('Black-stone removable cassette',(x,y,.368),(2.42,2.42,.13),black,.02)
        box('Inset machined cassette face',(x,y,.293),(2.11,2.11,.025),dark,.003)
        for offset in (-.86,.86):
            box('Gold cassette rail X',(x,y+offset,.278),(1.54,.018,.013),gold,.003)
            box('Gold cassette rail Y',(x+offset,y,.278),(.018,1.54,.013),gold,.003)
        for dx in (-.94,.94):
            for dy in (-.94,.94):
                disk('Captive service rivet',.032,.012,.276,gold,16).location.x=x+dx
                parts[-1].location.y=y+dy

# Eight tapered spokes and nested orbit rings form the rare sovereign device.
# Their underside is lower than the cassettes, giving a real layered relief.
for index in range(8):
    angle=index*math.tau/8
    direction=Vector((math.cos(angle),math.sin(angle),0))
    a=direction*.48; b=direction*2.39
    bar('Sovereign radial bearer',(a.x,a.y,.218),(b.x,b.y,.218),.095,.13,stone,.009)
    bar('Bearer shadow cut',(a.x,a.y,.142),(b.x,b.y,.142),.050,.016,dark,.003)
    bar('Axial gold circuit',(a.x,a.y,.130),(b.x,b.y,.130),.021,.010,gold,.002)
    for radius in (1.29,1.99):
        point=direction*radius
        obj=box('Circuit keyed node',(point.x,point.y,.115),(.088,.088,.025),gold,.004)
        obj.rotation_euler.z=angle

for radius,minor,z,mat in ((.93,.058,.214,stone),(.78,.018,.164,gold),
                            (.61,.055,.178,black),(.52,.022,.119,gold),
                            (.34,.039,.106,stone),(.245,.022,.045,gold)):
    torus('Concentric machined oculus',radius,minor,z,mat)
disk('Recessed lens well',.245,.055,.107,dark)
disk('Functional oculus lens',.185,.012,.068,lens)
for i in range(16):
    a=i*math.tau/16
    r=1.07
    obj=box('Sixteen radial index ticks',(r*math.cos(a),r*math.sin(a),.145),
            (.075,.018,.018),gold,.002)
    obj.rotation_euler.z=a

module=export('SM_Aurelion_KIT_Z12SovereignOculus_6m',[5.99,5.99,.777])
manifest[-1].update(collision='None: visual ceiling only',convex_hulls=0,
    intended_world_base_z_cm=560,proposed_bay_centres_cm=[[-600,47200],[600,47800]])
(ROOT/'manifest.json').write_text(json.dumps(dict(
    status='Editable source, FBX and in-engine preview candidate',
    design_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Concourse-Ceiling.png',
    measured_room='42 x 24 m M13 Z12; 7 x 4 noncolliding roof bays at Z 560..640 cm',
    module=manifest[-1]),indent=2))

scene.world=bpy.data.worlds.new('Oculus studio')
scene.world.color=(.08,.09,.11)
for pos,power,color,size in [((-8,-6,2),3500,(1,.78,.55),7),
                              ((6,-5,3),3400,(.75,.89,1),7),
                              ((1,7,4),5000,(1,.95,.84),9)]:
    bpy.ops.object.light_add(type='AREA',location=pos)
    lamp=bpy.context.object;lamp.data.energy=power;lamp.data.color=color;lamp.data.size=size
    lamp.rotation_euler=(Vector((0,0,.3))-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(7,-9,-8))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,.3))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=8.5;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1500;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'oculus-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z12-Sovereign-Oculus.blend'))
bpy.ops.render.render(write_still=True)
print('Z12_SOVEREIGN_OCULUS_SOURCE_COMPLETE')
