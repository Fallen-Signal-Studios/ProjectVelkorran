"""Fourteen fitted Aurelion parapet profiles, including mitered ring joints."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'AtriumParapetKit';fit=json.loads((ROOT/'guard-fit.json').read_text())
def clip(poly,a,b,left):
    result=[]
    for p,q in zip(poly,poly[1:]+poly[:1]):
        dp=(p[0]-a-b*p[1])*(1 if left else -1);dq=(q[0]-a-b*q[1])*(1 if left else -1)
        if dp>=-1e-10:result.append(p)
        if (dp<0)!=(dq<0):
            t=dp/(dp-dq);result.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
    return result
def piece(name,x0,x1,y0,y1,z0,z1,mat=stone,bevel=.004):
    poly=clip(clip([(x0,y0),(x1,y0),(x1,y1),(x0,y1)],la,lb,True),ra,rb,False)
    assert len(poly)>=3
    n=len(poly);vertices=[(x,y,z) for z in (z0,z1) for x,y in poly]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update();o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);return finish(o,mat,bevel)
for profile in fit['profiles']:
    length,la,lb,ra,rb=profile['values'];lo=-length/2;hi=length/2
    piece('Dressed footing',lo,hi,-.11,.11,0,.18,stone,.006)
    piece('Solid stone core',lo,hi,-.065,.065,.18,1.12,stone,.005)
    piece('Upper reveal course',lo,hi,-.085,.085,1.12,1.15,dark,.002)
    piece('Honed hand-rest cornice',lo,hi,-.11,.11,1.15,1.30,stone,.007)
    for x0,x1 in ((lo,lo+.14),(hi-.14,hi)):piece('Joint pier',x0,x1,-.11,.11,.18,1.15,stone,.005)
    usable0=la+.20;usable1=ra-.20;count=max(1,math.ceil((usable1-usable0)/1.45));step=(usable1-usable0)/count
    for side in (-1,1):
        for i in range(count):
            x0=usable0+i*step+.025;x1=usable0+(i+1)*step-.025
            def panel(name,a,b,depth0,depth1,z0,z1,mat,bevel=.002):
                ys=sorted((side*depth0,side*depth1));piece(name,a,b,*ys,z0,z1,mat,bevel)
            panel('Recessed panel bed',x0,x1,.065,.077,.27,1.03,dark)
            panel('Dressed inset panel',x0+.025,x1-.025,.077,.085,.31,.99,stone)
            for x in (x0+.055,x1-.075):panel('Carved panel border',x,x+.02,.085,.095,.36,.94,stone)
            for z in (.36,.92):panel('Panel cross border',x0+.055,x1-.055,.085,.095,z,z+.02,stone)
            mid=(x0+x1)/2;panel('Short functional inlay',mid-.006,mid+.006,.095,.104,.51,.79,gold,0)
    coords=[v.co for o in parts for v in o.data.vertices]
    dimensions=[max(v[i] for v in coords)-min(v[i] for v in coords) for i in range(3)]
    o=export(profile['asset'],dimensions);o.hide_render=True;manifest[-1].update(collision='None; original solid guard collision retained',miter_profile=profile['values'])
lookup={o.name:o for o in modules}
for spec in fit['placements']:
    o=lookup[spec['mesh']].copy();o.data=lookup[spec['mesh']].data;scene.collection.objects.link(o);o.hide_render=False
    o.location=(spec['position'][0]/100,-spec['position'][1]/100,0);o.rotation_euler.z=-math.radians(spec['yaw'])
scene.world=bpy.data.worlds.new('Atrium parapet studio');scene.world.color=(.18,.18,.18)
for pos in ((40,-10,15),(10,-30,18)):
    d=bpy.data.lights.new('Parapet softbox','AREA');d.energy=8000;d.size=15;o=bpy.data.objects.new('Parapet softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((28,-15,0))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Parapet ring review');camera=bpy.data.objects.new('Parapet ring review',d);scene.collection.objects.link(camera);scene.camera=camera
camera.location=(43,-26,6);camera.rotation_euler=(Vector((29,-14,.6))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=40
scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'Ring-parapets.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Mitered custom parapet candidate; runtime assembly review required',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_AtriumParapetKit.blend'));bpy.ops.render.render(write_still=True)
