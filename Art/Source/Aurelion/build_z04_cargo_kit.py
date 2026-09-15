"""Nine complete relay cover assemblies aligned to native obstruction owners."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
base_box=box
def box(name,loc,size,mat=stone,bevel=.008):
    return base_box(name,loc,size,mat,min(bevel,min(size)*.2))
ROOT=Path(__file__).resolve().parent/'Z04CargoKit';ROOT.mkdir(exist_ok=True)
recipe=Path(__file__).with_name('build_z08_cargo_kit.py').read_text().split('# Proportions adapt',1)[1].split('# Preserve original',1)[0]
recipe='# Proportions adapt'+recipe
def stores(name,w,d,h):
    exec(compile(recipe,'relay_stores_recipe','exec'),globals(),dict(w=w,d=d,h=h))
    o=export(name,[w,d,h]);o.hide_render=True
    manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='None; one assembly per retained native cover')
    return o
low=stores('SM_Aurelion_KIT_Z04LowStores',3,3,1.2)
wide=stores('SM_Aurelion_KIT_Z04WideStores',3,2,1.2)
high=stores('SM_Aurelion_KIT_Z04HighStores',3,3,2.2)
# These two plinths finish the previously exposed cover sides under the stone caps.
balcony=stores('SM_Aurelion_KIT_Z04BalconyStores',3,2,1.125)
foot=stores('SM_Aurelion_KIT_Z04FootStores',3,3,2.125)
specs=[('LC_SouthApproach',low,[6950,-12150,0],[0,1]),('LC_Southeast',low,[7850,-11850,0],[2,3]),
    ('LC_NorthMid',low,[6550,-10250,0],[4,5]),('LC_Northwest',wide,[5450,-9500,0],[6]),
    ('HC_SouthFlank',high,[8650,-11950,0],[7,8]),('HC_Central',high,[6450,-11350,0],[9,10]),
    ('HC_West',high,[5150,-10850,0],[11,12]),('LC_Balcony',balcony,[9050,-10300,300],[]),
    ('HC_BalconyFoot',foot,[8350,-10800,0],[])]
placements=[dict(asset=o.name,location_cm=p,yaw=0,native_actor='Z04_'+label,original_indices=indices) for label,o,p,indices in specs]
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
low.hide_render=False;high.hide_render=False;high.location.x=3.8
scene.world=bpy.data.worlds.new('Relay stores studio');scene.world.color=(.18,.18,.18)
target=Vector((1.7,0,.9))
for pos,power in [((2,-6,7),2200),((-5,1,5),1800),((6,4,6),2600)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=5;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(10,-14,9));camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=10;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'stores-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z04-Stores.blend'));bpy.ops.render.render(write_still=True)
