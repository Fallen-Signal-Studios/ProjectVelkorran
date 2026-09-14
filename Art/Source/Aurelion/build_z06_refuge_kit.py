"""Fitted refuge landing and ramp surfaces; centre pivots retain surveyed transforms."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z06RefugeKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)

def landing(name,length,width,nx,ny):
    # Solid backing ends 6 mm below the physical top. Joint recesses cannot open holes.
    box('Continuous stone bed',(0,0,-.003),(length-.10,width-.10,.394),grout,.001)
    box('Continuous cap joint backing',(0,0,.184),(length,width,.020),grout,.001)
    # Broad capstones and a separate perimeter course give the walking face readable scale.
    border=.24;inner_x=length-2*border;inner_y=width-2*border
    for i in range(nx):
        for j in range(ny):
            x=-inner_x/2+(i+.5)*inner_x/nx;y=-inner_y/2+(j+.5)*inner_y/ny
            box('Dressed walking slab',(x,y,.15),(inner_x/nx-.022,inner_y/ny-.022,.10),stone,.006)
    for side in (-1,1):
        for i in range(nx):
            x=-length/2+(i+.5)*length/nx
            box('Long perimeter cap',(x,side*(width/2-border/2),.15),(length/nx-.018,border-.018,.10),stone,.006)
        for j in range(ny):
            y=-inner_y/2+(j+.5)*inner_y/ny
            box('End perimeter cap',(side*(length/2-border/2),y,.15),(border-.018,inner_y/ny-.018,.10),stone,.006)
        # Horizontal fascia: substantial dressed stone, recessed middle bed and restrained gold.
        box('Long lower fascia',(0,side*(width/2-.025),-.135),(length,.05,.09),stone,.006)
        box('Long fascia return',(side*(length/2-.025),0,-.135),(.05,width-.10,.09),stone,.006)
        box('Long upper fascia',(0,side*(width/2-.025),.065),(length,.05,.07),stone,.006)
        box('Upper fascia return',(side*(length/2-.025),0,.065),(.05,width-.10,.07),stone,.006)
        box('Recessed long conductor',(0,side*(width/2-.034),-.045),(length-.16,.012,.022),gold,.002)
        box('Recessed return conductor',(side*(length/2-.034),0,-.045),(.012,width-.16,.022),gold,.002)
    obj=export(name,[length,width,.4])
    manifest[-1].update(nominal_dimensions_m=list(obj.dimensions),collision='None; visual surface fitted to retained surveyed physical proxy',physical_top_local_m=.2,physical_bottom_local_m=-.2,pivot='Physical proxy centre; apply its rotation with unit scale')
    return obj

platform=landing('SM_Aurelion_KIT_Z06RefugeLanding',6,10,3,5)
ramp=landing('SM_Aurelion_KIT_Z06RefugeRamp',4.243106,4,4,2)
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; Unreal placement and visual acceptance pending',modules=manifest),indent=2))
# Move only the studio display after export; FBX assets retain centred pivots.
platform.location.x=-3.6;ramp.location.x=3.3
scene.render.engine='CYCLES';scene.cycles.samples=48
scene.world=bpy.data.worlds.new('Refuge studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((2,-7,10),1800,7),((-7,3,7),1300,6)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(13,-17,17));camera=bpy.context.object;camera.rotation_euler=(Vector((-.7,0,0))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=17;scene.camera=camera
scene.render.resolution_x=1600;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'refuge-surfaces.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z06-Refuge.blend'));bpy.ops.render.render(write_still=True)
print('Z06_REFUGE_BUILD_PASS')
