"""Dressed cantilever landing, 2 x 1.43 x .2 m, including a 5 cm wall-joint closure."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z06FlankLandingKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
box('Continuous reinforced bed',(0,0,-.005),(1.96,1.34,.13),grout,.005)
box('Soffit bearing plate',(0,0,-.077),(1.94,1.32,.046),stone,.008)
for x in (-.963,.963):box('Dressed perimeter nosing',(x,0,.025),(.074,1.38,.15),stone,.008)
for y in (-.653,.653):box('Dressed longitudinal nosing',(0,y,.020),(1.852,.074,.14),stone,.008)
for x in (-.612,0,.612):
    for y in (-.305,.305):box('Fitted tread ashlar',(x,y,.067),(.600,.598,.066),stone,.008)
for side in (-1,1):
    box('Recessed fascia channel',(0,side*.664,-.060),(1.80,.008,.016),dark,.002)
    box('Continuous bearing inlay',(0,side*.670,-.060),(1.70,.004,.006),gold,.001)
# Alternating stone and metal crown pieces form a flush, non-overlapping surface.
for y in (-.653,.653):
    cursor=-.926
    for x in (-.78,-.66,-.54,-.42,-.30,-.18,-.06,.06,.18,.30,.42,.54,.66,.78):
        start=x-.012;end=x+.012
        box('Crown stone between registers',((cursor+start)/2,y,.095),(start-cursor,.074,.010),stone,.001)
        box('Flush grip register',(x,y,.095),(.024,.074,.010),gold,.001)
        cursor=end
    box('Crown terminal stone',((cursor+.926)/2,y,.095),(.926-cursor,.074,.010),stone,.001)
for y in (-.306,.306):box('Soffit reinforcement rib',(0,y,-.084),(1.80,.072,.032),stone,.004)
# Five centimetres of added depth close the existing wall/landing seam.
for part in parts:
    part.location.y*=1.43/1.38;part.scale.y*=1.43/1.38
o=export('SM_Aurelion_KIT_Z06FlankLanding',[2,1.43,.2]);manifest[-1].update(nominal_dimensions_m=list(o.dimensions),collision='None; existing flank landing Cube remains authoritative')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source candidate; scene and traversal acceptance pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Landing studio');scene.world.color=(.2,.2,.2)
for pos,power,size in [((2,-3,4),600,3),((-2,2,2),400,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2.8,-3,2.4));camera=bpy.context.object;camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=2.8;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'flank-landing.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-FlankLanding.blend'));bpy.ops.render.render(write_still=True)
print('FLANK_LANDING_BUILD_PASS')
