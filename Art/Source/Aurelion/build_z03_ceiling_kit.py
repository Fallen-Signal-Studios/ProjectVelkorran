"""True-size 11/3 by 4 m coffers across Selene's 22 by 48 m sensor room."""
from pathlib import Path
base=Path(__file__).resolve().parent
code=(base/'build_z06_ceiling_kit.py').read_text().split("wide=coffer(")[0]
code=code.replace("/'Z06CeilingKit'","/'Z03CeilingKit'").replace('6.45 m room clearance','6 m room clearance')
code=code.replace('(-width/4,width/4)', '(-(width-.36)/4,(width-.36)/4)').replace('(width/2-.012,.18,.22)', '((width-.36)/2-.012,.18,.22)')
exec(compile(code,'shared_stone_coffer','exec'),globals())
o=coffer(11/3,'SM_Aurelion_KIT_Z03Coffer');o.hide_render=False
manifest[-1].update(position_precision=10,preserve_fallback_geometry=True)
baseline=json.loads((ROOT/'ceiling-baseline.json').read_text());placements=[]
for row in baseline['measured_bounds']:
    lo,hi=row['bounds'];assert abs(hi[0]-lo[0]-1100/3)<.01 and abs(hi[1]-lo[1]-400)<.01 and abs(lo[2]-600)<.01
    placements.append(dict(original_index=row['index'],location_cm=[(lo[0]+hi[0])/2,(lo[1]+hi[1])/2,600],yaw=0))
assert len(placements)==72
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source authored; entire measured Z03 ceiling replacement pending engine review',modules=manifest,placements=placements),indent=2))
scene.world=bpy.data.worlds.new('Sensor coffer studio');scene.world.color=(.15,.15,.15)
for pos,power in [((2,-4,-4),1200),((-4,1,-2),750),((2,4,-1),550)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=4;a.rotation_euler=(Vector((0,0,.2))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(5,-6,-5));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.2))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=6;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'sensor-coffer.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z03-Coffer.blend'));bpy.ops.render.render(write_still=True)
print('Z03_CEILING_SOURCE_PASS')
