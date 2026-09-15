"""Repeated vertical Eclipse fissure fitted to the gallery's eastern stone panels."""
from pathlib import Path
source=Path(__file__).with_name('build_eclipse_wall_scars.py')
code=source.read_text().split("(ROOT/'manifest.json').write_text")[0]
code=code.replace("/'EclipseWallKit'","/'Z09SlitKit'").replace('range(2):','range(1):').replace("'SM_Aurelion_KIT_EclipseWallScar_'","'SM_Aurelion_KIT_Z09Slit_'")
code=code.replace("short,.003,.002,violet,.0004","short,.012,.002,violet,.0004")
code=code.replace('    import bmesh',"    for piece in parts:\n        for vertex in piece.data.vertices:\n            x,y,z=vertex.co\n            vertex.co=(x*.7,y*2,(z-.8)*1.8)\n        piece.data.update()\n    import bmesh")
exec(compile(code,str(source),'exec'))
manifest[0]['preserve_fallback_geometry']=True
# Panel centers are derived from the saved wall-bay centers and quarter-width.
placements=[dict(actor='Z09__RestrainedVioletSlit_'+str(i+1).zfill(2),asset=manifest[0]['asset'],location_cm=[775.5,y,-1212],yaw=90) for i,y in enumerate((28001.785714286,28198.214285714,28394.642857143))]
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
obj=modules[0];obj.hide_render=False
for x in (-1,1):
    copy=obj.copy();copy.data=obj.data;scene.collection.objects.link(copy);copy.location.x=x
scene.world=bpy.data.worlds.new('Synchronized fissure studio');scene.world.color=(.2,.2,.2)
for pos,power in [((1,-3,3),650),((-2,-1,1),250)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=3;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(1,-5,2));o=bpy.context.object;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=4.2;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1500;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'slits.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z09-Slits.blend'));bpy.ops.render.render(write_still=True)
