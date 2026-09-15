"""Relay rail runs with upright posts and butt-fitted balcony corners."""
from pathlib import Path
helper=Path(__file__).with_name('build_z03_rail_kit.py')
code=helper.read_text().split("slope=rail(")[0]
# Seat capitals and infill into the grip instead of leaving the old 2.5cm gap.
code=code.replace('(1.21,.12,d*.86,.05)','(1.21,.12,d*.86,.11)')
code=code.replace('floor+.71),(.019,.03,1.06)','floor+.72),(.019,.03,1.12)')
code=code.replace('        for z,ww,dd,hh in ((.025,.16,d,.05),',
    "        beam('Fitted foot shoe',.16,0,d,0,.05,rise*.16/w,stone)\n"
    "        parts[-1].location.x+=x;parts[-1].location.z+=floor-rise*.08/w\n"
    '        for z,ww,dd,hh in (')
# Corner feet stop 8cm short of perpendicular runs; a narrow grip tongue reaches
# the perpendicular grip face at 4.96cm without overlapping its post bases.
code=code.replace('    o=export(name,[w,d,h+rise]);',
    "    ends=(-1,1) if name.endswith('EastRail') else (-1,) if name.endswith('WestRail') else (1,) if name.endswith('ReturnRail') else ()\n"
    "    for side in ends:box('Corner grip tongue',(side*(w/2+.0152),0,h-.02),(.0304,d*.62,.04),dark,.001)\n"
    '    o=export(name,[w,d,h+rise]);')
# Level balcony guards retain their original 110 cm height; ramps retain 130 cm.
code=code.replace("    o=export(name,[w,d,h+rise]);", "    if rise==0:\n        for part in parts:\n            part.location.z*=1.1/1.3;part.scale.z*=1.1/1.3\n        h=1.1\n    o=export(name,[w,d,h+rise]);")
exec(compile(code,str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z04RailKit';ROOT.mkdir(exist_ok=True)
slope=rail('SM_Aurelion_KIT_Z04SlopeRail',12,.22,3,6)
east=rail('SM_Aurelion_KIT_Z04EastRail',10.84,.16,0,6)
north=rail('SM_Aurelion_KIT_Z04NorthRail',14,.16,0,7)
south=rail('SM_Aurelion_KIT_Z04SouthRail',7,.16,0,4)
corner=rail('SM_Aurelion_KIT_Z04CornerRail',1,.16,0,1)
west=rail('SM_Aurelion_KIT_Z04WestRail',3.92,.16,0,2)
return_rail=rail('SM_Aurelion_KIT_Z04ReturnRail',.92,.16,0,1)
placements=[dict(asset=slope.name,location_cm=[9588,-11200,0],yaw=90,original_indices=list(range(0,6))),
    dict(asset=slope.name,location_cm=[9012,-11200,0],yaw=90,original_indices=list(range(6,12))),
    dict(asset=slope.name,location_cm=[7700,-10188,0],yaw=0,original_indices=list(range(12,18))),
    dict(asset=slope.name,location_cm=[7700,-9612,0],yaw=0,original_indices=list(range(18,24))),
    dict(asset=east.name,location_cm=[9700,-10050,300],yaw=90,original_indices=[24,25,26]),
    dict(asset=north.name,location_cm=[9000,-9500,300],yaw=0,original_indices=[27,28,29]),
    dict(asset=south.name,location_cm=[8650,-10600,300],yaw=0,original_indices=[30,31]),
    dict(asset=corner.name,location_cm=[9650,-10600,300],yaw=0,original_indices=[32]),
    dict(asset=west.name,location_cm=[8300,-10396,300],yaw=90,original_indices=[33]),
    dict(asset=return_rail.name,location_cm=[8300,-9554,300],yaw=90,original_indices=[34])]
assert sorted(i for r in placements for i in r['original_indices'])==list(range(35))
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements,
    qualification='Four 12m ramp runs and six balcony runs. Corners shortened 8cm for butt joints; native guards preserved. Engine and route review pending.'),indent=2))
slope.hide_render=False;corner.hide_render=False;corner.location.y=-1.5
scene.world=bpy.data.worlds.new('Relay rail studio');scene.world.color=(.17,.17,.17)
target=Vector((0,0,1.8))
for pos,power,size in [((1,-5,6),1700,5),((-4,2,5),1400,4),((6,3,6),1800,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(12,-20,10));camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=14;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'rail-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z04-Rails.blend'));bpy.ops.render.render(write_still=True)
