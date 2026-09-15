"""Double-sided relay masonry within the native half-metre wall envelope."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z04WallKit';ROOT.mkdir(exist_ok=True)

def bay(name,w,h,rows):
    box('Continuous stone core',(0,0,h/2),(w-.016,.18,h-.024),stone,.005)
    for z,width,d,t in ((.065,w,.5,.13),(.175,w-.05,.44,.08),(h-.32,w-.04,.424,.10),(h-.20,w,.47,.12),(h-.065,w,.5,.13)):
        box('Entablature' if z>h/2 else 'Socle',(0,0,z),(width,d,t),stone,.008)
    field_height=h-.60;row_height=field_height/rows
    for side in (-1,1):
        for x in (-w/4,w/4):
            for j in range(rows):
                z=.27+(j+.5)*row_height
                box('Dressed ashlar',(x,side*.12,z),(w/2-.026,.12,row_height-.018),stone,.009)
                box('Relief field',(x,side*.194,z),(w/2-.36,.040,row_height-.16),stone,.007)
                for dx in (-w/4+.25,w/4-.25):
                    box('Field incision',(x+dx,side*.217,z),(.010,.008,row_height-.27),dark,.001)
        for x in (-w/2+.05,w/2-.05):
            box('Outer seam bed',(x,side*.19,h/2),(.068,.040,h-.5),dark,.004)
            box('Stone arris',(x,side*.223,h/2),(.034,.044,h-.5),stone,.005)
        box('Axial shadow',(0,side*.197,h/2),(.070,.038,h-.53),dark,.003)
        for x in (-.020,.020):box('Axial conductor',(x,side*.221,h/2),(.010,.012,h-.55),gold,.0015)
        for x in (-w/4,w/4):
            for j in range(5):box('Maintenance register',(x,side*.220,.39+j*.035),(.18,.014,.010),gold,.001)
        for x in (-w/4,w/4):
            box('Upper stone seal',(x,side*.219,h-.55),(.38,.032,.28),stone,.006)
            for dx in (-.14,.14):box('Seal register',(x+dx,side*.240,h-.55),(.015,.008,.19),gold,.001)
    o=export(name,[w,.5,h]);o.hide_render=True;return o

end=bay('SM_Aurelion_KIT_Z04Wall4m',4,7,5)
side=bay('SM_Aurelion_KIT_Z04WallSide',38/9,7,5)
lintel=bay('SM_Aurelion_KIT_Z04Lintel',6,2.5,2)
placements=[]
for y in (-12900,-9100):
    for start in (3900,7300):
        for i in range(7):placements.append(dict(asset=end.name,location_cm=[start+200+i*400,y,0],yaw=0))
    placements.append(dict(asset=lintel.name,location_cm=[7000,y,450],yaw=0))
for x in (3900,10100):
    for i in range(9):placements.append(dict(asset=side.name,location_cm=[x,-12900+(i+.5)*3800/9,0],yaw=90))
for s in manifest:s.update(position_precision=10,preserve_fallback_geometry=True,collision='None; native Z04 walls remain authoritative')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
end.hide_render=False;side.hide_render=False;side.location.x=4.4;lintel.hide_render=False;lintel.location=(-1,0,7.3)
scene.world=bpy.data.worlds.new('Relay masonry studio');scene.world.color=(.17,.17,.17)
for pos,power,size in [((3,-6,8),2400,5),((-5,-2,5),1500,4),((4,3,8),2000,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((1,0,4))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(13,-20,12));camera=bpy.context.object;camera.rotation_euler=(Vector((1,0,4.8))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=15;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'wall-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z04-Walls.blend'));bpy.ops.render.render(write_still=True)
