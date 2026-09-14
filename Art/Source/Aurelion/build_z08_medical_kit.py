"""Human-scale Aurelion treatment trolley with articulated support hardware."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z08MedicalKit';ROOT.mkdir(exist_ok=True)
old=json.loads((ROOT/'bed-baseline.json').read_text());scale=old['instances'][0]['scale']
assert all(max(abs(a-b) for a,b in zip(r['scale'],scale))<1e-6 for r in old['instances'])
w,d,h=[v*s/50 for v,s in zip(old['mesh_extent'],scale)]
cloth=material('M_Aurelion_MedicalUpholstery',(.035,.075,.072),0,.82)
def rod(name,a,b,r,mat=dark):
    a=Vector(a);b=Vector(b);bpy.ops.mesh.primitive_cylinder_add(vertices=24,radius=r,depth=(b-a).length,location=(a+b)/2)
    o=bpy.context.object;o.name=name;o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();return finish(o,mat,min(r*.15,.002,(b-a).length*.2))
# Four independent caster forks with hubs, brake pedals and swivel stems.
radius=h*.075
for x in (-w*.35,w*.35):
    for y in (-d*.34,d*.34):
        rod('Caster resilient tyre',(x-.025,y,radius),(x+.025,y,radius),radius,cloth)
        rod('Caster axle',(x-.04,y,radius),(x+.04,y,radius),radius*.40,dark)
        for side in (-1,1):
            box('Caster fork',(x+side*.034,y,h*.115),(.015,.035,h*.13),stone,.004)
            rod('Axle cap',(x+side*.04,y,radius),(x+side*.043,y,radius),radius*.24,gold)
        box('Caster swivel housing',(x,y,h*.20),(.09,.10,.038),dark,.008)
        rod('Swivel stem',(x,y,h*.21),(x,y,h*.29),.017,stone)
        box('Wheel brake pedal',(x,y-d*.07,h*.115),(.06,.05,.018),dark,.004)
# Open undercarriage, crossed braces and service tray.
for y in (-d*.31,d*.31):box('Lower chassis runner',(0,y,h*.29),(w*.78,.045,.05),dark,.009)
for x in (-w*.35,w*.35):box('Chassis cross member',(x,0,h*.29+.026),(.055,d*.73,.05),stone,.009)
box('Underslung equipment tray',(0,0,h*.32),(w*.37,d*.57,.032),stone,.008)
for side in (-1,1):
    y=side*d*.24
    rod('Crossed lift actuator',(-w*.26,y,h*.30),(w*.23,y,h*.50),.020,stone)
    rod('Crossed lift cylinder',(w*.26,y,h*.30),(-w*.08,y,h*.44),.026,dark)
    rod('Exposed lift piston',(-w*.08,y,h*.44),(-w*.23,y,h*.50),.012,gold)
    rod('Lift pivot pin',(0,y-.03,h*.40),(0,y+.03,h*.40),.027,dark)
# Dressed ceramic deck with a resilient segmented patient surface.
box('Patient platform',(0,0,h*.52),(w*.89,d*.83,h*.075),stone,.015)
box('Mattress support gasket',(0,0,h*.565),(w*.84,d*.78,h*.022),dark,.006)
for i,(x,length) in enumerate([(-w*.285,w*.23),(-w*.047,w*.23),(w*.194,w*.23),(w*.357,w*.075)]):
    box('Upholstered support segment',(x,0,h*.625),(length,d*.73,h*.095),cloth,.022)
    for side in (-1,1):
        box('Mattress sewn edge',(x,side*d*.366,h*.626),(length-.022,.003,h*.052),cloth,.001)
box('Contoured head cushion',(-w*.29,0,h*.711),(w*.18,d*.57,h*.08),cloth,.025)
# Lifting rails and folding hinges; open spaces remain between posts.
for side in (-1,1):
    y=side*d*.475
    rod('Side safety handrail',(-w*.27,y,h*.84),(w*.26,y,h*.84),d*.025,dark)
    for x in (-w*.25,w*.24):
        rod('Folding rail upright',(x,y,h*.55),(x,y,h*.84),.013,stone)
        rod('Rail hinge cap',(x,side*d*.455,h*.555),(x,side*d*.495,h*.555),.028,dark)
        box('Hinge release key',(x,side*d*.492,h*.555),(.024,d*.012,.012),gold,.001)
    rod('Lower side guard',(-w*.25,y,h*.68),(w*.24,y,h*.68),.009,stone)
    for x in (-w*.10,w*.09):rod('Guard infill',(x,y,h*.68),(x,y,h*.84),.007,stone)
    box('Side service module',(w*.05,side*d*.448,h*.535),(.16,.055,.055),dark,.007)
    for i in range(3):box('Service status key',(w*.01+i*.029,side*d*.483,h*.535),(.014,.004,.018),gold,.001)
# Head and foot end frames; handles, recessed service fields and floor-safe bumpers.
for side in (-1,1):
    x=side*w*.49;top=h if side<0 else h*.79
    for y in (-d*.36,d*.36):
        box('Endboard upright',(x,y,(h*.49+top-h*.05)/2),(w*.02,.045,top-h*.05-h*.49),stone,.007)
    box('End push grip',(x,0,top-h*.025),(w*.02,d*.77,h*.05),dark,.009)
    box('End service panel',(x,0,h*.58),(w*.012,d*.60,h*.10),stone,.008)
    box('End recessed strip',(x+side*w*.008,0,h*.58),(w*.003,d*.36,h*.025),dark,.001)
    for y in (-d*.34,d*.34):box('Impact bumper',(side*w*.473,y,h*.48),(w*.049,.065,.065),dark,.012)
    for y in (-d*.05,0,d*.05):box('End routing register',(x+side*w*.0095,y,h*.58),(w*.001,.012,.012),gold,.0005)
obj=export('SM_Aurelion_KIT_Z08TreatmentTrolley',[w,d,h])
manifest[-1]['position_precision']=10;manifest[-1]['preserve_fallback_geometry']=True
manifest[-1]['collision']='None; decorative medical furnishing, native evacuation geometry unchanged'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored true-size trolley; engine fit and final visual acceptance pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Treatment trolley studio');scene.world.color=(.18,.18,.18)
target=Vector((0,0,h*.5));size=w
for delta,power in [((2,-3,4),1000),((-3,-1,2),700),((1,3,3),1200)]:
    bpy.ops.object.light_add(type='AREA',location=target+Vector(delta)*size);o=bpy.context.object;o.data.energy=power*size*size;o.data.size=size*3;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=target+Vector((2.8,-4,2))*size);camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=size*1.3;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1500;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'trolley.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Treatment-Trolley.blend'));bpy.ops.render.render(write_still=True)
print('TREATMENT_TROLLEY_BUILD_PASS')
