"""Aurelion aid cache: floor-standing visual fitted around the retained native body."""
from pathlib import Path
base=Path(__file__).resolve().parent
exec(compile((base/'build_architecture_kit.py').read_text().split('# Four metre bay:')[0],'kit_helpers','exec'),globals())
ROOT=base/'MedicalCacheKit';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.45,.65,.68),.1,.25)
raw_box=box
def box(name,loc,size,mat=stone,bevel=.006):return raw_box(name,loc,size,mat,min(bevel,min(size)*.2))
for x in (-.33,.33):
    for y in (-.42,.42):
        box('Floor isolation pad',(x,y,.018),(.12,.14,.036),dark,.008)
        box('Recessed support foot',(x,y,.097),(.10,.12,.122),stone,.008)
box('Protective base tray',(0,0,.19),(.8,1,.08),stone,.012)
box('Sealed supply housing',(0,0,.595),(.716,.916,.73),dark,.015)
for x in (-.375,.375):
    for y in (-.46,.46):
        box('Armored corner rail',(x,y,.596),(.05,.08,.72),stone,.008)
        for z in (.29,.87):box('Rail collar',(x,y,z),(.055,.084,.06),gold,.004)
for face in (-1,1):
    for y in (-.235,.235):
        box('Side ceramic panel',(face*.367,y,.60),(.034,.36,.60),stone,.008)
        for yy in (y-.16,y+.16):
            for z in (.35,.84):
                box('Side pin socket',(face*.387,yy,z),(.005,.022,.022),dark,.001)
                box('Side retaining pin',(face*.391,yy,z),(.003,.008,.008),gold,.0005)
    box('Side carry recess',(face*.388,0,.70),(.009,.24,.075),dark,.003)
    for y in (-.105,.105):box('Carry handle standoff',(face*.395,y,.70),(.010,.025,.042),gold,.001)
    box('Carry handle',(face*.397,0,.716),(.004,.19,.014),dark,.0006)
# Three individually sealed aid drawers on the approach-facing side.
for i,z in enumerate((.35,.56,.77)):
    box('Aid cartridge seal',(0,-.466,z),(.63,.02,.199),dark,.006)
    box('Aid cartridge ceramic',(0,-.48,z),(.594,.018,.174),stone,.006)
    box('Cartridge pull pocket',(0,-.491,z),(.23,.006,.045),dark,.002)
    box('Cartridge release grip',(0,-.496,z+.009),(.17,.008,.014),gold,.001)
    for j in range(i+1):box('Cartridge indexing light',(-.245+j*.024,-.492,z+.047),(.012,.006,.012),lens,.001)
    for x in (-.264,.264):
        box('Cartridge service fastener',(x,-.491,z-.059),(.01,.006,.01),dark,.001)
box('Rear maintenance panel',(0,.466,.59),(.63,.025,.59),stone,.008)
for x in (-.245,.245):
    for z in (.36,.80):box('Rear fastener',(x,.482,z),(.018,.008,.018),dark,.002)
for z in (.45,.49,.53,.57,.61):box('Rear ventilation register',(0,.482,z),(.35,.008,.015),dark,.001)
for x in (-.25,.25):
    box('Lid hinge',(x,.48,.935),(.095,.036,.055),dark,.005)
    box('Hinge pin',(x,.492,.935),(.073,.01,.015),gold,.001)
box('Lid gasket',(0,0,.963),(.76,.96,.016),dark,.003)
box('Ceramic protective lid',(0,0,.992),(.8,1,.042),stone,.008)
box('Lid instrument recess',(0,0,1.016),(.58,.71,.008),dark,.004)
for x in (-.185,0,.185):
    box('Sealed top supply cassette',(x,.055,1.031),(.159,.46,.03),stone,.004)
    box('Cassette identification',(x,.055,1.047),(.012,.31,.002),gold,.0003)
    box('Cassette release socket',(x,-.135,1.048),(.065,.038,.004),dark,.0005)
for x in (-.20,-.10,0,.10,.20):box('Aid availability register',(x,-.285,1.023),(.054,.024,.006),lens,.001)
o=export('SM_Aurelion_KIT_MedicalAidCache',[.8,1,1.05]);manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='No visual collision; native Body box and interaction retained',placement=dict(location_cm=[-3050,22600,-1200],yaw_degrees=180))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source authored; native cache presentation replacement pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Aid case studio');scene.world.color=(.15,.15,.15)
for pos,power in [((2,-3,4),850),((-2,-1,2),500),((1,2,3),750)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=3;a.rotation_euler=(Vector((0,0,.5))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2,-3,2.4));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.53))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=1.65;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'medical-aid-cache.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Medical-Aid-Cache.blend'));bpy.ops.render.render(write_still=True)
print('MEDICAL_CACHE_SOURCE_PASS')
