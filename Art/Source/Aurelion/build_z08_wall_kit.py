"""Seven-metre Crucible masonry, clear lintels and fitted perimeter assembly."""
from pathlib import Path
base=Path(__file__).resolve().parent;helper=base/'build_architecture_kit.py'
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=base/'Z08WallKit';ROOT.mkdir(exist_ok=True)
original_export=export
def export(name,dimensions,collision_points=None):
    factor=7/6 if 'Wall' in name else 5/3
    for o in parts:o.location.z*=factor;o.scale.z*=factor
    o=original_export(name,[dimensions[0],dimensions[1],dimensions[2]*factor],collision_points)
    manifest[-1]['nominal_dimensions_m']=list(o.dimensions)
    manifest[-1]['collision']='None; native Crucible wall and doorway collision retained'
    return o
reference=(base/'build_z07_wall_kit.py').read_text()
wall_definition='def wall'+reference.split('def wall',1)[1].split("side=wall",1)[0]
exec(compile(wall_definition,'shared_dressed_masonry','exec'))
bay=wall('SM_Aurelion_KIT_Z08Wall_4x7',4)
lintel_code=reference.split("box('Lintel continuous backing'",1)[1].split('for row in manifest:',1)[0]
exec(compile("box('Lintel continuous backing'"+lintel_code.replace('Z07Lintel','Z08Lintel'),'shared_clear_lintel','exec'))
placements=[];assembled=[]
def place(module,x,y,z,yaw):
    o=module.copy();o.data=module.data.copy();scene.collection.objects.link(o)
    # Source -Y imports as Unreal +Y; export yaw is correspondingly reversed.
    o.location=(x,-y,z);o.rotation_euler.z=math.radians(-yaw);o.hide_render=False
    assembled.append(o);placements.append(dict(asset=module.name,location_m=[x,y,z],yaw=yaw))
for side in (-1,1):
    for y in range(-22,23,4):place(bay,side*35.165,y,0,-90 if side<0 else 90)
    for x in list(range(-33,-4,4))+list(range(5,34,4)):place(bay,x,side*24.165,0,0 if side<0 else 180)
    place(lintel,0,side*24.165,4.5,0 if side<0 else 180)
assert len(assembled)==58
for i,o in enumerate(assembled):
    for loop in o.data.uv_layers[1].data:loop.uv=((loop.uv.x+i%8)/8,(loop.uv.y+i//8)/8)
bpy.ops.object.select_all(action='DESELECT')
for o in assembled:o.select_set(True)
bpy.context.view_layer.objects.active=assembled[0];bpy.ops.object.join();assembly=bpy.context.object
assembly.name='SM_Aurelion_KIT_Z08WallAssembly';scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.ops.export_scene.fbx(filepath=str(ROOT/(assembly.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
assembly.data.calc_loop_triangles()
manifest.append(dict(asset=assembly.name,triangles=len(assembly.data.loop_triangles),nominal_dimensions_m=list(assembly.dimensions),materials=[m.name for m in assembly.data.materials],uv_layers=2,convex_hulls=0,collision='None; original native walls remain authoritative'))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored perimeter; in-engine review required',modules=manifest,placements=placements),indent=2))
assembly.hide_render=True;bay.hide_render=False;lintel.hide_render=False;lintel.location=(0,0,7.2)
scene.world=bpy.data.worlds.new('Crucible masonry studio');scene.world.color=(.14,.14,.14)
for pos,power,size in [((1,-6,7),2100,5),((-5,-2,4),1300,4),((4,2,7),1600,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((0,0,4))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(9,-16,10));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,4.7))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=12;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'wall-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z08-Walls.blend'));bpy.ops.render.render(write_still=True)
print('Z08_WALL_KIT_BUILD_PASS')
