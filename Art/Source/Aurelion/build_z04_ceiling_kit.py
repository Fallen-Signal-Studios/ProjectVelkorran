"""Full 62 x 38 m relay roof: fitted coffers with continuous upper edge closure."""
from pathlib import Path
base=Path(__file__).resolve().parent
code=(base/'build_z06_ceiling_kit.py').read_text().split('wide=coffer(')[0]
code=code.replace("/'Z06CeilingKit'","/'Z04CeilingKit'").replace('6.45 m room clearance','7 m room clearance')
code=code.replace('(-width/4,width/4)','(-(width-.36)/4,(width-.36)/4)').replace('(width/2-.012,.18,.22)','((width-.36)/2-.012,.18,.22)')
code=code.replace('    o=export(name,[width,4,.55])',"""    # Upstands close the perimeter between beams and backing, including room edges.
    for x in (-width/2+.045,width/2-.045):box('Roof edge upstand',(x,0,.345),(.06,3.97,.29),stone,0)
    for y in (-1.955,1.955):box('Roof transverse upstand',(0,y,.345),(width-.16,.06,.29),stone,0)
    for part in parts:
        part.location.y*=38/36;part.scale.y*=38/36
        bpy.context.view_layer.objects.active=part
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o=export(name,[width,38/9,.55])""")
exec(compile(code,'relay_coffer_vocabulary','exec'),globals())
o=coffer(62/16,'SM_Aurelion_KIT_Z04Coffer');o.hide_render=False
manifest[-1].update(position_precision=10,preserve_fallback_geometry=True)
placements=[dict(location_cm=[3900+(i+.5)*6200/16,-12900+(j+.5)*3800/9,700],yaw=0) for i in range(16) for j in range(9)]
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
scene.world=bpy.data.worlds.new('Relay coffer studio');scene.world.color=(.15,.15,.15)
for pos,power in [((2,-4,-4),1200),((-4,1,-2),750),((2,4,-1),550)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=4;a.rotation_euler=(Vector((0,0,.2))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(5,-6,-5));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.2))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=6.5;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'relay-coffer.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z04-Ceiling.blend'));bpy.ops.render.render(write_still=True)
