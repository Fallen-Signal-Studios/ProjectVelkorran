"""Detailed 32-sided Aurelion ring floors fitted to the exported chord boundaries."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'AtriumRingKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)
angle=math.radians(11.25)
def point(r,u,z):return (r*(1-u+u*math.cos(angle)),-r*u*math.sin(angle),z)
def block(name,r0,r1,u0,u1,z0,z1,mat=stone,bevel=.004):
    vertices=[point(r,u,z) for z in (z0,z1) for r,u in ((r0,u0),(r1,u0),(r1,u1),(r0,u1))]
    faces=[(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);return finish(o,mat,bevel)
for name,ri,ro in (('Outer',28,36),('Inner',9,18)):
    # Original playable perimeter and elevation remain exact; all depth is below the floor.
    block('Continuous floor bed',ri,ro,0,1,-.60,-.07,grout,0)
    for r in (ri,ro-.34):
        center=ri+.21 if r==ri else ro-.21
        for start,end in ((r,center-.014),(center+.014,r+.34)):
            block('Dressed perimeter coping',start,end,0,1,-.20,0,stone,.003)
    usable0=ri+.36;usable1=ro-.36;rows=4
    for row in range(rows):
        r0=usable0+(usable1-usable0)*row/rows;r1=usable0+(usable1-usable0)*(row+1)/rows
        divisions=4 if name=='Outer' else 2
        cuts=sorted(set([0,1]+[max(0,min(1,(i+(.5 if row%2 else 0))/divisions)) for i in range(divisions+1)]))
        for index,(u0,u1) in enumerate(zip(cuts,cuts[1:])):
            du=.004/((r0+r1)*.5*math.sin(angle))
            block('Honed radial flagstone',r0+.004,r1-.004,u0+du,u1-du,-.07,0,stone,.003)
    # Paired restrained conductors mark the promenade edges, recessed into stone bands.
    for r in (ri+.21,ro-.21):
        block('Conductor recess',r-.014,r+.014,0,1,-.025,-.006,basalt,0)
        block('Inlaid gold conductor',r-.005,r+.005,.025,.975,-.006,-.001,gold,0)
    for edge in ('inner','outer'):
        r0,r1=(ri,ri+.52) if edge=='inner' else (ro-.52,ro)
        block('Continuous fascia backing',r0+.16,r1-.16,0,1,-2.18,-.20,grout,0)
        for z0,z1,inset in ((-.38,-.22,.015),(-.56,-.40,.045),(-2.26,-2.10,.03),(-2.40,-2.28,.08)):
            block('Stepped ring cornice',r0+inset,r1-inset,0,1,z0,z1,stone,.005)
        divisions=4 if name=='Outer' else 2
        for row in range(2):
            cuts=sorted(set([0,1]+[max(0,min(1,(i+(.5 if row else 0))/divisions)) for i in range(divisions+1)]))
            for u0,u1 in zip(cuts,cuts[1:]):
                du=.009/((r0+r1)*.5*math.sin(angle))
                block('Recessed fascia ashlar',r0+.10,r1-.10,u0+du,u1-du,-1.31-row*.77,-.58-row*.77,stone,.005)
        # Two layered load keys break up the long fascia with substantial carved geometry.
        for u in (.25,.75):
            du=.09/((r0+r1)*.5*math.sin(angle))
            for z0,z1,shrink in ((-2.08,-1.65,.10),(-1.65,-1.02,.065),(-1.02,-.57,.015)):
                block('Stepped fascia load key',r0+shrink,r1-shrink,u-du,u+du,z0,z1,stone,.006)
    dims=[ro-ri*math.cos(angle),ro*math.sin(angle),2.4]
    o=export('SM_Aurelion_KIT_Atrium'+name+'RingSector',dims);o.hide_render=True
    manifest[-1].update(collision='None; retained original ring collision remains authoritative',inner_radius_m=ri,outer_radius_m=ro,sector_degrees=11.25,top_m=0,perimeter='Straight chord boundary, not circular interpolation')
    # A quarter-ring studio assembly shows both the paving rhythm and the underside mass.
    for i in range(8):
        copy=o.copy();copy.data=o.data;scene.collection.objects.link(copy);copy.hide_render=False;copy.rotation_euler.z=-i*angle
scene.world=bpy.data.worlds.new('Atrium ring studio');scene.world.color=(.14,.14,.14)
for pos,energy,size in (((10,-15,25),15000,18),((35,5,12),12000,15),((-5,-30,8),9000,12)):
    d=bpy.data.lights.new('Ring softbox','AREA');d.energy=energy;d.size=size;o=bpy.data.objects.new('Ring softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((18,-18,-1))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Ring review');camera=bpy.data.objects.new('Ring review',d);scene.collection.objects.link(camera);scene.camera=camera
camera.location=(53,-49,30);camera.rotation_euler=(Vector((18,-18,-.7))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=43
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'Ring-quarter.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Candidate fitted ring floors; in-engine review pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_AtriumRingKit.blend'));bpy.ops.render.render(write_still=True)
