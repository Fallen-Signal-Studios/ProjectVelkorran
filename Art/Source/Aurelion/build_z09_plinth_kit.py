"""Grounded split basalt plinth, preserving the six-metre central doorway."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z09PlinthKit';ROOT.mkdir(exist_ok=True)
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)
for side in (-1,1):
    x=side*5.5
    for z,h,d in ((.06,.12,.30),(.17,.10,.27),(.70,.10,.27),(.775,.05,.30)):
        box('Dressed basalt molding',(x,-d/2,z),(4.992,d,h),basalt,.008)
    for i in range(5):
        for row in range(2):
            box('Jointed plinth stone',(x-2+(i),-.12,.335+row*.22),(.988,.24,.208),basalt,.006)
    # Broad metallic boundary above the plinth; backed by stone and flush below
    # the cap. Does not introduce a route cue across the opening.
    box('Functional gold cap register',(x,-.273,.715),(4.90,.006,.025),gold,.002)
    for px in (x-2.43,x+2.43):
        box('Ivory return arris',(px,-.253,.435),(.11,.034,.40),stone,.004)
o=export('SM_Aurelion_KIT_Z09SplitPlinth',[16,.30,.80]);manifest[-1].update(nominal_dimensions_m=list(o.dimensions),position_precision=10,preserve_fallback_geometry=True,collision='None; native end-wall collision retained',central_opening_m=6.008)
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=[dict(actor='Z09__BlackLowerBand_01',location_cm=[0,25375,-1500],yaw=0),dict(actor='Z09__BlackLowerBand_02',location_cm=[0,30825,-1500],yaw=180)]),indent=2))
scene.world=bpy.data.worlds.new('Split plinth studio');scene.world.color=(.18,.18,.18)
for pos in ((0,-5,7),(-7,0,5),(7,0,5)):
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=2400;a.data.size=6;a.rotation_euler=(-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(10,-16,8));a=bpy.context.object;a.rotation_euler=(-a.location).to_track_quat('-Z','Y').to_euler();a.data.type='ORTHO';a.data.ortho_scale=18;scene.camera=a
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=800;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'plinth.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z09-SplitPlinth.blend'));bpy.ops.render.render(write_still=True)
