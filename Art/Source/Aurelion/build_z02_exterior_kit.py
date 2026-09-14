"""Fitted Survivor Bend exterior end bays and dressed roof, metres."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z02ExteriorKit';ROOT.mkdir(exist_ok=True)
fit=json.loads((ROOT/'exterior-fit.json').read_text())
H=9.83

def solid_export(name,lo,hi):
    visual=export(name,[]);manifest[-1]['nominal_dimensions_m']=list(visual.dimensions)
    hull=box('UCX_'+name+'_00',[(a+b)/2 for a,b in zip(lo,hi)],[b-a for a,b in zip(lo,hi)],dark,0)
    bpy.context.view_layer.objects.active=hull;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    hull.hide_render=True;hull.display_type='WIRE';parts.clear()
    bpy.ops.object.select_all(action='DESELECT');visual.select_set(True);hull.select_set(True);bpy.context.view_layer.objects.active=visual
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    manifest[-1].update(convex_hulls=1,solid_bounds_m=[lo,hi],collision='Authored solid core; shallow mouldings remain decorative')
    return visual

def wall(width,suffix):
    box('Exterior continuous stone core',(0,.065,H/2),(width-.012,.25,H-.02),stone,.008)
    if width>1:
        for row in range(13):
            z=.3+(row+.5)*.70
            for col in range(2):box('Exterior ashlar course',(-width/4+col*width/2,-.09,z),(width/2-.008,.065,.69),stone,.005)
        for x in (-width/2+.13,width/2-.13):
            box('Framing pilaster',(x,-.15,4.85),(.19,.16,8.96),stone,.012)
            for z in (.50,5.90,8.90):box('Pilaster collar',(x,-.19,z),(.26,.20,.13),stone,.006)
        for z in (2.30,5.00):
            box('Recessed exterior register',(0,-.132,z),(width-.80,.016,2.20),dark,.002)
            box('Raised stone field',(0,-.151,z),(width-.94,.024,2.06),stone,.009)
            for x in (-.9,.9):box('Register fine inlay',(x,-.169,z),(.018,.006,1.44),gold,0)
        points=[(-1.20,-.16,6.70),(-1.20,-.16,7.80),(0,-.16,8.72),(1.20,-.16,7.80),(1.20,-.16,6.70)]
        path('Crowned stone relief',points,.13,.05,stone,.008)
        path('Relief narrow conductor',[(x,y-.025,z) for x,y,z in points],.022,.012,gold,0)
    else:
        box('Closing pier front',(0,-.13,4.84),(width-.04,.15,9.08),stone,.008)
        box('Closing pier axial reveal',(0,-.211,4.84),(.035,.014,8.5),dark,0)
    for z,depth,height in ((.09,.48,.18),(.25,.40,.12),(9.45,.39,.12),(9.63,.46,.20),(9.79,.50,.08)):
        box('Dressed exterior coping',(0,-.035,z),(width,depth,height),stone,.01)
    o=solid_export('SM_Aurelion_KIT_Z02Exterior_'+suffix,[-width/2,-.06,0],[width/2,.19,H]);o.hide_render=True;return o

wall(4,'4m');wall(fit['outer_x_m']-fit['inner_x_m']-8,'Closing')
W=fit['outer_x_m']*2+.10;L=fit['roof_y_half_m']*2+.10
box('Roof continuous slab',(0,0,.059),(W,L,.118),stone,.008)
for ix in range(8):
    for iy in range(6):
        box('Roof fitted wearing stone',(-W/2+(ix+.5)*W/8,-L/2+(iy+.5)*L/6,.105),(W/8-.008,L/6-.008,.03),stone,.003)
for side in (-1,1):
    for z,width,height in ((.16,.24,.08),(.245,.32,.09)):
        box('Long roof edge coping',(side*(W/2-.16),0,z),(width,L-.64,height),stone,.008)
        box('End roof edge coping',(0,side*(L/2-.16),z),(W,width,height),stone,.008)
    for axis in (-1,1):box('Roof corner cap',(side*(W/2-.16),axis*(L/2-.16),.32),(.34,.34,.06),stone,.01)
for x in (-6.20,6.20):
    box('Roof drainage recess',(x,0,.121),(.055,L-.70,.006),dark,0)
    for y in (-L/2+.65,L/2-.65):
        box('Drain grate bed',(x,y,.13),(.45,.55,.025),dark,.002)
        for j in range(7):box('Drain grate slat',(x-.18+j*.06,y,.15),(.024,.48,.02),gold,.002)
roof=solid_export('SM_Aurelion_KIT_Z02RoofFinish',[-W/2,-L/2,0],[W/2,L/2,.12])
manifest[-1]['preserve_fallback_geometry']=True
scene.world=bpy.data.worlds.new('Exterior studio');scene.world.color=(.16,.16,.16)
for pos in ((-16,-20,28),(15,12,22)):
    d=bpy.data.lights.new('Exterior softbox','AREA');d.energy=18000;d.size=18;o=bpy.data.objects.new('Exterior softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Exterior review');camera=bpy.data.objects.new('Exterior review',d);scene.collection.objects.link(camera);scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1500;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
camera.location=(25,-28,25);camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=40;scene.render.filepath=str(ROOT/'Roof.png');bpy.ops.render.render(write_still=True)
roof.hide_render=True;modules[0].hide_render=False
camera.location=(8,-20,10);camera.rotation_euler=(Vector((0,0,4.8))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=48;scene.render.filepath=str(ROOT/'End-bay.png');bpy.ops.render.render(write_still=True)
for o in modules:o.hide_render=False
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_Z02ExteriorKit.blend'))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Fitted exterior candidate; level and live traversal acceptance pending',modules=manifest),indent=2))
