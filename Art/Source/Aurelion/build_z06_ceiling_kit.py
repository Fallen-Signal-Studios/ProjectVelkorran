"""Breach-rescue ceiling: deep octagonal stone coffers, unit-scale fitted edge bays."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z06CeilingKit';ROOT.mkdir(exist_ok=True)

def wedge(name,rx,ry,ix,iy,a,b,z,h,mat):
    xy=[(rx*math.cos(a),ry*math.sin(a)),(rx*math.cos(b),ry*math.sin(b)),(ix*math.cos(b),iy*math.sin(b)),(ix*math.cos(a),iy*math.sin(a))]
    verts=[(x,y,zz) for zz in (z,z+h) for x,y in xy]
    faces=[(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
    o=bpy.data.objects.new(name,m);scene.collection.objects.link(o);return finish(o,mat,.008)

def coffer(width,name):
    # The full backing closes every bay; articulated stone sits below it.
    box('Continuous stone roof backing',(0,0,.52),(width,4,.06),stone,.006)
    for x in (-width/2+.09,width/2-.09):
        for y in (-1,1):box('Jointed primary ceiling beam',(x,y,.11),(.18,1.98,.22),stone,.014)
        box('Beam recessed fascia',(x,0,.028),(.09,3.96,.048),dark,.005)
    for y in (-1.91,1.91):
        for x in (-width/4,width/4):box('Jointed transverse beam',(x,y,.11),(width/2-.012,.18,.22),stone,.014)
    rx=width/2-.22;ry=1.78
    for j in range(8):
        a=math.pi/8+j*math.pi/4+.005;b=math.pi/8+(j+1)*math.pi/4-.005
        wedge('Octagonal dressed voussoir',rx,ry,rx-.23,ry-.23,a,b,.12,.18,stone)
        wedge('Recessed coffer moulding',rx-.225,ry-.225,rx-.35,ry-.35,a,b,.255,.115,stone)
        wedge('Coffer shadow bed',rx-.345,ry-.345,rx-.405,ry-.405,a,b,.352,.055,dark)
        wedge('Recessed conductor',rx-.36,ry-.36,rx-.392,ry-.392,a,b,.340,.03,gold)
        wedge('Inner stone bevel course',rx-.40,ry-.40,rx-.51,ry-.51,a,b,.396,.096,stone)
    # Corner corbels bridge the square structural grid and octagonal coffers.
    for sx in (-1,1):
        for sy in (-1,1):
            x=sx*(width/2-.34);y=sy*1.66
            box('Corner bearing block',(x,y,.25),(.43,.43,.26),stone,.025)
            box('Bearing inset',(x,y,.105),(.25,.25,.035),dark,.005)
            box('Bearing stone boss',(x,y,.075),(.16,.16,.04),stone,.013)
    # Shallow, visible central carved register; broad stone remains nonmetallic.
    for sx in (-1,1):
        x=sx*.30
        box('Carved central reveal',(x,0,.476),(.038,1.16,.024),dark,.004)
        for y in (-.52,.52):box('Register stone terminal',(x,y,.444),(.15,.15,.056),stone,.012)
    o=export(name,[width,4,.55]);manifest[-1]['nominal_dimensions_m']=list(o.dimensions)
    manifest[-1]['collision']='None; ceiling relief stays above 6.45 m room clearance; no gameplay collision changed'
    manifest[-1]['minimum_z_m']=min(v.co.z for v in o.data.vertices)
    manifest[-1]['maximum_z_m']=max(v.co.z for v in o.data.vertices)
    o.hide_render=True
    return o

wide=coffer(4,'SM_Aurelion_KIT_Z06Coffer_4m')
edge=coffer(3,'SM_Aurelion_KIT_Z06Coffer_3x4')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored coffer candidate; in-engine lighting and gameplay review pending',modules=manifest),indent=2))
wide.hide_render=False
scene.render.engine='CYCLES';scene.cycles.samples=48
scene.world=bpy.data.worlds.new('Coffer studio world');scene.world.color=(.2,.2,.2)
for pos,power,size in [((2,-4,-4),1000,5),((-4,1,-2),650,4),((2,4,-1),400,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);l=bpy.context.object;l.data.energy=power;l.data.shape='DISK';l.data.size=size;l.rotation_euler=(Vector((0,0,.25))-l.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4,-5,-5));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.25))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=6.3;scene.camera=camera
scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'coffer-detail.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z06-Ceiling.blend'))
bpy.ops.render.render(write_still=True)
print('Z06_CEILING_BUILD_PASS')
