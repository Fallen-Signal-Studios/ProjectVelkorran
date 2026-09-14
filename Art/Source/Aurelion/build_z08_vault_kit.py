"""Crucible coffer kit and retained-shell assembly; metres, original vault pivot."""
from pathlib import Path
import json

base=Path(__file__).resolve().parent
original=base/'build_crucible_vault.py'
old={'__file__':str(original)}
exec(compile(original.read_text().split("bpy.ops.object.select_all(action='DESELECT')")[0],str(original),'exec'),old)
retained=[];removed=[]
for obj in old['parts']:
    if obj.name.startswith(('Coffer longitudinal','Coffer gold line')):
        removed.append(obj.name);old['bpy'].data.objects.remove(obj,do_unlink=True)
    else:retained.append(obj)
assert len(removed)==8
helpers=base/'build_architecture_kit.py'
exec(compile(helpers.read_text().split('# Four metre bay:')[0].replace('bpy.ops.wm.read_factory_settings(use_empty=True)',''),str(helpers),'exec'))
ROOT=base/'Z08VaultKit';ROOT.mkdir(exist_ok=True)
for obj in retained:
    for slot in obj.material_slots:
        slot.material={'M_Vault_Stone':stone,'M_Vault_Gold':gold,'M_Vault_Shadow':dark}.get(slot.material.name,slot.material)
    # Legacy bevels contain collapsed end faces. Remove zero-area topology
    # before UV authoring instead of carrying invalid polygons into the kit.
    import bmesh
    bm=bmesh.new();bm.from_mesh(obj.data)
    bmesh.ops.dissolve_degenerate(bm,dist=0.000001,edges=list(bm.edges))
    bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.calc_area()<1e-10],context='FACES_ONLY')
    bm.to_mesh(obj.data);bm.free();obj.data.update()
parts.extend(retained)
shell=export('SM_Aurelion_KIT_Z08VaultShell',[71.4,49.4,15.4])
manifest[-1]['nominal_dimensions_m']=list(shell.dimensions)
shell.hide_render=True
coffer_source=(base/'build_z07_ceiling_kit.py').read_text().split('def wedge',1)[1].split('wide=coffer',1)[0]
coffer_source='def wedge'+coffer_source
coffer_source=coffer_source.replace("o=export(name,[width,length,.55])", "\n    for part in parts:\n        part.location.z=(part.location.z-.55)*(.45/.55)\n        part.scale.z*=.45/.55\n    o=export(name,[width,length,.45])")
exec(compile(coffer_source,'shared_coffer_construction','exec'))
coffer=coffer(10,8,'SM_Aurelion_KIT_Z08Coffer_10x8')
manifest[-1]['collision']='None; visual soffit remains 12.5 cm above existing portal ribs'
placements=[]
parts.append(shell)
for x in range(-30,31,10):
    for y in range(-20,21,8):
        o=coffer.copy();o.data=coffer.data.copy();scene.collection.objects.link(o)
        o.location=(x,y,15);o.hide_render=False;parts.append(o)
        placements.append([x,y,15])
assert len(placements)==42
# Keep UV1 islands unique across the assembly, including the retained shell.
# UV0 remains metre-scaled and unchanged on every module.
for index,obj in enumerate(parts):
    for loop in obj.data.uv_layers[1].data:
        loop.uv=((loop.uv.x+index%7)/7,(loop.uv.y+index//7)/7)
# Joining preserves the individually authored UV channels; no second unwrap of
# the repeated modules. A single replacement keeps the existing vault actor.
bpy.ops.object.select_all(action='DESELECT')
for obj in parts:obj.select_set(True)
bpy.context.view_layer.objects.active=shell;bpy.ops.object.join();assembly=shell
assembly.name='SM_Aurelion_KIT_Z08VaultAssembly';assembly.hide_render=False
bpy.ops.export_scene.fbx(filepath=str(ROOT/(assembly.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
assembly.data.calc_loop_triangles()
manifest.append(dict(asset=assembly.name,triangles=len(assembly.data.loop_triangles),nominal_dimensions_m=list(assembly.dimensions),materials=[m.name for m in assembly.data.materials],uv_layers=len(assembly.data.uv_layers),convex_hulls=0,collision='None; native room collision retained'))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored kit; in-engine visual and runtime acceptance pending',modules=manifest,coffer_placements_m=placements,removed_longitudinal_parts=removed,retained_shell_parts=len(retained)),indent=2))
assembly.hide_render=True;coffer.hide_render=False
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.world=bpy.data.worlds.new('Coffer review world');scene.world.color=(.2,.2,.2)
for pos,power,size in [((2,-4,-5),1800,6),((-4,1,-3),1200,5),((2,4,-2),700,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);l=bpy.context.object;l.data.energy=power;l.data.shape='DISK';l.data.size=size;l.rotation_euler=(Vector((0,0,-.2))-l.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(7,-8,-10));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,-.2))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=14;scene.camera=camera
scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'coffer-detail.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z08-Vault.blend'));bpy.ops.render.render(write_still=True)
print('Z08_VAULT_KIT_BUILD_PASS')
