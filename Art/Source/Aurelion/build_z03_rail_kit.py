"""Continuous sensor-route rails: upright posts over a 3m/12m slope."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z03RailKit';ROOT.mkdir(exist_ok=True)

def beam(name,w,y,depth,z,height,rise,mat):
    # Sheared prism keeps end cuts vertical and nominal grip height exact.
    verts=[(x,yy,zz+lift) for x,lift in ((-w/2,0),(w/2,rise)) for yy in (y-depth/2,y+depth/2) for zz in (z,z+height)]
    faces=[(0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3)]
    m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
    o=bpy.data.objects.new(name,m);scene.collection.objects.link(o);finish(o,mat,min(.003,depth*.2,height*.2))

def rail(name,w,d,rise,spans):
    h=1.3
    beam('Continuous dark grip',w,0,d*.62,h-.04,.04,rise,dark)
    beam('Sloped lower tie',w,0,.048,.13,.04,rise,dark)
    for side in (-1,1):
        beam('Fine grip register',w-.018,side*(d*.31+.002),.003,h-.023,.007,rise*(w-.018)/w,gold)
    for i in range(spans+1):
        x=-w/2+.08+(w-.16)*i/spans;floor=rise*(x+w/2)/w
        for z,ww,dd,hh in ((.025,.16,d,.05),(.076,.12,d*.85,.04),(.64,.075,d*.58,1.08),(1.21,.12,d*.86,.05)):
            box('Dressed upright post',(x,0,floor+z),(ww,dd,hh),stone,.004)
        for side in (-1,1):
            box('Post flute',(x,side*(d*.29+.002),floor+.64),(.027,.009,.99),dark,.001)
            box('Post conductor',(x,side*(d*.29+.008),floor+.64),(.006,.004,.96),gold,.0007)
            for z in (.08,1.21):box('Mounting fastener',(x,side*(d*.43+.003),floor+z),(.018,.006,.012),gold,.001)
    count=round(w/.16)
    for i in range(1,count):
        x=-w/2+i*w/count
        if any(abs(x-(-w/2+.08+(w-.16)*j/spans))<.08 for j in range(spans+1)):continue
        floor=rise*(x+w/2)/w
        box('Vertical infill',(x,0,floor+.71),(.019,.03,1.06),dark,.002)
        for z in (.19,1.20):box('Infill ferrule',(x,0,floor+z),(.025,.036,.027),dark,.002)
    o=export(name,[w,d,h+rise]);manifest[-1]['nominal_dimensions_m']=list(o.dimensions)
    manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='None; native route guards remain authoritative')
    o.hide_render=True;return o

slope=rail('SM_Aurelion_KIT_Z03SlopeRail',12,.22,3,6)
deck=rail('SM_Aurelion_KIT_Z03DeckRail',12,.20,0,6)
bridge=rail('SM_Aurelion_KIT_Z03BridgeRail',4.2,.22,0,2)
placements=[]
for x,ids in ((7788,list(range(0,6))),(7412,list(range(6,12)))):
    placements.append(dict(asset=slope.name,location_cm=[x,-18500,0],yaw=90,original_indices=ids))
for x,ids in ((7788,list(range(12,18))),(7412,list(range(18,24)))):
    placements.append(dict(asset=slope.name,location_cm=[x,-16100,0],yaw=-90,original_indices=ids))
for x,ids in ((7410,[24,25,26]),(7790,[27,28,29])):
    placements.append(dict(asset=deck.name,location_cm=[x,-17300,300],yaw=90,original_indices=ids))
for x,start in ((7288,30),(6712,40)):
    for j in range(5):placements.append(dict(asset=bridge.name,location_cm=[x,-14790+j*420,0],yaw=90,original_indices=[start+j*2,start+j*2+1]))
assert sorted(i for r in placements for i in r['original_indices'])==list(range(50))
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements,
    qualification='Continuous 12m ramp and landing runs with vertical posts, 4.2m bridge modules, native collision preserved. Engine visual and route review required.'),indent=2))
slope.hide_render=False;bridge.hide_render=False;bridge.location.y=-1.4
scene.world=bpy.data.worlds.new('Sensor rail studio');scene.world.color=(.17,.17,.17)
for pos,power,size in [((1,-5,6),1700,5),((-4,2,5),1400,4),((6,3,6),1800,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((0,0,1.8))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(12,-20,10));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,1.8))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=14;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'rail-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z03-Rails.blend'));bpy.ops.render.render(write_still=True)
