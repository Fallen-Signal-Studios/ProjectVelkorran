"""Fitted fallen masonry section: dressed courses, layered cornice and fitted stone cap."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z06FallenSlabKit';ROOT.mkdir(exist_ok=True)
# Continuous backing prevents holes through the obstacle; separate dressed courses expose mortar relief.
box('Structural stone core',(0,0,1.16),(9.92,6.92,2.32),stone,.025)
for z,h,inset in [(.14,.28,0),(.35,.14,.035),(2.06,.16,.025),(2.22,.12,0)]:
    box('Long cornice',(0,-3.44+inset,z),(10,.12,h),stone,.018)
    box('Long cornice',(0,3.44-inset,z),(10,.12,h),stone,.018)
    for x in (-4.94+inset,4.94-inset):box('Cornice return',(x,0,z),(.12,6.76,h),stone,.018)
for row in range(2):
    z=.46+row*.75+.36
    for sign in (-1,1):
        for i in range(5):
            x=-4+i*2
            box('Dressed long-face ashlar',(x,sign*3.44,z),(1.968,.08,.718),stone,.021)
            box('Inset face panel',(x,sign*3.475,z),(1.64,.04,.47),stone,.006)
        for i in range(4):
            y=-2.625+i*1.75
            box('Dressed end-face ashlar',(sign*4.96,y,z),(.08,1.718,.718),stone,.021)
    # Conductors are recessed below the outermost coping envelope.
for sign in (-1,1):
    box('Long conductor bed',(0,sign*3.475,1.995),(9.80,.025,.075),dark,.006)
    box('Long conductor',(0,sign*3.492,1.995),(9.76,.012,.030),gold,.003)
    box('Return conductor bed',(sign*4.975,0,1.995),(.025,6.72,.075),dark,.006)
    box('Return conductor',(sign*4.992,0,1.995),(.012,6.68,.030),gold,.003)
# Six large capstones keep the physical top plane; narrow joints stay within six millimetres.
box('Cap joint backing',(0,0,2.367),(10,7,.054),dark,.001)
for x in (-10/3,0,10/3):
    for y in (-1.75,1.75):box('Heavy capstone',(x,y,2.355),(10/3-.024,3.476,.09),stone,.012)
# Faceted spalling affects only the outer four centimetres of selected face/cornice edges.
# The cap plane and structural core remain intact; no rubble is placed in the rescue lanes.
damage=[]
for sign in (-1,1):
    for i,(x,z) in enumerate([(-4.6,.31),(-2.9,1.18),(-1.3,2.18),(.8,.44),(2.4,1.94),(4.7,2.13)]):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=(x,sign*3.54,z));cutter=bpy.context.object;cutter.name='Spall cutter';cutter.scale=(.23+.025*(i%3),.08,.12)
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        bpy.context.view_layer.update()
        for target in list(parts):
            points=[target.matrix_world@Vector(v) for v in target.bound_box]
            if not (min(v.x for v in points)<x+.30 and max(v.x for v in points)>x-.30 and min(v.z for v in points)<z+.12 and max(v.z for v in points)>z-.12):continue
            bpy.context.view_layer.objects.active=target
            mod=target.modifiers.new('Localized impact spall','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter
            bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.data.objects.remove(cutter,do_unlink=True);damage.append(dict(x=x,y=sign*3.54,z=z,max_depth_m=.04))
o=export('SM_Aurelion_KIT_Z06FallenMasonry',[10,7,2.4]);manifest[-1]['nominal_dimensions_m']=list(o.dimensions);manifest[-1]['collision']='None; fitted to the retained 10 x 7 x 2.4 m physical slab, pending in-engine verification'
manifest[-1]['localized_spalls']=damage
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; Unreal fit and visual acceptance pending',modules=manifest),indent=2))
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.world=bpy.data.worlds.new('Slab studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((2,-7,10),1800,7),((-7,3,7),1300,6)]:
    bpy.ops.object.light_add(type='AREA',location=pos);l=bpy.context.object;l.data.energy=power;l.data.size=size;l.rotation_euler=(Vector((0,0,1))-l.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(12,-15,10));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,1))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=14;scene.camera=camera
scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'fallen-masonry.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z06-FallenMasonry.blend'));bpy.ops.render.render(write_still=True)
print('Z06_FALLEN_MASONRY_BUILD_PASS')
