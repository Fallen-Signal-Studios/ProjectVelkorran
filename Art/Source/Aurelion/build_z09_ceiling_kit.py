"""Fitted wound-gallery stone coffers; preserve the six-metre underside datum."""
from pathlib import Path
base=Path(__file__).resolve().parent
code=(base/'build_z06_ceiling_kit.py').read_text().split('wide=coffer(')[0]
code=code.replace("/'Z06CeilingKit'","/'Z09CeilingKit'").replace('6.45 m room clearance','6 m room clearance')
code=code.replace('(-width/4,width/4)','(-(width-.36)/4,(width-.36)/4)').replace('(width/2-.012,.18,.22)','((width-.36)/2-.012,.18,.22)')
code=code.replace('    o=export(name,[width,4,.55])',"""    # Close beam-to-backing gaps, including outer room edges.
    for x in (-width/2+.045,width/2-.045):box('Roof edge upstand',(x,0,.345),(.06,3.97,.29),stone,0)
    for y in (-1.955,1.955):box('Roof transverse upstand',(0,y,.345),(width-.16,.06,.29),stone,0)
    for part in parts:
        part.location.y*=55/56;part.scale.y*=55/56
        bpy.context.view_layer.objects.active=part
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o=export(name,[width,55/14,.55])""")
exec(compile(code,'wound_gallery_coffer_vocabulary','exec'),globals())
o=coffer(4,'SM_Aurelion_KIT_Z09Coffer');o.hide_render=False
manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='None; underside remains at the original gallery ceiling datum, relief extends upward')
placements=[dict(location_cm=[-800+(i+.5)*400,25350+(j+.5)*5500/14,-900],yaw=0) for i in range(4) for j in range(14)]
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
scene.world=bpy.data.worlds.new('Wound-gallery coffer studio');scene.world.color=(.15,.15,.15)
for pos,power in [((2,-4,-4),1200),((-4,1,-2),750),((2,4,-1),550)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=4;a.rotation_euler=(Vector((0,0,.2))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(5,-6,-5));a=bpy.context.object;a.rotation_euler=(Vector((0,0,.2))-a.location).to_track_quat('-Z','Y').to_euler();a.data.type='ORTHO';a.data.ortho_scale=6.5;scene.camera=a
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'coffer.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z09-Ceiling.blend'));bpy.ops.render.render(write_still=True)
