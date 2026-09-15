"""Sensor gallery paving and fitted service deck; all walk faces use native datums."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z03FloorKit';ROOT.mkdir(exist_ok=True)

def slab(name,x0,x1,y0,y1,top,thickness,rise,length,mat,bevel=.004):
    points=[(x0,y0),(x1,y0),(x1,y1),(x0,y1)]
    verts=[(x,y,top+rise*(.5-y/length)-drop) for drop in (thickness,0) for x,y in points]
    faces=[(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
    o=bpy.data.objects.new(name,m);scene.collection.objects.link(o)
    finish(o,mat,min(bevel,thickness*.2,(x1-x0)*.2,(y1-y0)*.2))

def paving(name,w,d):
    slab('Continuous joint backing',-w/2,w/2,-d/2,d/2,-.012,.063,0,d,dark,0)
    for i in range(2):
        for j in range(3):
            slab('Cut paving stone',-w/2+i*w/2+.004,-w/2+(i+1)*w/2-.004,-d/2+j*d/3+.004,-d/2+(j+1)*d/3-.004,0,.025,0,d,stone,.002)
    o=export(name,[w,d,.075]);o.hide_render=True;return o

def platform(name,rise):
    w,d=4,12
    slab('Continuous deck core',-1.98,1.98,-5.99,5.99,-.06,.31,rise,d,stone,.005)
    for i in range(2):
        for j in range(6):
            slab('Walking ashlar',-2+i*2+.004,-2+(i+1)*2-.004,-6+j*2+.004,-6+(j+1)*2-.004,0,.065,rise,d,stone,.002)
    for side in (-1,1):
        x0,x1=(-2,-1.84) if side<0 else (1.84,2)
        slab('Edge load-bearing fascia',x0,x1,-6,6,-.075,.325,rise,d,stone,.004)
        xx0,xx1=(-2.001,-1.994) if side<0 else (1.994,2.001)
        slab('Recessed side register',xx0,xx1,-5.98,5.98,-.18,.08,rise,d,dark,.001)
        slab('Fine side conductor',xx0-.001 if side<0 else xx0+.002,xx1-.002 if side<0 else xx1+.001,-5.96,5.96,-.20,.008,rise,d,gold,.0007)
    for j in range(6):
        # Shallow underside coffers sit below the core, within the 40cm deck depth.
        y0,y1=-6+j*2+.12,-6+(j+1)*2-.12
        slab('Underside recessed field',-1.68,1.68,y0,y1,-.371,.008,rise,d,dark,.001)
        for x0,x1 in ((-1.82,-1.69),(1.69,1.82)):
            slab('Underside longitudinal course',x0,x1,y0,y1,-.374,.026,rise,d,stone,.002)
    for j in range(7):
        y=-6+j*2; y0,y1=max(-6,y-.07),min(6,y+.07)
        slab('Underside transverse course',-1.82,1.82,y0,y1,-.374,.026,rise,d,stone,.002)
    o=export(name,[4.004,12,3.4 if rise else .4]);manifest[-1]['nominal_dimensions_m']=list(o.dimensions)
    o.hide_render=True;return o

room=paving('SM_Aurelion_KIT_Z03Paving',11/3,4)
bridge=paving('SM_Aurelion_KIT_Z03BridgePaving',3,4.224)
slope=platform('SM_Aurelion_KIT_Z03ServiceSlope',3)
deck=platform('SM_Aurelion_KIT_Z03ServiceDeck',0)
for row in manifest:row.update(position_precision=10,preserve_fallback_geometry=True,collision='None; native floor and platform collision retained')
placements=[]
for i in range(6):
    for j in range(12):placements.append(dict(asset=room.name,location_cm=[5900+(i+.5)*1100/3,-19600+j*400,0],yaw=0))
for x in (6850,7150):
    for j in range(5):placements.append(dict(asset=bridge.name,location_cm=[x,-14794.8+j*422.4,0],yaw=0))
placements.extend([dict(asset=slope.name,location_cm=[7600,-18500,0],yaw=0),dict(asset=slope.name,location_cm=[7600,-16100,0],yaw=180),dict(asset=deck.name,location_cm=[7600,-17300,300],yaw=0)])
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
slope.hide_render=False;deck.hide_render=False;deck.location=(5,0,1)
scene.world=bpy.data.worlds.new('Sensor platform studio');scene.world.color=(.16,.16,.16)
for pos,power,size in [((2,-6,9),2400,6),((-5,4,5),1700,5),((8,4,7),2200,5)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((2,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(14,-20,15));camera=bpy.context.object;camera.rotation_euler=(Vector((2,0,1))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=18;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'floor-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z03-Floors.blend'));bpy.ops.render.render(write_still=True)
