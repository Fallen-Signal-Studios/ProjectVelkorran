"""Load-bearing refuge plinth and fitted stone ramp parapet."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z06RefugeFinishKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
box('Plinth structural core',(0,0,.30),(5.90,9.90,.60),grout,.003)
for z,h in ((.055,.11),(.55,.10)):
    box('Long bearing course',(0,-4.95,z),(6,.10,h),stone,.008)
    box('Long bearing course',(0,4.95,z),(6,.10,h),stone,.008)
    for side in (-1,1):box('Bearing return',(side*2.95,0,z),(.10,9.80,h),stone,.008)
for side in (-1,1):
    for i in range(5):
        box('Rusticated long-face ashlar',(side*2.965,-4+i*2,.31),(.07,1.968,.35),stone,.015)
    for i in range(3):
        box('Rusticated end ashlar',(-2+i*2,side*4.965,.31),(1.968,.07,.35),stone,.015)
plinth=export('SM_Aurelion_KIT_Z06RefugePlinth',[6,10,.6])
# A single bounded hull gives the new masonry an honest physical volume.
bpy.ops.object.select_all(action='DESELECT')
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,.3));hull=bpy.context.object;hull.name='UCX_SM_Aurelion_KIT_Z06RefugePlinth_00';hull.dimensions=(6,10,.6)
bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);hull.hide_render=True;hull.display_type='WIRE';plinth.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_Aurelion_KIT_Z06RefugePlinth.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
manifest[-1].update(convex_hulls=1,solid_bounds_m=[[-3,-5,0],[3,5,.6]],collision='Single solid plinth hull; new collision requires Unreal passage validation')

length=4.123106;inside=length-.28
box('Parapet footing',(0,0,-.56),(inside,.22,.18),stone,.008)
box('Parapet core',(0,0,0),(inside,.13,.94),stone,.008)
box('Upper shadow course',(0,0,.485),(inside,.17,.03),grout,.003)
box('Hand-rest cornice',(0,0,.575),(inside,.22,.15),stone,.009)
for side in (-1,1):box('Terminal bearing pier',(side*(length/2-.07),0,0),(.14,.22,1.3),stone,.008)
panel=inside/3
for side in (-1,1):
    for x in (-panel,0,panel):
        box('Recessed panel bed',(x,side*.069,0),(panel-.07,.014,.75),grout,.003)
        box('Dressed panel face',(x,side*.081,0),(panel-.15,.012,.66),stone,.004)
        for dx in (-panel/2+.16,panel/2-.16):box('Carved panel border',(x+dx,side*.091,0),(.026,.012,.55),stone,.002)
        for z in (-.26,.26):box('Panel cross border',(x,side*.091,z),(panel-.30,.012,.026),stone,.002)
        box('Recessed conductor',(x,side*.104,0),(.014,.006,.30),gold,0)
    box('Cornice inlay',(0,side*.107,.54),(inside-.02,.006,.018),gold,0)
guard=export('SM_Aurelion_KIT_Z06RefugeGuard',[length,.22,1.3]);manifest[-1]['collision']='None; exact retained ramp guard envelope'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; in-engine fit and final art acceptance pending',modules=manifest),indent=2))
plinth.location.x=-3.5;hull.location.x=-3.5;guard.location=(3.5,0,.65)
scene.world=bpy.data.worlds.new('Refuge finish studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((0,-7,10),1800,7),((-7,3,7),1300,6)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(13,-17,12));camera=bpy.context.object;camera.rotation_euler=(Vector((-.7,0,.2))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=16;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.render.resolution_x=1600;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'refuge-finish.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z06-RefugeFinish.blend'));bpy.ops.render.render(write_still=True)
print('Z06_REFUGE_FINISH_BUILD_PASS')
