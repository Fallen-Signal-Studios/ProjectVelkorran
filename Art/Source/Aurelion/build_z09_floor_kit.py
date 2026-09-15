"""True-size wound-gallery basalt paving and approach cuts; flush walk surfaces."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z09FloorKit';ROOT.mkdir(exist_ok=True)
basalt=material('M_Aurelion_Basalt',(.035,.041,.047),0,.66)

def floor(name,w,l):
    box('Recessed joint backing',(0,0,.018),(w,l,.036),dark,.001)
    # Eight individually dressed slabs, with real expansion joints. The route
    # conductors belong to the retained gallery actors, not every paving tile.
    for ix in range(2):
        for iy in range(4):
            x=-w/2+(ix+.5)*w/2;y=-l/2+(iy+.5)*l/4
            box('Honed basalt course',(x,y,.078),(w/2-.012,l/4-.012,.084),basalt,.003)
    # Flush end-register pockets: cut space out of the slab rather than stacking
    # coplanar gold faces onto the walking surface.
    # Registers sit in the open transverse joints, below the Z=0.12 walk datum.
    for side in (-1,1):
        for j in (-1,0,1):
            box('Joint registration inlay',(side*(w/2-.18),j*l/4,.116),(.12,.008,.006),gold,.001)
    obj=export(name,[w,l,.12]);obj.hide_render=True
    manifest[-1].update(nominal_dimensions_m=list(obj.dimensions),surface_top_metres=.12,position_precision=10,preserve_fallback_geometry=True,collision='None; native gallery floor collision remains authoritative')
    return obj

main=floor('SM_Aurelion_KIT_Z09Paving',4,55/14)
approach=floor('SM_Aurelion_KIT_Z09PavingApproach',3,4.81)
main.hide_render=False;approach.hide_render=False;approach.location.x=3.7
scene.world=bpy.data.worlds.new('Gallery basalt paving studio');scene.world.color=(.15,.15,.15)
target=Vector((1.6,0,0))
for pos,power,size in [((0,-4,7),2300,5),((5,2,6),2000,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(7,-8,9));o=bpy.context.object;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=10;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'paving.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z09-Paving.blend'));bpy.ops.render.render(write_still=True)
