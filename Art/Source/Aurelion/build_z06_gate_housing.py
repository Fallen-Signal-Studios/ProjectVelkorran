"""Fixed Aurelion lifting-gate housing around the measured rescue leaf travel."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z06GateHousingKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
# Local X is depth, Y is width; the origin is floor level beneath the moving body.
# The moving leaf occupies X +/- .22, Y +/- 2.15, Z .01 through 5.89.
for sign in (-1,1):
    box('Continuous guide jamb',(0,sign*2.375,3.125),(.78,.35,6.25),stone,.012)
    for z in (.12,1.28,2.52,3.76,5.00,6.10):
        for face in (-1,1):box('Guide bearing register',(face*.397,sign*2.375,z),(.014,.28,.12),stone,.003)
    for face in (-1,1):
        box('Guide conductor bed',(face*.396,sign*2.375,3.12),(.012,.075,5.66),grout,.002)
        box('Guide conductor',(face*.405,sign*2.375,3.12),(.008,.025,5.62),gold,.001)
    # Front/back pocket skins leave five centimetres beside the moving leaf.
    box('Upper leaf pocket skin',(sign*.33,0,4.675),(.12,4.40,3.05),stone,.008)
    for y in (-1.1,1.1):
        box('Pocket recessed field',(sign*.395,y,4.675),(.010,1.92,2.72),grout,.002)
        box('Pocket fitted panel',(sign*.407,y,4.675),(.014,1.82,2.62),stone,.003)
        for yy in (y-.78,y+.78):box('Panel vertical relief',(sign*.421,yy,4.675),(.014,.06,2.37),stone,.003)
        for z in (3.52,5.83):box('Panel cross relief',(sign*.421,y,z),(.014,1.50,.06),stone,.003)
        box('Pocket mechanism register',(sign*.43,y,4.675),(.008,.12,.58),gold,.001)
box('Upper load-bearing lintel',(0,0,6.175),(.78,4.40,.15),stone,.008)
housing=export('SM_Aurelion_KIT_Z06GateHousing',[.868,5.10,6.25])
bounds=[([-.39,-2.55,0],[.39,-2.20,6.25]),([-.39,2.20,0],[.39,2.55,6.25]),([-.39,-2.20,6.10],[.39,2.20,6.25]),([-.39,-2.20,3.15],[-.27,2.20,6.20]),([.27,-2.20,3.15],[.39,2.20,6.20])]
bpy.ops.object.select_all(action='DESELECT');housing.select_set(True)
for i,(lo,hi) in enumerate(bounds):
    bpy.ops.mesh.primitive_cube_add(size=1,location=tuple((a+b)/2 for a,b in zip(lo,hi)));h=bpy.context.object;h.name='UCX_SM_Aurelion_KIT_Z06GateHousing_'+str(i).zfill(2);h.dimensions=tuple(b-a for a,b in zip(lo,hi));bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);h.hide_render=True;h.display_type='WIRE'
# Creation changes selection; select the complete owned export explicitly.
bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_Aurelion_KIT_Z06GateHousing.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
manifest[-1].update(nominal_dimensions_m=list(housing.dimensions),convex_hulls=5,box_collision_bounds_m=bounds,moving_leaf_swept_bounds_m=[[-.22,-2.15,.01],[.22,2.15,5.89]],collision='Five separate guide/pocket hulls, leaving the full leaf travel volume open')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; Unreal fit and live transit acceptance pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Housing studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((7,-4,8),1800,6),((-5,4,7),1400,5)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((0,0,3))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(11,-8,7));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,3))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=8.5;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.render.resolution_x=1300;scene.render.resolution_y=1400;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'gate-housing.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z06-GateHousing.blend'));bpy.ops.render.render(write_still=True)
print('Z06_GATE_HOUSING_BUILD_PASS')
