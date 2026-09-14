"""Aurelion service coffers and buttresses fitted to Z01 cover silhouettes."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'CoverKit'; ROOT.mkdir(exist_ok=True)
W,D,H=1.434004,2.795958,1.122887

def export_solid(name,bottom,top):
    o=export(name,[W,D,top-bottom]); spec=manifest[-1]
    hull=box('UCX_'+name+'_00',(0,0,(bottom+top)/2),(W,D,top-bottom),dark,0)
    hull.hide_render=True; hull.display_type='WIRE'; parts.clear()
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); hull.select_set(True); bpy.context.view_layer.objects.active=o
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    spec.update(convex_hulls=1,solid_bounds_m=[[-W/2,-D/2,bottom],[W/2,D/2,top]],collision='Authored solid convex envelope for combat cover')
    o.hide_render=True
    return o

def coffer():
    box('Coffer footing',(0,0,.07),(W,D,.14),stone,.018)
    box('Coffer lower recessed seam',(0,0,.157),(W-.055,D-.055,.035),dark,.005)
    box('Monolithic service housing',(0,0,.568),(W-.10,D-.10,.79),stone,.028)
    box('Upper isolation seam',(0,0,.971),(W-.065,D-.065,.045),dark,.006)
    box('Honed stone cover slab',(0,0,H-.064),(W,D,.128),stone,.024)
    # Long faces: deeply nested panels with restrained conductive ribs.
    for side in (-1,1):
        for y in (-.88,0,.88):
            box('Coffer panel reveal',(side*(W/2-.041),y,.566),(.035,.79,.60),dark,.008)
            box('Coffer chamfered panel',(side*(W/2-.021),y,.566),(.035,.70,.50),stone,.014)
            for dy in (-.28,.28):
                box('Panel conductive tick',(side*(W/2-.003),y+dy,.566),(.006,.018,.39),gold,0)
        for y in (-1.18,1.18):
            box('Service locking strap',(side*(W/2-.025),y,.564),(.045,.12,.69),stone,.014)
            box('Strap inlay',(side*(W/2-.003),y,.564),(.006,.032,.53),gold,0)
    for side in (-1,1):
        box('End access reveal',(0,side*(D/2-.04),.566),(1.11,.035,.6),dark,.008)
        box('End access stone',(0,side*(D/2-.02),.566),(.98,.035,.49),stone,.014)
        for x in (-.4,.4):
            box('End locking rail',(x,side*(D/2-.003),.566),(.028,.006,.36),gold,0)
    # Fine top inlays stay below the wearing surface, not raised trip geometry.
    for x in (-.53,.53):box('Cover slab conductor',(x,0,H-.001),(.016,D-.30,.002),gold,0)

coffer(); low=export_solid('SM_Aurelion_KIT_CoverCoffer',0,H)
coffer()
# The upper legacy crate floated 4.216 cm above its support. This fitted skirt
# reaches the lower coffer without changing the upper cover's top or placement.
skirt=.042158
box('Stack seating skirt',(0,0,-skirt/2),(W-.06,D-.06,skirt),stone,.007)
stack=export_solid('SM_Aurelion_KIT_CoverCofferStack',-skirt,H)

tall_h=5.441921
box('Buttress continuous backing',(0,0,tall_h/2),(W-.12,D-.12,tall_h-.06),stone,.02)
for z,height in ((.12,.24),(.36,.12),(tall_h-.30,.16),(tall_h-.10,.20)):
    box('Buttress footing or capital',(0,0,z),(W,D,height),stone,.025)
for side in (-1,1):
    for i in range(5):
        z=.94+i*.87
        box('Buttress fitted ashlar',(side*(W/2-.06),0,z),(.11,D-.17,.852),stone,.018)
        for y in (-.93,0,.93):
            box('Buttress vertical reveal',(side*(W/2-.003),y,z),(.006,.11,.76),dark,.002)
            box('Buttress conduction strip',(side*(W/2-.003),y,z),(.006,.025,.71),gold,0)
    for i in range(5):
        z=.94+i*.87
        box('Buttress end field',(0,side*(D/2-.04),z),(W-.2,.07,.82),stone,.02)
        for x in (-.4,.4):
            box('Buttress end flute',(x,side*(D/2-.003),z),(.07,.006,.71),dark,.002)
            box('Buttress end rail',(x,side*(D/2-.003),z),(.018,.006,.66),gold,0)
tall=export_solid('SM_Aurelion_KIT_CoverButtress',0,tall_h)

low.hide_render=False; tall.hide_render=False; tall.location.x=2.8
scene.world=bpy.data.worlds.new('Cover studio'); scene.world.color=(.15,.15,.15)
for pos,energy,size in (((-4,-6,7),1500,6),((7,-2,7),1900,6),((3,6,9),2300,5)):
    d=bpy.data.lights.new('Cover review light','AREA'); d.energy=energy; d.size=size
    a=bpy.data.objects.new('Cover review light',d); scene.collection.objects.link(a); a.location=pos; a.rotation_euler=(Vector((1,0,2))-a.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Cover review'); camera=bpy.data.objects.new('Cover review',d); scene.collection.objects.link(camera)
camera.location=(-7,-10,6); camera.rotation_euler=(Vector((1,0,2.2))-camera.location).to_track_quat('-Z','Y').to_euler(); d.lens=47; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1600; scene.render.resolution_y=1200; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'Cover-family.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Fitted source candidate; combat and surface acceptance pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_CoverKit.blend')); bpy.ops.render.render(write_still=True)
