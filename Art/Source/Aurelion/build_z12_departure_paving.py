"""Fitted departure lounge and dock paving; exact outlines and flush walking plane."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z12DeparturePaving';ROOT.mkdir(exist_ok=True)
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)

def slab(name,x0,x1,y0,y1,mat,top=0.,bottom=-.032,bevel=.002):
    assert x1>x0 and y1>y0 and top>bottom
    return box(name,((x0+x1)/2,(y0+y1)/2,(bottom+top)/2),(x1-x0,y1-y0,top-bottom),mat,bevel)

for name,w,h,nx,ny in [('Lounge',42.,24.,10,6),('Dock',28.,18.,7,5)]:
    slab('Continuous recessed substrate',-w/2,w/2,-h/2,h/2,dark,-.032,-.0462399,0)
    rim=.30;edge=.03;gap=.008;dx=(w-2*rim)/nx;dy=(h-2*rim)/ny
    for ix in range(nx):
        x0=-w/2+rim+ix*dx;x1=x0+dx
        for iy in range(ny):
            y0=-h/2+rim+iy*dy;y1=y0+dy
            # Cut the paving at the inlay boundaries: stone and metal occupy
            # disjoint faces at the same walking plane, without overlay flicker.
            xs=sorted({x0+gap/2,x1-gap/2}|{v for v in (-5.4,-5.1,-5.02,-4.97,4.97,5.02,5.1,5.4) if x0+gap/2<v<x1-gap/2})
            ys=sorted({y0+gap/2,y1-gap/2}|{v for v in (-7.8,-7.5,-7.42,-7.37,7.37,7.42,7.5,7.8) if name=='Lounge' and y0+gap/2<v<y1-gap/2})
            for xa,xb in zip(xs,xs[1:]):
                for ya,yb in zip(ys,ys[1:]):
                    x=abs((xa+xb)/2);y=abs((ya+yb)/2)
                    ivory=(5.1<x<5.4 and (name=='Dock' or y<7.8)) or (name=='Lounge' and 7.5<y<7.8 and x<5.4)
                    metal=(4.97<x<5.02 and (name=='Dock' or y<7.42)) or (name=='Lounge' and 7.37<y<7.42 and x<5.02)
                    chosen=stone if ivory else gold if metal else basalt
                    slab('Flush axial paving frame' if ivory or metal else 'Honed basalt paving',xa,xb,ya,yb,chosen,-.001 if metal else 0.,bevel=.001)
            # Keys occupy the grout crossing, never overlap paving faces.
            if ix<nx-1 and iy<ny-1:
                slab('Joint conductor',x1-.003,x1+.003,y1-.10,y1+.10,gold,-.001,-.028,.001)
    for ix in range(nx):
        x0=-w/2+rim+ix*dx;x1=x0+dx
        for side in (-1,1):
            y0,y1=((-h/2,-h/2+rim-edge) if side<0 else (h/2-rim+edge,h/2))
            slab('Perimeter ashlar',x0+gap/2,x1-gap/2,y0,y1,stone)
            y0,y1=((-h/2+rim-edge,-h/2+rim-gap/2) if side<0 else (h/2-rim+gap/2,h/2-rim+edge))
            slab('Perimeter gold seam',x0+gap/2,x1-gap/2,y0,y1,gold,-.001,bevel=.001)
    for iy in range(ny):
        y0=-h/2+iy*h/ny;y1=y0+h/ny
        for side in (-1,1):
            x0,x1=((-w/2,-w/2+rim-edge) if side<0 else (w/2-rim+edge,w/2))
            slab('Perimeter return ashlar',x0,x1,y0+gap/2,y1-gap/2,stone)
            x0,x1=((-w/2+rim-edge,-w/2+rim-gap/2) if side<0 else (w/2-rim+gap/2,w/2-rim+edge))
            slab('Perimeter return conductor',x0,x1,y0+gap/2,y1-gap/2,gold,-.001,bevel=.001)
    obj=export('SM_Aurelion_KIT_Departure'+name+'Paving',[w,h,.0462399])
    manifest[-1].update(nominal_dimensions_m=list(obj.dimensions),surface_top_metres=0.,position_precision=10,
        preserve_fallback_geometry=True,collision='None: original native floor and dock collision retained',group=name)
    if name=='Dock':obj.location.x=39
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Paving studio');scene.world.color=(.15,.15,.15)
target=Vector((12,0,0))
for pos,power,size in [((-15,-20,35),180000,25),((35,10,30),180000,25)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object
    o.data.energy=power;o.data.size=size;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(40,-48,65));o=bpy.context.object
o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=82;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'departure-paving.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Departure-Paving.blend'))
bpy.ops.render.render(write_still=True)
