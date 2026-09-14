"""True-size Aurelion medical sideboard: drawers, supplies and instrument surface."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z08CabinetKit';ROOT.mkdir(exist_ok=True)
old=json.loads((ROOT/'cabinet-baseline.json').read_text());scale=old['instances'][0]['scale'];w,d,h=[v*s/50 for v,s in zip(old['mesh_extent'],scale)]
cloth=material('M_Aurelion_MedicalUpholstery',(.035,.075,.072),0,.82)
raw_box=box
def box(name,loc,size,mat=stone,bevel=.008):return raw_box(name,loc,size,mat,min(bevel,min(size)*.2))
# Separate feet, recessed chassis and corner protection.
for x in (-w*.42,w*.42):
    for y in (-d*.39,d*.39):
        box('Isolating foot',(x,y,h*.025),(.095,.095,h*.05),dark,.008)
        box('Foot housing',(x,y,h*.070),(.10,.10,h*.055),stone,.008)
box('Lower protective plinth',(0,0,h*.115),(w,d*.96,h*.09),dark,.012)
box('Sealed chassis',(0,0,h*.50),(w*.89,d*.87,h*.70),dark,.016)
for x in (-w*.47,w*.47):
    for y in (-d*.425,d*.425):box('Corner stile',(x,y,h*.51),(w*.06,d*.10,h*.70),stone,.009)
for side in (-1,1):
    box('Side ceramic field',(side*w*.462,0,h*.51),(w*.036,d*.70,h*.59),stone,.010)
    for j in range(6):box('Service ventilation',(side*w*.484,d*(-.14+j*.055),h*.30),(w*.008,d*.026,h*.055),dark,.002)
    for z in (h*.25,h*.73):
        for y in (-d*.27,d*.27):
            bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=.006,depth=.004,location=(side*w*.487,y,z),rotation=(0,math.pi/2,0));finish(bpy.context.object,dark,.0007)
# Front comprises three individually gasketed drawers and a supplies door.
for i in range(3):
    z=h*(.275+i*.20);x=-w*.205
    box('Drawer ceramic face',(x,-d*.451,z),(w*.435,d*.04,h*.184),stone,.008)
    box('Drawer handle pocket',(x,-d*.478,z+h*.025),(w*.21,d*.012,h*.060),dark,.005)
    for dx in (-w*.085,w*.085):box('Handle socket',(x+dx,-d*.489,z+h*.025),(.020,.016,.026),gold,.003)
    box('Drawer pull',(x,-d*.491,z+h*.034),(w*.17,.008,.013),dark,.0015)
    for j in range(i+1):box('Drawer inventory register',(x-w*.16+j*.018,-d*.475,z-h*.035),(.009,.005,.012),gold,.001)
box('Supply door',(w*.24,-d*.451,h*.475),(w*.38,d*.04,h*.582),stone,.010)
box('Door grip recess',(w*.11,-d*.478,h*.53),(.048,.012,.14),dark,.004)
for z in (h*.47,h*.59):box('Door grip socket',(w*.11,-d*.489,z),(.030,.016,.020),gold,.002)
box('Door grip',(w*.11,-d*.491,h*.53),(.014,.008,h*.12),dark,.0015)
for z in (h*.285,h*.665):
    box('Door hinge body',(w*.414,-d*.477,z),(.019,.014,.055),dark,.002)
    for dz in (-.016,.016):
        bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.003,depth=.002,location=(w*.414,-d*.486,z+dz),rotation=(math.pi/2,0,0));finish(bpy.context.object,gold,.0003)
for j in range(4):box('Supply count marker',(w*.28+j*.018,-d*.475,h*.36),(.008,.005,.014+j*.005),gold,.001)
# Rear maintenance hatch is distinct from the user-facing storage.
box('Rear service hatch',(0,d*.451,h*.50),(w*.77,.035,h*.56),stone,.012)
for x in (-w*.30,w*.30):
    for z in (h*.29,h*.70):
        bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=.006,depth=.003,location=(x,d*.471,z),rotation=(math.pi/2,0,0));finish(bpy.context.object,dark,.0005)
for j in range(7):box('Rear thermal louvre',(0,d*.473,h*(.40+j*.029)),(w*.40,.010,.008),dark,.001)
# Counter has an inset work mat and raised perimeter; no coplanar overlays.
box('Counter support',(0,0,h*.895),(w*.98,d*.98,h*.075),dark,.012)
box('Ceramic worktop',(0,0,h*.94),(w,d,h*.050),stone,.014)
for x in (-w*.48,w*.48):box('Counter side rim',(x,0,h*.9825),(w*.04,d,h*.035),stone,.006)
box('Rear counter rim',(0,d*.48,h*.9825),(w*.919,d*.04,h*.035),stone,.006)
box('Instrument mat',(w*.22,0,h*.972),(w*.34,d*.64,h*.012),cloth,.004)
for y in (-d*.28,d*.28):box('Mat locator',(w*.22,y,h*.978+.001),(w*.29,.006,.003),gold,.0005)
for x in (-w*.32,-w*.12):
    box('Instrument dock backing',(x,-d*.08,h*.973),(w*.14,d*.43,h*.016),dark,.005)
    box('Instrument dock pad',(x,-d*.08,h*.985),(w*.10,d*.37,h*.008),cloth,.002)
    box('Dock index',(x,-d*.30,h*.970),(w*.065,.012,.003),gold,.0005)
obj=export('SM_Aurelion_KIT_Z08MedicalSideboard',[w,d,h]);manifest[-1]['position_precision']=10;manifest[-1]['preserve_fallback_geometry']=True
manifest[-1]['collision']='One engine-authored bounds box added after import; source FBX has no collision hulls'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored medical sideboard; engine fit and final visual review pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Medical sideboard studio');scene.world.color=(.18,.18,.18);target=Vector((0,0,h*.5));size=max(w,d,h)
for delta,power in [((2,-3,4),1000),((-3,-1,2),700),((1,3,3),1200)]:
    bpy.ops.object.light_add(type='AREA',location=target+Vector(delta)*size);o=bpy.context.object;o.data.energy=power*size*size;o.data.size=size*3;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=target+Vector((2.8,-4,2.5))*size);camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=size*1.6;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'sideboard.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Medical-Sideboard.blend'));bpy.ops.render.render(write_still=True)
print('MEDICAL_SIDEBOARD_BUILD_PASS')
