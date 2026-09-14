"""Fitted Z01 end-wall kit: six-metre passage, deep stone frame and journal door."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'EndwallKit'; ROOT.mkdir(exist_ok=True)

# Nominal proxy aperture is 6 x 4.5m. The retained monolithic collision has
# a soffit at approximately 4.224m; fit a 4.2m visible intrados below it.
for side in (-1,1):
    x=side*3.5
    box('Continuous jamb backing',(x,.12,2.25),(1,.76,4.5),stone,.008)
    for j in range(6):
        box('Load bearing jamb ashlar',(x,-.10,.375+j*.75),(.99,1.04,.738),stone,.016)
        box('Inset jamb face',(x,-.635,.375+j*.75),(.58,.055,.55),stone,.012)
    for dx in (-.35,.35):
        box('Jamb vertical recess',(x+dx,-.64,2.25),(.065,.035,4.12),dark,.003)
        box('Jamb conductor',(x+dx,-.665,2.25),(.018,.025,4.08),gold,.003)
    for z in (.12,.35,4.27,4.43):
        box('Jamb collar',(x,-.13,z),(1,1.3,.12 if z>.2 else .24),stone,.018)
        box('Collar brass lip',(x,-.795,z),(.91,.02,.022),gold,.003)

box('Lintel continuous structural core',(0,.13,5.74),(8,.74,2.48),stone,.008)
for row in range(3):
    for i in range(8):
        box('Lintel cut stone course',(-3.5+i,-.09,4.87+row*.69),(.984,1.02,.674),stone,.018)
for z,depth,h in ((4.57,1.22,.14),(4.73,1.12,.08),(6.62,1.2,.13),(6.79,1.36,.18),(6.94,1.26,.12)):
    box('Portal layered entablature',(0,-.10,z),(8,depth,h),stone,.015)
    if z in (4.73,6.62): box('Entablature conductor',(0,-.10-depth/2-.015,z),(7.94,.025,.025),gold,.003)
for x in (-2.68,2.68):
    box('Header recessed panel',(x,-.61,5.61),(2.14,.06,1.38),dark,.014)
    box('Header relief face',(x,-.65,5.61),(1.95,.07,1.19),stone,.018)
    for dx in (-.85,.85): box('Panel fine inlay',(x+dx,-.689,5.61),(.018,.012,1.03),gold,.002)
for radius,width,mat in ((.68,.13,stone),(.555,.035,gold),(.455,.023,dark),(.29,.023,gold)):
    ring('Journal seal housing',0,-.72,5.61,radius,width,mat)
for i in range(16):
    a=i*math.tau/16
    o=box('Journal seal index',(math.sin(a)*.56,-.758,5.61+math.cos(a)*.56),(.02,.022,.06),gold,.002); o.rotation_euler[1]=a
box('Fitted intrados stone soffit',(0,-.10,4.35),(6,1.10,.30),stone,.01)
box('Intrados recessed channel',(0,-.661,4.26),(5.92,.035,.065),dark,.003)
box('Intrados conductor',(0,-.684,4.26),(5.88,.02,.018),gold,.002)
portal=export('SM_Aurelion_KIT_Portal_6x4p5',[8,1.4,7])
manifest[-1]['nominal_dimensions_m']=list(portal.dimensions)
manifest[-1]['aperture_m']=[6,4.2]
manifest[-1]['nominal_proxy_aperture_m']=[6,4.5]
manifest[-1]['collision']='Visual only; existing six-metre end-wall proxies and mission gate body retained'
portal.hide_render=True

# One-metre termination fits the remaining width without scaling a four-metre bay.
box('End return backing',(0,.26,3.5),(1,.3,7),stone,.007)
for j in range(7): box('Return ashlar',(0,-.02,.5+j),(.986,.55,.985),stone,.016)
for z,w,d,h in ((.1,1,.72,.2),(.29,1,.64,.12),(6.55,1,.78,.18),(6.86,1,.85,.28)):
    box('Return base or cornice',(0,0,z),(w,d,h),stone,.015)
for x in (-.32,.32):
    box('Return reveal',(x,-.305,3.4),(.06,.03,5.95),dark,.004)
    box('Return spine',(x,-.327,3.4),(.018,.02,5.85),gold,.002)
ret=export('SM_Aurelion_KIT_EndReturn_1x7',[1,.85,7]); manifest[-1]['nominal_dimensions_m']=list(ret.dimensions); ret.hide_render=True
manifest[-1]['collision']='Visual only; retained end-wall proxy'

# Both faces are finished: doors can be seen from adjoining campaign rooms.
# The existing gate toggles the entire visual; this does not claim animated retraction.
for side in (-1,1):
    x=side*1.509
    box('Door stone leaf core',(x,0,2.25),(3.018,.30,4.54),stone,.01)
    for face in (-1,1):
        for row in range(3):
            z=.75+row*1.5
            box('Door recessed field',(x,face*.157,z),(2.65,.03,1.30),dark,.009)
            box('Door carved inset',(x,face*.19,z),(2.49,.065,1.15),stone,.019)
            # Deep, asymmetric chevrons feed the central locking seam.
            pts=[(side*2.63,face*.232,z+.42),(side*1.72,face*.232,z+.42),(side*.68,face*.232,z-.34)]
            path('Door recessed chevron',pts,.11,.015,dark)
            path('Door conductor',[(px,py+face*.024,pz) for px,py,pz in pts],.025,.014,gold)
        for xx in (side*.12,side*2.87):
            box('Door vertical rail recess',(xx,face*.185,2.25),(.095,.07,4.3),dark,.004)
            box('Door brass guide',(xx,face*.23,2.25),(.026,.025,4.25),gold,.003)
        for z in (.11,4.39):
            box('Door stone cap',(x,face*.19,z),(2.98,.12,.18),stone,.014)
        for j in range(7):
            box('Locking seam tooth',(side*.07,face*.235,.48+j*.6),(.12,.08,.20),gold,.009)
door=export('SM_Aurelion_KIT_JournalDoor_6x4p5',[6.036,.55,4.54]); manifest[-1]['nominal_dimensions_m']=list(door.dimensions)
manifest[-1]['collision']='Visual only; journal gate Body remains authoritative'
door.hide_render=True

portal.hide_render=False; door.hide_render=False
scene.world=bpy.data.worlds.new('Portal studio'); scene.world.color=(.15,.15,.15)
for pos,energy,size in (((-5,-6,8),1800,6),((6,-3,4),1200,5),((0,4,8),1500,5)):
    d=bpy.data.lights.new('Portal review light','AREA'); d.energy=energy; d.size=size
    a=bpy.data.objects.new('Portal review light',d); scene.collection.objects.link(a); a.location=pos
    a.rotation_euler=(Vector((0,0,3.5))-a.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Portal review'); camera=bpy.data.objects.new('Portal review',d); scene.collection.objects.link(camera)
camera.location=(8,-13,6.5); camera.rotation_euler=(Vector((0,0,3.5))-camera.location).to_track_quat('-Z','Y').to_euler(); d.lens=44; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1600; scene.render.resolution_y=1200; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'Portal-assembly.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Fitted source candidate; in-engine end-wall review required',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_EndwallKit.blend'))
bpy.ops.render.render(write_still=True)
