"""Six-metre wound-gallery variant of the custom Aurelion fluted pier."""
from pathlib import Path
source=Path(__file__).with_name('build_z08_column_kit.py')
exec(compile(source.read_text().split("o=export(")[0],str(source),'exec'))
from mathutils import Matrix
ROOT=Path(__file__).resolve().parent/'Z09PierKit';ROOT.mkdir(exist_ok=True)
# Bake the six-metre proportion into authored geometry; runtime instances stay unit scale.
for obj in parts:
    obj.matrix_world=Matrix.Diagonal((1,1,6/7,1))@obj.matrix_world
o=export('SM_Aurelion_KIT_Z09EngagedPier',[1.2,1.,6.])
manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='None; existing gallery collision retained')
placements=[dict(location_cm=[x,y,-1500],yaw=yaw) for y in (26550,27750,28950,30150) for x,yaw in ((-725,-90),(725,90))]
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
scene.world=bpy.data.worlds.new('Wound gallery pier studio');scene.world.color=(.18,.18,.18)
target=Vector((0,0,3))
for pos,power,size in [((2,-5,6),1800,4),((-3,-1,3),1000,3),((3,3,7),1800,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=size;a.rotation_euler=(target-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(7,-13,8));a=bpy.context.object;a.rotation_euler=(target-a.location).to_track_quat('-Z','Y').to_euler();a.data.type='ORTHO';a.data.ortho_scale=7.3;scene.camera=a
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1000;scene.render.resolution_y=1400;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'pier.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z09-Pier.blend'));bpy.ops.render.render(write_still=True)
