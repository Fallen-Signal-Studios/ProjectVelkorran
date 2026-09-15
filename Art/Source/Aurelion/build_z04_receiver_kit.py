"""Human-scale Aurelion receiver terminal with native-driven status apertures."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z04ReceiverKit';ROOT.mkdir(exist_ok=True)
base_box=box
def box(name,loc,size,mat=stone,bevel=.008):return base_box(name,loc,size,mat,min(bevel,min(size)*.2))
status=material('M_Aurelion_ReceiverStatus',(.08,.65,.75),0,.28)
box('Floor bearing shoe',(0,0,.035),(.9,.6,.07),dark,.009)
box('Cut stone plinth',(0,0,.085),(.86,.56,.03),stone,.004)
box('Sealed service core',(0,.025,.655),(.72,.44,1.11),dark,.01)
for side in (-1,1):
    box('Stone side cheek',(side*.389,0,.66),(.082,.50,1.12),stone,.009)
    box('Side inset service field',(side*.434,0,.62),(.008,.37,.74),dark,.001)
    for y in (-.13,0,.13):box('Side cooling fin',(side*.441,y,.58),(.012,.018,.51),stone,.002)
    for z in (.16,1.15):box('Corner collar',(side*.387,0,z),(.095,.52,.05),dark,.006)
box('Crown seam',(0,0,1.235),(.84,.55,.045),dark,.006)
box('Stone crown',(0,0,1.28),(.90,.60,.06),stone,.007)
box('Front control surround',(0,-.24,.96),(.68,.085,.50),stone,.009)
box('Recessed display',(0,-.284,.99),(.59,.012,.40),dark,.002)
for x in (-.298,.298):box('Display edge conductor',(x,-.292,.99),(.006,.004,.37),gold,.0005)
# Status bars and bus aperture share the native disabled-state material slot.
box('Signal bus',(-.24,-.294,1.005),(.008,.004,.30),status,.0005)
for i in range(5):
    z=1.13-i*.052
    box('Receiver channel',(-.025,-.294,z),(.37-i*.035,.004,.008),status,.0005)
    box('Channel terminal',(.245,-.294,z),(.017,.004,.017),gold,.0005)
box('Isolation switch bed',(0,-.267,.70),(.34,.044,.11),dark,.006)
for x in (-.125,.125):box('Switch guard',(x,-.294,.70),(.035,.012,.09),stone,.002)
box('Physical isolation lever',(0,-.296,.70),(.16,.008,.024),gold,.001)
box('Lower access cover',(0,-.211,.37),(.60,.018,.40),stone,.005)
for x in (-.21,.21):
    for z in (.22,.51):
        bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=.009,depth=.005,location=(x,-.223,z),rotation=(math.pi/2,0,0));finish(bpy.context.object,dark,.0006)
for i in range(7):box('Access breathing register',(0,-.224,.30+i*.023),(.22,.004,.007),dark,.0005)
box('Rear service plate',(0,.252,.67),(.59,.012,.90),stone,.005)
for x in (-.20,.20):
    for z in (.29,.99):box('Rear captive fastener',(x,.261,z),(.012,.006,.012),gold,.0008)
for i in range(3):box('Rear cable socket',((i-1)*.12,.267,.48),(.065,.018,.11),dark,.003)
o=export('SM_Aurelion_KIT_Z04Receiver',[.9,.6,1.31])
manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='None; native receiver Body remains unchanged')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,native_status_primitive_data_index=0),indent=2))
scene.world=bpy.data.worlds.new('Receiver studio');scene.world.color=(.18,.18,.18)
target=Vector((0,0,.66))
for pos,power in [((2,-3,4),500),((-3,-1,2),300),((1,3,3),700)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=3;a.rotation_euler=(target-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2,-3,2));camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=1.75;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1000;scene.render.resolution_y=1300;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'receiver.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z04-Receiver.blend'));bpy.ops.render.render(write_still=True)
