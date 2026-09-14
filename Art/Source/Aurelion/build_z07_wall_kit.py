"""Six-metre capture-gallery masonry: fitted bays, piers and clear-span lintels."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z07WallKit';ROOT.mkdir(exist_ok=True)
def wall(name,w):
    box('Continuous structural backing',(0,.25,3),(w-.012,.18,6),stone,.006)
    for x in (-w/4,w/4):
        for j in range(4):box('Dressed ashlar course',(x,.015,.90+j*1.26),(w/2-.026,.50,1.238),stone,.018)
    for z,width,depth,h in ((.08,w,.70,.16),(.22,w-.068,.62,.10),(5.32,w-.04,.62,.12),(5.47,w,.78,.18),(5.63,w-.02,.72,.12),(5.84,w,.85,.32)):
        box('Entablature' if z>5 else 'Socle',(0,0,z),(width,depth,h),stone,.015)
    for side in (-1,1):
        x=side*(w/2-.08)
        box('Outer recessed seam',(x,-.263,2.80),(.14,.043,4.98),dark,.005)
        box('Stone arris',(x,-.318,2.80),(.073,.11,5.0),stone,.008)
        pts=[(side*(w/2-.28),-.30,.40),(side*(w/2-.28),-.30,3.91),(side*(w/2-1.02),-.30,4.65),(side*(w/2-1.02),-.30,5.22)]
        path('Oblique conduit bed',pts,.16,.05,dark)
        path('Recessed gold conduit',[(x,y-.035,z) for x,y,z in pts],.034,.028,gold)
        for j in range(9):box('Service register',(side*(w/2-.52),-.287,.55+j*.047),(.26,.04,.017),gold,.003)
        for z in (1.42,3.35):
            px=side*w/4;pw=w/2-.62
            box('Inset stone field',(px,-.27,z),(pw,.075,1.60),stone,.022)
            for dx in (-pw/2+.08,pw/2-.08):box('Field incision',(px+dx,-.312,z),(.012,.008,1.40),dark,.002)
    box('Axial shadow',(0,-.295,2.78),(.15,.07,4.87),dark,.006)
    for x in (-.059,.059):box('Axial rail',(x,-.345,2.78),(.014,.028,4.87),gold,.003)
    o=export(name,[w,.85,6]);o.hide_render=True;return o
side=wall('SM_Aurelion_KIT_Z07WallSide',4.4)
end=wall('SM_Aurelion_KIT_Z07WallEnd',14/3)
for z,w,d,h in ((.11,1.2,1.02,.22),(.31,1.1,.92,.18),(2.88,.76,.68,4.96),(5.44,.94,.82,.18),(5.63,1.08,.95,.20),(5.84,1.2,1.04,.32)):
    box('Pier dressed course',(0,0,z),(w,d,h),stone,.018)
for x in (-.32,.32):
    box('Pier flute',(x,-.352,2.85),(.072,.035,4.76),dark,.006)
    box('Pier conductor',(x,-.38,2.85),(.022,.035,4.70),gold,.004)
for z in (.65,2.12,3.69,5.08):
    box('Pier collar',(0,-.04,z),(.81,.76,.085),stone,.009)
    box('Collar inlay',(0,-.43,z),(.66,.025,.018),gold,.003)
pier=export('SM_Aurelion_KIT_Z07Pier',[1.2,1.04,6]);pier.hide_render=True
box('Lintel continuous backing',(0,.22,.75),(6,.22,1.5),stone,.006)
for x in (-2,0,2):box('Lintel ashlar',(x,0,.64),(1.984,.50,1.24),stone,.018)
for z,w,d,h in ((.08,6,.70,.16),(1.04,5.96,.62,.12),(1.19,6,.78,.18),(1.40,6,.85,.20)):
    box('Lintel course',(0,0,z),(w,d,h),stone,.015)
for x in (-2,0,2):
    box('Lintel relief panel',(x,-.29,.62),(1.64,.08,.65),stone,.018)
    for z in (.32,.92):box('Lintel register groove',(x,-.337,z),(1.50,.016,.026),dark,.003)
    for dx in (-.71,.71):box('Lintel gold register',(x+dx,-.352,.62),(.022,.025,.52),gold,.003)
lintel=export('SM_Aurelion_KIT_Z07Lintel',[6,.85,1.5]);lintel.hide_render=True
for row in manifest:row['collision']='None; native gallery collision remains authoritative'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Fitted gallery wall kit; in-engine visual review required',modules=manifest),indent=2))
side.hide_render=False;pier.hide_render=False;pier.location.x=3.0;lintel.hide_render=False;lintel.location=(0,0,6.12)
scene.world=bpy.data.worlds.new('Gallery wall studio');scene.world.color=(.14,.14,.14)
for pos,power,size in [((1,-6,7),2100,5),((-5,-2,4),1300,4),((4,2,7),1600,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((0,0,3))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(10,-17,10));camera=bpy.context.object;camera.rotation_euler=(Vector((.5,0,3.8))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=12;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'wall-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z07-Walls.blend'));bpy.ops.render.render(write_still=True)
