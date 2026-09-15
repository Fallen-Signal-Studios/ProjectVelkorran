"""Sensor-gallery piers and complete rib-cover assemblies at authored dimensions."""
from pathlib import Path
source=Path(__file__).with_name('build_z08_column_kit.py').read_text()
exec(compile(source.split("o=export(",1)[0],'aurelion_pier_vocabulary','exec'))
ROOT=Path(__file__).resolve().parent/'Z03RibKit';ROOT.mkdir(exist_ok=True)
# Fit the established seven-metre vocabulary in Blender, not through scaled UE instances.
for o in parts:
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    o.location.z*=6/7;o.scale.z*=6/7
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
pier=export('SM_Aurelion_KIT_Z03Pier',[1.2,1.,6.]);pier.hide_render=True

# Three solid, floor-standing architectural rib remnants replace 15 tiled columns each.
# Keep a continuous stone core: apparent gaps must not imply openings through the native collider.
box('Continuous rib core',(0,0,1.5),(3.86,1.80,2.88),stone,.016)
for z,w,d,h in ((.06,4.,2.,.12),(.18,3.94,1.94,.10),(2.78,3.94,1.94,.12),(2.94,4.,2.,.12)):
    box('Rib end course',(0,0,z),(w,d,h),stone,.014)
for x in (-1.90,1.90):
    box('Rib edge buttress',(x,0,1.49),(.16,1.93,2.42),stone,.012)
for side in (-1,1):
    for x in (-1.28,0,1.28):
        for z in (.92,2.08):
            box('Dressed rib face',(x,side*.915,z),(1.22,.11,1.08),stone,.017)
            for dx in (-.49,.49):
                box('Face incision',(x+dx,side*.974,z),(.012,.012,.89),dark,.002)
        for j in range(5):
            box('Cold service register',(x,side*.976,1.455+j*.021),(.22,.014,.006),gold,.001)
    for x in (-.64,.64):
        box('Rib vertical channel',(x,side*.938,1.48),(.06,.05,2.30),dark,.004)
        box('Rib narrow conductor',(x,side*.969,1.48),(.013,.015,2.23),gold,.002)
    for z in (.30,2.64):
        box('Inset transverse reveal',(0,side*.916,z),(3.65,.032,.046),dark,.004)
for side in (-1,1):
    for y in (-.57,0,.57):
        box('End grain field',(side*1.968,y,1.5),(.035,.50,2.26),stone,.006)
    for z in (.46,2.53):
        box('End register',(side*1.990,0,z),(.006,1.55,.020),gold,.001)
cover=export('SM_Aurelion_KIT_Z03RibCover',[4,2,3]);cover.hide_render=True
for row in manifest:
    row.update(position_precision=10,preserve_fallback_geometry=True,collision='None; measured native Z03 rib collision remains authoritative')
placements=[]
for y in (-18600,-17400,-16200):
    for x,yaw in ((5975,-90),(8025,90)):
        placements.append(dict(asset=pier.name,location_cm=[x,y,0],yaw=yaw))
for x,y in ((6400,-18600),(6900,-17400),(6400,-16300)):
    placements.append(dict(asset=cover.name,location_cm=[x,y,0],yaw=90))
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
room=json.loads((ROOT.parent/'Z03CeilingKit/room-baseline.json').read_text())
old=next(c for c in room['components'] if c['actor']=='Aurelion_Art_M12_Z03_14_9e14c4')
(ROOT/'rib-baseline.json').write_text(json.dumps(old,indent=2))
pier.hide_render=False;pier.location.x=-2.7;cover.hide_render=False;cover.location.x=.8
scene.world=bpy.data.worlds.new('Sensor rib studio');scene.world.color=(.15,.15,.15)
for pos,power,size in [((2,-5,6),1900,4),((-4,-1,3),1200,3),((3,3,7),1700,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((0,0,2.7))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(10,-17,10));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,2.8))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=11;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'rib-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z03-Ribs.blend'));bpy.ops.render.render(write_still=True)
