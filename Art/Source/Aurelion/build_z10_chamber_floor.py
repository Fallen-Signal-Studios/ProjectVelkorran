"""Exact-outline chamber paving and central dais, with flush radial inlays."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z10ChamberFloor'
profile=json.loads((ROOT/'source-profile.json').read_text(encoding='utf-8-sig'))['rim']
assert len(profile)==32
assert all(abs(math.hypot(x,y)-.5)<1e-6 for x,y in profile), 'Use only the outer rim, excluding cap-ring vertices.'
angles=[math.atan2(y,x) for x,y in profile]
assert all(abs((angles[(i+1)%32]-angles[i])%(2*math.pi)-2*math.pi/32)<1e-5 for i in range(32))
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)

def polyblock(name,points,z0,z1,mat,bevel=.002):
    n=len(points);vertices=[(x,y,z) for z in (z0,z1) for x,y in points]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj)
    return finish(obj,mat,bevel)

def ring_tile(name,index,r0,r1,z0,z1,mat,gap=.008):
    # Linear interpolation follows the original 32-sided outline, not an
    # enlarged circular arc between its corners.
    a=Vector(profile[index]);b=Vector(profile[(index+1)%32])
    fraction=min(.04,gap/(max(.5,(r0+r1)/2)*(2*math.pi/32))/2)
    p=a.lerp(b,fraction);q=a.lerp(b,1-fraction)
    points=[tuple(p*(r0*2)),tuple(p*(r1*2)),tuple(q*(r1*2)),tuple(q*(r0*2))]
    if r0==0:points=[(0,0),tuple(p*(r1*2)),tuple(q*(r1*2))]
    return polyblock(name,points,z0,z1,mat,.0015 if mat==gold else .003)

polyblock('Continuous recessed floor substrate',[(x*54,y*54) for x,y in profile],-.30,.22,dark,0)
bands=[(0,4.97,basalt),(4.98,5.45,stone),(5.46,5.51,gold),(5.52,11.96,basalt),
    (11.97,12.05,gold),(12.06,12.55,stone),(12.56,19.98,basalt),
    (19.99,20.05,gold),(20.06,26.50,basalt),(26.51,26.95,stone),(26.96,27,gold)]
for r0,r1,mat in bands:
    for index in range(32):
        chosen=stone if mat==basalt and index in (7,8,23,24) and r0>=5 else mat
        ring_tile('Radial paving / flush conductor',index,r0,r1,.22,.299 if chosen==gold else .30,chosen)
floor=export('SM_Aurelion_KIT_Z10ChamberPaving',[54,54,.60])
manifest[-1].update(nominal_dimensions_m=list(floor.dimensions),source_instance=0,surface_top_metres=.30,
    position_precision=10,preserve_fallback_geometry=True,collision='None: retained native floor remains physical')

polyblock('Dais stone core',[(x*9.92,y*9.92) for x,y in profile],-.10,.04,basalt,.003)
for index in range(32):
    ring_tile('Recessed gold fascia',index,4.96,5,-.085,-.035,gold,.003)
    for r0,r1,mat in [(0,3.58,basalt),(3.59,3.65,gold),(3.66,4.28,stone),
                       (4.29,4.35,gold),(4.36,4.54,basalt),(4.55,5,stone)]:
        ring_tile('Dais fitted coffer paving',index,r0,r1,.04,.099 if mat==gold else .10,mat)
dais=export('SM_Aurelion_KIT_Z10CrownmarkDais',[10,10,.20])
manifest[-1].update(nominal_dimensions_m=list(dais.dimensions),source_instance=1,surface_top_metres=.10,
    position_precision=10,preserve_fallback_geometry=True,collision='None: retained native dais remains physical')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,source_outline_vertices=32),indent=2))
dais.location.z=.40
scene.world=bpy.data.worlds.new('Chamber paving studio');scene.world.color=(.2,.2,.2)
target=Vector((0,0,0))
for pos,power,size in [((-15,-20,35),150000,25),((22,10,25),100000,20)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object
    o.data.energy=power;o.data.size=size;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(35,-48,60));o=bpy.context.object
o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=65;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1500;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'paving.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Chamber-Paving.blend'))
bpy.ops.render.render(write_still=True)
