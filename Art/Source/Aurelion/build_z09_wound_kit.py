"""Three horizontal stone-fracture variants for the north wall and lintel."""
from pathlib import Path
source=Path(__file__).with_name('build_eclipse_wall_scars.py')
code=source.read_text().split("(ROOT/'manifest.json').write_text")[0]
code=code.replace("/'EclipseWallKit'","/'Z09WoundKit'").replace('range(2):','range(3):').replace("'SM_Aurelion_KIT_EclipseWallScar_'","'SM_Aurelion_KIT_Z09Wound_'")
code=code.replace('    import bmesh',"    for piece in parts:\n        for vertex in piece.data.vertices:\n            x,y,z=vertex.co\n            vertex.co=((z-.8)*1.1,y*2,x*.30)\n        piece.data.update()\n    import bmesh")
exec(compile(code,str(source),'exec'))
for spec in manifest:spec['preserve_fallback_geometry']=True
placements=[dict(actor='Z09__WoundDiscontinuity_01',asset=manifest[0]['asset'],location_cm=[-620,30825.5,-1180],yaw=180),dict(actor='Z09__WoundDiscontinuity_02',asset=manifest[1]['asset'],location_cm=[0,30825.5,-980],yaw=180),dict(actor='Z09__WoundDiscontinuity_03',asset=manifest[2]['asset'],location_cm=[620,30825.5,-1180],yaw=180)]
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
for i,obj in enumerate(modules):obj.hide_render=False;obj.location.z=i*.65
scene.world=bpy.data.worlds.new('Wound fracture studio');scene.world.color=(.20,.20,.20)
for pos,power in [((1,-3,3),600),((-2,-1,1),220)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=3;o.rotation_euler=(Vector((0,0,.6))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(.5,-4,1.8));o=bpy.context.object;o.rotation_euler=(Vector((0,0,.6))-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=2.6;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1500;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'wounds.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z09-Wounds.blend'));bpy.ops.render.render(write_still=True)
