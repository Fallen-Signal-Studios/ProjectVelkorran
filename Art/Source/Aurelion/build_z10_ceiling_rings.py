"""Three curved Aurelion cornices fitted to the existing 32-sector overhead rings."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z10CeilingRings';ROOT.mkdir(exist_ok=True)
half=math.pi/32

def band(name,radius,inner,outer,a0,a1,z0,z1,mat=stone,bevel=.008):
    steps=max(1,round((a1-a0)/(half*2)*24))
    angles=[a0+(a1-a0)*i/steps for i in range(steps+1)]
    verts=[(r*math.sin(a),r*math.cos(a)-radius,z)
           for a in angles for z,r in ((z0,radius+inner),(z0,radius+outer),(z1,radius+outer),(z1,radius+inner))]
    faces=[(3,2,1,0),(4*steps,4*steps+1,4*steps+2,4*steps+3)]
    for i in range(steps):
        for k in range(4):faces.append((4*i+k,4*i+(k+1)%4,4*(i+1)+(k+1)%4,4*(i+1)+k))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj)
    return finish(obj,mat,bevel)

for radius in (32,40,50):
    band('Continuous curved shadow bed',radius,-.23,.23,-half,half,-.38,.38,dark,0)
    for z0,z1,width in ((-.5,-.39,.35),(-.37,-.26,.30),(.26,.37,.30),(.39,.5,.35)):
        band('Dressed stepped cornice',radius,-width,width,-half,half,z0,z1)
    # Broad readable conductors, recessed between the stone courses.
    for z0,z1 in ((-.39,-.32),(.32,.39)):
        band('Gold conductor',radius,-.31,.31,-half,half,z0,z1,gold,.004)
    for i in range(4):
        a0=-half+2*half*i/4+.025/radius;a1=-half+2*half*(i+1)/4-.025/radius
        band('Inset honed frieze bay',radius,-.27,.27,a0,a1,-.245,.245)
        # Substantial repeated relief, readable from the chamber floor.
        center=(a0+a1)/2
        for offset in (-.18,0,.18):
            band('Raised stepped register',radius,-.295,.295,center+(offset-.045)/radius,center+(offset+.045)/radius,-.15,.15,gold,.006)
    for i in range(5):
        center=-half+2*half*i/4
        a0=max(-half,center-.055/radius);a1=min(half,center+.055/radius)
        band('Carved load key',radius,-.33,.33,a0,a1,-.27,.27,stone,.008)
    # Exact evaluated bounds feed the import gate; pivots stay at the old sector centers.
    coords=[o.matrix_world@v.co for o in parts for v in o.data.vertices]
    dims=[max(v[i] for v in coords)-min(v[i] for v in coords) for i in range(3)]
    obj=export('SM_Aurelion_KIT_Z10CeilingRing'+str(radius),dims)
    manifest[-1].update(radius_m=radius,sector_degrees=11.25,instances=32,
        position_precision=10,preserve_fallback_geometry=True,
        collision='None; existing gameplay collision untouched')
    obj.location=(0,(radius-32)*.19,0)

scene.world=bpy.data.worlds.new('Cornice studio');scene.world.color=(.15,.15,.15)
for pos,energy,size in (((1,-6,7),1900,6),((-6,2,5),1600,5),((6,5,4),1800,5)):
    d=bpy.data.lights.new('Cornice softbox','AREA');d.energy=energy;d.size=size
    o=bpy.data.objects.new('Cornice softbox',d);scene.collection.objects.link(o);o.location=pos
    o.rotation_euler=(Vector((0,1,0))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Cornice review');camera=bpy.data.objects.new('Cornice review',d);scene.collection.objects.link(camera);scene.camera=camera
camera.location=(9,-12,9);camera.rotation_euler=(Vector((0,1.5,0))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=48
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'Ceiling-ring-modules.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Ceiling-Rings.blend'))
bpy.ops.render.render(write_still=True)
