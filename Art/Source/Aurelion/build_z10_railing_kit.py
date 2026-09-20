"""Four fitted chamber balustrades using the established Aurelion open-metalwork design."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z10RailingKit'
fit=json.loads((ROOT/'rail-fit.json').read_text())
design=Path(__file__).with_name('build_z09_railing_kit.py').read_text().split('# Built-up dressed posts',1)[1].split("obj=export(",1)[0]
design='# Built-up dressed posts'+design
design=design.replace('range(4)','range(bays)').replace('(bay+.5)*w/4','(bay+.5)*w/bays')
for spec in fit['modules']:
    w,target_depth,target_height=spec['dimensions_m'];d=.22;h=1.30;bays=2 if w<3 else 4
    exec(compile(design,'shared_aurelion_balustrade','exec'))
    for piece in parts:
        piece.location.y*=target_depth/.22;piece.scale.y*=target_depth/.22
        piece.location.z*=target_height/1.3;piece.scale.z*=target_height/1.3
    obj=export(spec['asset'],spec['dimensions_m'])
    manifest[-1].update(nominal_dimensions_m=list(obj.dimensions),position_precision=10,preserve_fallback_geometry=True,collision='None: native chamber barriers remain authoritative')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
for i,obj in enumerate(modules):obj.location.y=i*1.7
scene.world=bpy.data.worlds.new('Chamber rail studio');scene.world.color=(.16,.16,.16)
target=Vector((0,2.5,.7))
for pos,power in [((-3,-3,7),2300),((4,5,6),1900)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=5;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(8,-11,7));o=bpy.context.object;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=9;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1500;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'guardrails.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Chamber-Guardrails.blend'));bpy.ops.render.render(write_still=True)
