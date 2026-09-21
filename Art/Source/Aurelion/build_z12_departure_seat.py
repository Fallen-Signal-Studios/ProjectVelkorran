"""Custom three-place Aurelion departure seating; Blender 4.5, metres."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z12DepartureSeat';ROOT.mkdir(exist_ok=True)
# Long axis Y, back toward +X. Lower edge sits on the floor at zero.
for y in (-1.52,1.52):
    box('Dressed foot',(0,y,.055),(.94,.32,.11),stone,.024)
    box('Foot shadow break',(0,y,.121),(.81,.25,.022),dark,.005)
    path('Swept seat support',[(-.31,y-.10,.13),(-.23,y-.10,.30),(.13,y-.10,.39),(.28,y-.10,.82)],.15,.20,stone,.015)
    path('Support inset',[(-.30,y-.112,.14),(-.21,y-.112,.27),(.18,y-.112,.36),(.32,y-.112,.79)],.028,.012,gold,.004)
box('Suspended lower frame',(0,0,.345),(.75,3.42,.13),dark,.018)
for x in (-.36,.36):
    box('Continuous frame arris',(x,0,.367),(.042,3.43,.07),gold,.008)
for y in (-1.11,0,1.11):
    box('Seat stone cradle',(-.035,y,.419),(.83,1.075,.09),stone,.023)
    box('Seat reveal',(-.043,y,.472),(.759,.995,.019),gold,.005)
    box('Contoured seat pad',(-.052,y,.516),(.745,.98,.079),dark,.028)
    # Fine stitched channels and segmented forward wear strip remain geometry.
    for dy in (-.34,0,.34):
        box('Seat channel',(-.075,y+dy,.556),(.59,.012,.003),gold,.001)
    for z in (.61,.77):
        box('Backrest stone shell',(.339,y,z),(.105,1.067,.19),stone,.018)
        box('Backrest shadow bed',(.278,y,z),(.026,.995,.145),gold,.006)
        box('Backrest inset',(.255,y,z),(.045,.966,.127),dark,.015)
    box('Backrest upper cap',(.339,y,.894),(.14,1.075,.049),stone,.012)
    box('Backrest rear conductor',(.397,y,.721),(.012,.043,.28),gold,.003)
    for dy in (-.4,.4):
        box('Rear keyed joint',(.398,y+dy,.721),(.016,.10,.054),dark,.005)
        box('Rear joint insert',(.409,y+dy,.721),(.009,.047,.021),gold,.003)
for y in (-1.70,1.70):
    path('Terminal arm arch',[(-.32,y-.06,.39),(-.28,y-.06,.64),(.28,y-.06,.67),(.34,y-.06,.85)],.09,.12,stone,.012)
    box('Arm contact pad',(-.02,y,.699),(.50,.11,.037),dark,.011)
    box('Arm fine inlay',(-.02,y-.067,.65),(.38,.012,.021),gold,.004)
# Symmetric Y design; standard FBX handedness does not alter the facing axis.
obj=export('SM_Aurelion_KIT_DepartureSeat',[.94,3.52,.9185])
manifest[-1].update(nominal_dimensions_m=list(obj.dimensions),position_precision=10,
    preserve_fallback_geometry=True,collision='None: decorative seat; existing no-collision placement retained')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Seat studio');scene.world.color=(.15,.15,.15)
target=Vector((0,0,.45))
for pos,power,size in [((-4,-4,5),1300,4),((2,4,3),1100,3),((4,-1,4),900,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object
    light.data.energy=power;light.data.size=size
    light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(-4,-5,3));camera=bpy.context.object
camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=4.8;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'departure-seat.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Departure-Seat.blend'))
bpy.ops.render.render(write_still=True)
