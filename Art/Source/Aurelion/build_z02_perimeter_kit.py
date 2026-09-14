"""Measured Survivor Bend perimeter: coursed masonry and double-faced friezes."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z02PerimeterKit'; ROOT.mkdir(exist_ok=True)
fit=json.loads((ROOT/'perimeter-fit.json').read_text())
h=fit['wall_height_m']
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)

def wall(width,name):
    # Local -Y is the interior face; stone projects only 5 mm beyond the old wall.
    box('Recessed stone grout core',(0,.15,h/2),(width,.22,h),grout,.002)
    rows=14; rh=h/rows
    for row in range(rows):
        edges=[-width/2]+[v for v in [-width/2+(.65 if row%2 else 1.3)+1.3*i for i in range(5)] if -width/2+.02<v<width/2-.02]+[width/2]
        for left,right in zip(edges,edges[1:]):
            box('Staggered limestone ashlar',((left+right)/2,.025,(row+.5)*rh),(right-left-.008,.06,rh-.008),stone,.004)
    for z,height,depth in ((.14,.28,.16),(.32,.05,.10),(h-.16,.32,.14),(h-.36,.045,.085)):
        box('Cut stone footing and cornice',(0,-depth/2+.005,z),(width,depth,height),stone,.008)
    for z in (.37,h-.39):
        box('Recessed conductor bed',(0,-.014,z),(width-.02,.018,.042),dark,.002)
        box('Fine conductor inlay',(0,-.025,z),(width-.03,.008,.014),gold,.001)
    # Keep the construction joints legible without repeated decorative dash marks.
    obj=export(name,[width,.42,h]); manifest[-1]['nominal_dimensions_m']=list(obj.dimensions)
    manifest[-1]['collision']='None; original outer wall collision retained'
    obj.hide_render=True
    return obj

regular=wall(4,'SM_Aurelion_KIT_Z02Perimeter_4m')
closing=wall(fit['wall_length_m']-16,'SM_Aurelion_KIT_Z02Perimeter_Closing')

w=fit['end_width_m']; eh=fit['end_height_m']; depth=fit['end_depth_m']
box('End frieze stone core',(0,0,eh/2),(w,depth,eh),stone,.008)
for face in (-1,1):
    # Front and back are finished for views from the nave and bridge approaches.
    for z,height,d in ((.15,.30,.15),(.39,.07,.09),(eh-.16,.32,.15),(eh-.40,.07,.09)):
        box('Frieze entablature',(0,face*(depth/2+d/2-.015),z),(w,d,height),stone,.01)
    for i in range(8):
        cw=w/8; x=-w/2+(i+.5)*cw
        box('Carved panel recess',(x,face*(depth/2+.012),eh/2),(cw-.07,.025,eh-1.02),dark,.004)
        box('Recessed ivory field',(x,face*(depth/2+.030),eh/2),(cw-.18,.030,eh-1.14),stone,.008)
        # Geometric relief: rising split chevron, repeated as structural language.
        for direction in (-1,1):
            pts=[(x+direction*(cw*.32),face*(depth/2+.062),.76),
                 (x+direction*(cw*.32),face*(depth/2+.062),eh*.49),
                 (x,face*(depth/2+.062),eh-.73)]
            # Common path extrudes +Y; mirror the negative face's depth.
            if face<0:pts=[(px,py-.03,pz) for px,py,pz in pts]
            path('Carved rising chevron',pts,.065,.03,stone)
        box('Relief central conductor',(x,face*(depth/2+.093),eh/2),(.018,.012,.70),gold,.001)
    for z in (.49,eh-.50):
        box('Frieze conductor',(0,face*(depth/2+.051),z),(w-.07,.014,.024),gold,.002)
end=export('SM_Aurelion_KIT_Z02EndFrieze',[w,depth+.27,eh]); manifest[-1]['nominal_dimensions_m']=list(end.dimensions)
manifest[-1]['collision']='None; original elevated end-panel collision retained'

regular.hide_render=False; regular.location=(-w/2-2.25,0,0)
scene.world=bpy.data.worlds.new('Perimeter studio'); scene.world.color=(.16,.16,.16)
for pos in ((-6,-9,10),(8,-5,8)):
    d=bpy.data.lights.new('Stone review softbox','AREA');d.energy=2200;d.size=7
    o=bpy.data.objects.new('Stone review softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((-3,0,4))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Perimeter review'); camera=bpy.data.objects.new('Perimeter review',d);scene.collection.objects.link(camera)
camera.location=(11,-24,11);camera.rotation_euler=(Vector((-3,0,4))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=40;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'Perimeter-assembly.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Measured perimeter candidate; final art and live gameplay acceptance pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_Z02PerimeterKit.blend'));bpy.ops.render.render(write_still=True)
