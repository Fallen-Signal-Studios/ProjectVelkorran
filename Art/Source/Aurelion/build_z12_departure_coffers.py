"""Bake the custom coffer design into unit-scale departure wall sizes; Blender 4.5."""
from pathlib import Path
builder=Path(__file__).with_name('build_z10_chamber_wall.py')
exec(compile(builder.read_text().split('# Blender-to-Unreal FBX')[0],str(builder),'exec'))
ROOT=Path(__file__).resolve().parent/'Z12DepartureWalls';ROOT.mkdir(exist_ok=True)
original_parts=list(parts);parts.clear()
for index,(key,width) in enumerate((('South',4.49),('North',4.19),('Side',3.99))):
    factors=(width/3.99,-.249/.279,2.99/3.99)
    for original in original_parts:
        copy=original.copy();copy.data=original.data.copy();scene.collection.objects.link(copy)
        for axis,factor in enumerate(factors):
            copy.location[axis]*=factor;copy.scale[axis]*=factor
        parts.append(copy)
    obj=export('SM_Aurelion_KIT_Z12Coffer'+key,[width,.249,2.99])
    manifest[-1].update(nominal_dimensions_m=list(obj.dimensions),position_precision=10,
        preserve_fallback_geometry=True,wall_group=key,
        collision='None: departure art only; native room collision retained')
    obj.location.x=(index-1)*5.1
for obj in original_parts:bpy.data.objects.remove(obj,do_unlink=True)
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Departure kit studio');scene.world.color=(.15,.15,.15)
target=Vector((0,0,0))
for pos,power,size in [((-6,-5,7),2400,7),((7,-4,3),2200,6),((0,4,6),2000,6)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object
    light.data.energy=power;light.data.size=size
    light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(7,-19,7));camera=bpy.context.object
camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=16;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1800;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'departure-coffers.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Departure-Coffers.blend'))
bpy.ops.render.render(write_still=True)
