"""Relay paving, wide ascent modules and a fully dressed overlook balcony."""
from pathlib import Path
helper=Path(__file__).with_name('build_z03_floor_kit.py')
exec(compile(helper.read_text().split("room=paving(")[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z04FloorKit';ROOT.mkdir(exist_ok=True)

def deck_module(name,w,d,rise):
    nx,ny=math.ceil(w/2),math.ceil(d/2)
    slab('Continuous deck core',-w/2+.02,w/2-.02,-d/2+.01,d/2-.01,-.06,.31,rise,d,stone,.005)
    for i in range(nx):
        for j in range(ny):
            slab('Walking ashlar',-w/2+i*w/nx+.004,-w/2+(i+1)*w/nx-.004,
                 -d/2+j*d/ny+.004,-d/2+(j+1)*d/ny-.004,0,.065,rise,d,stone,.002)
    for side in (-1,1):
        x0,x1=(-w/2,-w/2+.16) if side<0 else (w/2-.16,w/2)
        slab('Continuous perimeter fascia',x0,x1,-d/2,d/2,-.075,.325,rise,d,stone,.004)
        xx0,xx1=(-w/2-.001,-w/2+.006) if side<0 else (w/2-.006,w/2+.001)
        slab('Recessed edge register',xx0,xx1,-d/2+.02,d/2-.02,-.18,.08,rise,d,dark,.001)
        slab('Fine edge conductor',xx0-.001 if side<0 else xx0+.002,xx1-.002 if side<0 else xx1+.001,
             -d/2+.04,d/2-.04,-.20,.008,rise,d,gold,.0007)
    for y0,y1 in ((-d/2,-d/2+.10),(d/2-.10,d/2)):
        slab('Deck end closure',-w/2+.16,w/2-.16,y0,y1,-.075,.325,rise,d,stone,.004)
    # Recessed underside bays and continuous load courses are visible from below.
    for i in range(nx):
        x=-w/2+(i+.5)*w/nx
        for j in range(ny):
            y0,y1=-d/2+j*d/ny+.14,-d/2+(j+1)*d/ny-.14
            slab('Underside coffer field',x-w/nx/2+.18,x+w/nx/2-.18,y0,y1,-.371,.008,rise,d,dark,.001)
    for i in range(nx+1):
        x=-w/2+i*w/nx
        x0,x1=max(-w/2+.17,x-.06),min(w/2-.17,x+.06)
        if x1>x0:slab('Underside longitudinal course',x0,x1,-d/2+.11,d/2-.11,-.374,.026,rise,d,stone,.002)
    for j in range(ny+1):
        y=-d/2+j*d/ny
        y0,y1=max(-d/2+.11,y-.06),min(d/2-.11,y+.06)
        if y1>y0:
            for i in range(nx):
                x0=-w/2+i*w/nx+(.17 if i==0 else .06)
                x1=-w/2+(i+1)*w/nx-(.17 if i==nx-1 else .06)
                slab('Underside transverse course',x0,x1,y0,y1,-.374,.026,rise,d,stone,.002)
    o=export(name,[w+.004,d,rise+.4]);manifest[-1]['nominal_dimensions_m']=list(o.dimensions)
    o.hide_render=True;return o

room=paving('SM_Aurelion_KIT_Z04Paving',3.875,3.8)
ramp=deck_module('SM_Aurelion_KIT_Z04Ascent',6,12,3)
balcony=deck_module('SM_Aurelion_KIT_Z04Balcony',14,11,0)
low=paving('SM_Aurelion_KIT_Z04LowCoverCap',3,2)
high=paving('SM_Aurelion_KIT_Z04HighCoverCap',3,3)
for row in manifest:row.update(position_precision=10,preserve_fallback_geometry=True,collision='None; native floor, ramp, balcony and cover collision retained')
placements=[dict(asset=room.name,location_cm=[3900+(i+.5)*387.5,-12900+(j+.5)*380,0],yaw=0) for i in range(16) for j in range(10)]
placements.extend([dict(asset=ramp.name,location_cm=[9300,-11200,0],yaw=0),
                   dict(asset=ramp.name,location_cm=[7700,-9900,0],yaw=-90),
                   dict(asset=balcony.name,location_cm=[9000,-10050,300],yaw=0),
                   dict(asset=low.name,location_cm=[9050,-10300,420],yaw=0),
                   dict(asset=high.name,location_cm=[8350,-10800,220],yaw=0)])
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
ramp.hide_render=False;balcony.hide_render=False;balcony.location=(11,0,1)
scene.world=bpy.data.worlds.new('Relay deck studio');scene.world.color=(.16,.16,.16)
target=Vector((7,0,1))
for pos,power,size in [((2,-6,10),4000,8),((-5,4,6),2300,6),((16,4,9),4000,8)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(25,-30,25));camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=29;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1500;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'floor-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z04-Floors.blend'));bpy.ops.render.render(write_still=True)
