"""True-size Aurelion approach balustrade; consolidate touching doubled vendor rails."""
from pathlib import Path
helper = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0], str(helper), 'exec'))
ROOT = Path(__file__).resolve().parent/'Z09RailingKit'
ROOT.mkdir(exist_ok=True)
w, d, h = 4.75, .22, 1.30

# Built-up dressed posts and recessed flutes repeat the gallery's engaged piers.
for x in (-w/2+.075, 0, w/2-.075):
    box('Plinth shoe', (x,0,.025), (.15,d,.05), stone,.006)
    box('Stepped plinth', (x,0,.07), (.125,.185,.04), stone,.004)
    box('Post shaft', (x,0,.645), (.095,.135,1.11), stone,.006)
    box('Capital neck', (x,0,1.207), (.115,.17,.045), stone,.004)
    box('Capital bearing', (x,0,1.251), (.15,d,.043), stone,.005)
    for side in (-1,1):
        box('Recessed flute backing', (x,side*.069,.65), (.036,.007,.985), dark,.001)
        box('Bronze flute register', (x,side*.074,.65), (.012,.004,.88), gold,.001)
        for z in (.07,1.208):
            box('Recessed anchor seat', (x,side*.094,z), (.033,.008,.019), dark,.001)
            box('Bronze anchor', (x,side*.099,z), (.015,.004,.010), gold,.001)

box('Rounded continuous grip', (0,0,1.28), (w,.11,.04), dark,.01)
for side in (-1,1):
    box('Grip conductor', (0,side*.054,1.277), (w-.04,.005,.009), gold,.001)
box('Lower structural tie', (0,0,.205), (w-.16,.045,.04), dark,.006)

# Open metalwork preserves sightlines. Four shallow pointed registers echo the
# architecture without repeating large gold medallions at hand height.
# Balusters run from inside the tie into the grip; ferrules stay shallower than
# the tie so their faces never share its planes.
for bay in range(4):
    x = -w/2 + (bay+.5)*w/4
    for offset in (-.29,0,.29):
        bx=x+offset
        box('Guard baluster', (bx,0,.74), (.023,.035,1.06), dark,.004)
        for z in (.241,1.245):
            box('Fitted ferrule', (bx,0,z), (.033,.039,.034), dark,.004)
    # Recessed within baluster depth and seated into both outer balusters.
    path('Pointed upper register', [(x-.29,-.014,.90),(x-.145,-.014,1.08),
        (x,-.014,1.15),(x+.145,-.014,1.08),(x+.29,-.014,.90)], .025,.028,dark,.003)

obj=export('SM_Aurelion_KIT_Z09Guardrail',[w,d,h])
manifest[-1].update(nominal_dimensions_m=list(obj.dimensions), position_precision=10,
    preserve_fallback_geometry=True, collision='None; existing native approach barriers remain authoritative')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))

# Pair identity comes from the fresh saved-map census. Unit-scale placement uses
# the measured union envelope, rather than baking the vendor's stretched scale.
old=json.loads((ROOT/'rail-baseline.json').read_text(encoding='utf-8-sig'))
pairs=((0,1),(2,3),(4,5),(6,7))
fit=[]
for ids in pairs:
    a,b=(old['instances'][i] for i in ids)
    assert max(abs(x-y) for x,y in zip(a['scale'],b['scale'])) < 1e-8
    assert abs(abs(a['location'][0]-b['location'][0])-11) < .001
    fit.append(dict(original_indices=list(ids), location=[sum(r['location'][0] for r in (a,b))/2,
        a['location'][1]+old['mesh_origin'][0]*a['scale'][0],
        a['location'][2]+(old['mesh_origin'][2]-old['mesh_extent'][2])*a['scale'][2]],
        scale=[1,1,1], quaternion=a['quaternion']))
(ROOT/'rail-fit.json').write_text(json.dumps(dict(placements=fit, original_instances=8, fitted_instances=4),indent=2))

scene.world=bpy.data.worlds.new('Wound gallery guardrail studio');scene.world.color=(.14,.14,.14)
target=Vector((0,0,.65))
for pos,power,size in [((1,-3,4),1100,4),((-3,2,3),950,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);light=bpy.context.object
    light.data.energy=power;light.data.size=size;light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4,-7,3));camera=bpy.context.object
camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=5.6;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'guardrail.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z09-Guardrail.blend'))
bpy.ops.render.render(write_still=True)
