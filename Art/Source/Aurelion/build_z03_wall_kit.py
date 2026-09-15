"""True-size sensor gallery masonry, using the established Aurelion cut-stone vocabulary."""
from pathlib import Path
source = Path(__file__).with_name('build_z07_wall_kit.py').read_text()
assert source.count('(w-.012,.18,6)') == 1 and source.count('(6,.22,1.5)') == 1
# Set the concealed backing inside the exposed courses to avoid coplanar faces.
source = source.replace('(w-.012,.18,6)', '(w-.012,.18,5.96)').replace('(6,.22,1.5)', '(5.94,.22,1.44)')
source = source.replace("    o=export(name,[w,.85,6])", "    box('Upper coffer backing',(0,0,6.275),(w-.012,.85,.55),stone,0)\n    o=export(name,[w,.85,6.55])")
source = source.replace("lintel=export('SM_Aurelion_KIT_Z07Lintel',[6,.85,1.5])", "box('Upper lintel backing',(0,0,1.775),(5.988,.85,.55),stone,0)\nlintel=export('SM_Aurelion_KIT_Z07Lintel',[6,.85,2.05])")
exec(compile(source.split("side=wall(", 1)[0], 'aurelion_wall_vocabulary', 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z03WallKit'
ROOT.mkdir(exist_ok=True)
side = wall('SM_Aurelion_KIT_Z03Wall4m', 4)
end = wall('SM_Aurelion_KIT_Z03Wall44m', 4.4)
lintel_source = "box('Lintel continuous backing'" + source.split("box('Lintel continuous backing'", 1)[1].split('for row in manifest:', 1)[0]
exec(compile(lintel_source.replace('SM_Aurelion_KIT_Z07Lintel', 'SM_Aurelion_KIT_Z03Lintel'), 'aurelion_lintel', 'exec'))
placements = []
for x, yaw in ((5882.5, -90), (8117.5, 90)):
    for j in range(12):
        placements.append(dict(asset=side.name, location_cm=[x, -19600 + j * 400, 0], yaw=yaw))
for x in (6100, 6500, 7500, 7900):
    placements.append(dict(asset=side.name, location_cm=[x, -14982.5, 0], yaw=180))
for j in range(5):
    placements.append(dict(asset=end.name, location_cm=[6120 + j * 440, -19817.5, 0], yaw=0))
placements.append(dict(asset=lintel.name, location_cm=[7000, -14982.5, 450], yaw=180))
for row in manifest:
    row['collision'] = 'None; native Z03 room walls remain authoritative'
    row['position_precision'] = 10
    row['preserve_fallback_geometry'] = True
(ROOT / 'manifest.json').write_text(json.dumps(dict(modules=manifest, placements=placements,
    qualification='34 fitted wall sections. Visual depth projects outward from native room faces; clear north doorway is 6m wide and 4.5m tall.'), indent=2))
room = json.loads((ROOT.parent / 'Z03CeilingKit/room-baseline.json').read_text())
baseline = next(c for c in room['components'] if c['actor'] == 'Aurelion_Art_M12_Z03_13_9b1ee2')
(ROOT / 'wall-baseline.json').write_text(json.dumps(baseline, indent=2))
end.hide_render = False
lintel.hide_render = False
lintel.location.z = 6.7
scene.world = bpy.data.worlds.new('Sensor gallery studio')
scene.world.color = (.14, .14, .14)
for pos, power, size in [((1,-6,7),2100,5),((-5,-2,4),1300,4),((4,2,7),1600,4)]:
    bpy.ops.object.light_add(type='AREA', location=pos)
    o = bpy.context.object; o.data.energy = power; o.data.size = size
    o.rotation_euler = (Vector((0,0,3)) - o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(10,-17,10))
camera = bpy.context.object
camera.rotation_euler = (Vector((0,0,3.8))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO'; camera.data.ortho_scale=11; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1200; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'wall-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z03-Walls.blend'))
bpy.ops.render.render(write_still=True)
