"""Fitted radial bridge paving and dressed fascia, with true chord-end joints."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'AtriumBridgeFloorKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)
slope=math.tan(math.radians(5.625))
# The ring polygon at each bridge is x=radius-|y|tan(half-sector).
# Local X is radial and Y is across the crossing; pivot radius is 23 m.
def point(u,y,z):return (-5+10*u-abs(y)*slope,y,z)
def block(name,u0,u1,y0,y1,z0,z1,mat=stone,bevel=.003):
    assert y0*y1>=0, 'Split across the chord cusp at y=0'
    vertices=[point(u,y,z) for z in (z0,z1) for u,y in ((u0,y0),(u1,y0),(u1,y1),(u0,y1))]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],[(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]);mesh.update()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);return finish(o,mat,bevel)
for sign in (-1,1):
    def band(name,u0,u1,a,b,z0,z1,mat=stone,bevel=.003):
        lo,hi=sorted((sign*a,sign*b));return block(name,u0,u1,lo,hi,z0,z1,mat,bevel)
    band('Continuous stone substrate',0,1,0,3,-.4,-.06,grout,0)
    # Dressed edge bands and central recessed navigation conductors.
    band('Central channel bed',0,1,0,.065,-.06,-.008,basalt,0)
    band('Recessed gold thread',.025,.975,.022,.036,-.008,-.002,gold,0)
    band('Channel shoulder',0,1,.065,.19,-.06,0)
    band('Edge coping',0,1,2.64,3,-.20,0)
    # Three staggered flagstone courses per half, 8 mm wide joints.
    for row in range(3):
        a=.20+row*(2.43/3);b=.20+(row+1)*(2.43/3)
        cuts=sorted(set([0,1]+[max(0,min(1,(i+(.5 if row%2 else 0))/6)) for i in range(7)]))
        for u0,u1 in zip(cuts,cuts[1:]):
            band('Honed staggered flagstone',u0+.0004,u1-.0004,a+.004,b-.004,-.06,0)
    band('Fascia mortar core',0,1,2.74,2.92,-1.20,-.20,grout,0)
    for z0,z1,a in ((-.38,-.21,2.70),(-.53,-.40,2.74),(-1.20,-1.07,2.73),(-1.32,-1.22,2.78)):
        band('Stepped bridge fascia',0,1,a,3,z0,z1)
    for i in range(8):
        band('Dressed fascia block',i/8+.0009,(i+1)/8-.0009,2.79,2.98,-1.05,-.55)
o=export('SM_Aurelion_KIT_AtriumBridgeFloor',[10+3*slope,6,1.32])
manifest[-1].update(collision='None; original bridge slabs remain physical',top_m=0,inner_radius_m=18,outer_radius_m=28,half_sector_degrees=5.625,radial_pivot_m=23)
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Candidate fitted paving; in-engine review pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Bridge paving studio');scene.world.color=(.16,.16,.16)
for pos,energy,size in (((1,-5,10),2600,8),((-8,4,6),1800,7)):
    d=bpy.data.lights.new('Paving softbox','AREA');d.energy=energy;d.size=size;l=bpy.data.objects.new('Paving softbox',d);scene.collection.objects.link(l);l.location=pos;l.rotation_euler=(Vector((0,0,-.3))-l.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Paving review');camera=bpy.data.objects.new('Paving review',d);scene.collection.objects.link(camera);scene.camera=camera
camera.location=(12,-11,10);camera.rotation_euler=(Vector((0,0,-.35))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=46
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'Bridge-floor.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_AtriumBridgeFloorKit.blend'));bpy.ops.render.render(write_still=True)
