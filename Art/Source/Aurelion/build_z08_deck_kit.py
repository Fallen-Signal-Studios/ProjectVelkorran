"""True-size ashlar decking and edge strips, fitted to all 42 retained panels."""
from pathlib import Path
from mathutils import Quaternion
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z08DeckKit';ROOT.mkdir(exist_ok=True)
old=json.loads((ROOT/'deck-baseline.json').read_text());anchor=Vector((0,208,-12));cache={};assembled=[];placements=[]
for row in old['instances']:
    w,l,h=[2*old['mesh_extent'][i]*row['scale'][i]/100 for i in range(3)];key=tuple(round(v,6) for v in (w,l,h))
    if key not in cache:
        box('Continuous stone backing',(0,0,-h/2),(w,l,h),stone,.001)
        # Surface courses rise from a six-mm joint bed and stay at the original top.
        parts[-1].scale.z=(h-.006)/h;parts[-1].location.z=-(h+.006)/2
        if l<.15:
            for side in (-1,1):box('Dressed edge strip',(0,side*(l+.008)/4,-.003),(w,(l-.008)/2,.006),stone,.001)
            box('Inset edge conductor',(0,0,-.001),(w,.008,.002),gold,.0004)
        else:
            nx=max(1,math.ceil(w/.85));ny=max(1,math.ceil(l/.85));margin=.045
            for ix in range(nx):
                for iy in range(ny):
                    tw=(w-.28)/nx;tl=(l-2*margin)/ny
                    box('Dressed paving course',(-w/2+.14+(ix+.5)*tw,-l/2+margin+(iy+.5)*tl,-.003),(tw-.006,tl-.006,.006),stone,.001)
            for side in (-1,1):
                for distance,width in ((.00975,.0195),(.03125,.0135)):
                    box('Side margin stone',(0,side*(l/2-distance),-.003),(w,width,.006),stone,.001)
                box('Edge fine conductor',(0,side*(l/2-.022),-.001),(w,.005,.002),gold,.0004)
                for lo,hi in ((0,.072),(.078,.090),(.096,.108),(.114,.14)):
                    box('End margin stone',(side*(w/2-(lo+hi)/2),0,-.003),(hi-lo,l-.076,.006),stone,.001)
                # Dark grip grooves remain flush, with no raised trip geometry.
                for j in range(3):
                    box('Threshold grip register',(side*(w/2-.075-j*.018),0,-.0007),(.006,l-.076,.0014),dark,.0003)
        name='SM_Aurelion_KIT_Z08Deck_'+str(len(cache)+1)
        module=export(name,[w,l,h]);module.hide_render=True;cache[key]=module
        manifest[-1]['nominal_dimensions_m']=list(module.dimensions);manifest[-1]['collision']='None; retains native floor/ramp planes'
    module=cache[key];q=row['quaternion'];rotation=Quaternion((q[3],q[0],q[1],q[2]))
    local_top=Vector([old['mesh_origin'][0]*row['scale'][0],old['mesh_origin'][1]*row['scale'][1],(old['mesh_origin'][2]+old['mesh_extent'][2])*row['scale'][2]])
    world_top=(Vector(row['location'])+rotation@local_top)/100
    o=module.copy();o.data=module.data.copy();scene.collection.objects.link(o);delta=world_top-anchor
    o.location=(delta.x,-delta.y,delta.z);o.rotation_mode='QUATERNION';o.rotation_quaternion=Quaternion((q[3],-q[0],q[1],-q[2]));o.hide_render=False;assembled.append(o)
    placements.append(dict(original_index=row['index'],asset=module.name,size_m=[w,l,h],top_center_world_m=list(world_top),quaternion=q))
assert len(assembled)==42
for i,o in enumerate(assembled):
    for loop in o.data.uv_layers[1].data:loop.uv=((loop.uv.x+i%7)/7,(loop.uv.y+i//7)/6)
bpy.ops.object.select_all(action='DESELECT')
for o in assembled:o.select_set(True)
bpy.context.view_layer.objects.active=assembled[0];bpy.ops.object.join();assembly=bpy.context.object
assembly.name='SM_Aurelion_KIT_Z08DeckAssembly';scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.ops.export_scene.fbx(filepath=str(ROOT/(assembly.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
assembly.data.calc_loop_triangles();manifest.append(dict(asset=assembly.name,triangles=len(assembly.data.loop_triangles),nominal_dimensions_m=list(assembly.dimensions),materials=[m.name for m in assembly.data.materials],uv_layers=2,convex_hulls=0,collision='None; all 42 retained surface footprints',preserve_fallback_geometry=True))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored true-size decking; in-engine fit and visual acceptance pending',modules=manifest,anchor_world_m=list(anchor),placements=placements),indent=2))
assembly.hide_render=True;sample=next(o for o in cache.values() if o.dimensions.y>1);sample.hide_render=False
scene.world=bpy.data.worlds.new('Deck studio');scene.world.color=(.18,.18,.18)
for pos,power,size in [((2,-4,5),1500,4),((-4,2,3),1000,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4,-5,5));camera=bpy.context.object;camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=5;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1300;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'deck.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Crucible-Decking.blend'));bpy.ops.render.render(write_still=True)
print('CRUCIBLE_DECK_KIT_BUILD_PASS')
