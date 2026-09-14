"""Aurelion sealed stores: ivory ceramic panels on a mechanical dark chassis."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z08CargoKit';ROOT.mkdir(exist_ok=True)
old=json.loads((ROOT/'cargo-baseline.json').read_text())
w,d,h=[v/50 for v in old['mesh_extent']]
# Proportions adapt to the measured cargo mesh; details sit in real recesses.
box('Sealed inner chassis',(0,0,h*.48),(w*.90,d*.82,h*.83),dark,min(w,d,h)*.035)
for z,ww,dd,hh in [(h*.045,w,d,.09*h),(h*.12,.97*w,.95*d,.045*h),(h*.86,.97*w,.95*d,.04*h)]:
    box('Stacking rim',(0,0,z),(ww,dd,hh),dark,min(w,d,h)*.018)
for x in (-w*.45,w*.45):box('Lid side frame',(x,0,h*.94),(w*.10,d,h*.12),dark,.012)
for y in (-d*.435,d*.435):box('Lid cross frame',(0,y,h*.94),(w*.799,d*.13,h*.12),dark,.012)
box('Recessed lid field',(0,0,h*.990),(w*.79,d*.72,h*.012),stone,.007)
for x in (-w*.44,w*.44):
    for y in (-d*.43,d*.43):
        box('Load corner pillar',(x,y,h*.48),(w*.10,d*.12,h*.70),stone,.016)
        for z in (h*.17,h*.79):
            box('Corner impact collar',(x,y,z),(w*.12,d*.14,h*.045),dark,.008)
for side in (-1,1):
    # Separated panels make vertical gasket joints without coplanar decals.
    for i in range(3):
        x=(i-1)*w*.245
        box('Front ceramic panel',(x,side*d*.44,h*.49),(w*.229,d*.06,h*.59),stone,.012)
        for dx in (-w*.075,w*.075):
            box('Recess shadow moulding',(x+dx,side*d*.473,h*.49),(w*.017,d*.01,h*.44),dark,.003)
        if i!=1:
            for j in range(5):
                box('Lower breathing register',(x,side*d*.476,h*(.26+j*.026)),(w*.07,d*.012,h*.009),dark,.002)
    # Recessed handle with separate sockets and a genuinely open grip gap.
    box('Handle pocket',(0,side*d*.483,h*.58),(w*.20,d*.016,h*.11),dark,.008)
    for x in (-w*.072,w*.072):
        box('Handle mounting lug',(x,side*d*.494,h*.58),(w*.027,d*.012,h*.048),gold,.003)
    box('Carry handle',(0,side*d*.496,h*.59),(w*.13,d*.004,h*.022),dark,.0015)
    for x in (-w*.31,w*.31):
        box('Lid latch keeper',(x,side*d*.481,h*.827),(w*.064,d*.02,h*.105),dark,.005)
        box('Latch lever',(x,side*d*.493,h*.835),(w*.034,d*.012,h*.067),gold,.004)
        for z in (h*.785,h*.872):
            bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=min(w,h)*.006,depth=d*.004,location=(x,side*d*.498,z),rotation=(math.pi/2,0,0))
            finish(bpy.context.object,dark,.0007)
    # Small recessed rank / routing registers, not broad decorative gold.
    for i in range(3):box('Cargo routing register',((i-1)*w*.024,side*d*.476,h*.36),(w*.01,d*.012,h*(.025+i*.012)),gold,.001)
    for x in (-w*.37,w*.37):
        for z in (h*.22,h*.74):
            bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=min(w,h)*.007,depth=d*.006,location=(x,side*d*.478,z),rotation=(math.pi/2,0,0))
            finish(bpy.context.object,dark,.001)
    # End faces have a different ribbed service field and an inset connector.
    box('End ceramic field',(side*w*.46,0,h*.49),(w*.04,d*.68,h*.58),stone,.012)
    for y in (-d*.24,-d*.12,0,d*.12,d*.24):
        box('End reinforcement rib',(side*w*.487,y,h*.49),(w*.024,d*.029,h*.47),dark,.004)
    box('End data connector bed',(side*w*.494,0,h*.65),(w*.008,d*.19,h*.09),dark,.003)
    for y in (-d*.045,0,d*.045):box('Recessed connector contact',(side*w*.499,y,h*.65),(w*.002,d*.014,h*.033),gold,.001)
for x in (-w*.32,w*.32):
    for lo,hi in [(-.31,-.262),(-.238,.238),(.262,.31)]:
        box('Lid stacking runner',(x,(lo+hi)*d/2,h*.997),(w*.027,(hi-lo)*d,h*.006),dark,.001)
    for y in (-d*.25,d*.25):box('Lid retaining key',(x,y,h*.997),(w*.05,d*.022,h*.006),gold,.001)
# Preserve original off-centre asset pivot and every legacy instance transform.
shift=Vector((old['mesh_origin'][0]/100,-old['mesh_origin'][1]/100,(old['mesh_origin'][2]-old['mesh_extent'][2])/100))
for obj in parts:obj.location+=shift
obj=export('SM_Aurelion_KIT_Z08SealedStores',[w,d,h])
manifest[-1]['position_precision']=10
manifest[-1]['preserve_fallback_geometry']=True
manifest[-1]['collision']='None; replaces decorative cargo instances; native cover unchanged'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored cargo; engine fit and visual acceptance pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Cargo studio');scene.world.color=(.18,.18,.18)
target=shift+Vector((0,0,h*.5));size=max(w,d,h)
for delta,power in [((2,-3,4),1000),((-3,-1,2),700),((1,3,3),1200)]:
    bpy.ops.object.light_add(type='AREA',location=target+Vector(delta)*size);o=bpy.context.object;o.data.energy=power*size*size;o.data.size=size*3;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=target+Vector((2.8,-4,2))*size);camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=size*1.7;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'stores.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Crucible-Sealed-Stores.blend'));bpy.ops.render.render(write_still=True)
print('CRUCIBLE_CARGO_BUILD_PASS')
