"""Custom Aurelion rescue carrier: centred, metre-scale hull, nacelle and bridge.

Three authored silhouettes replace the eight journal-owned graybox presentations.
The carrier is scenery; no collision hulls or destruction are authored here.
"""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'CarrierKit'; ROOT.mkdir(exist_ok=True)
base_box=box
def box(name,loc,size,mat=stone,bevel=.025):
    return base_box(name,loc,size,mat,min(bevel,min(size)*.18))

def loft(name,stations,mat,bevel=.04):
    # Each station is longitudinal Y, half-width X and half-height Z.
    verts=[]
    for y,w,h in stations:
        verts.extend([(x*w,y,z*h) for x,z in [(-.76,-1),(.76,-1),(1,-.48),(1,.48),(.76,1),(-.76,1),(-1,.48),(-1,-.48)]])
    faces=[tuple(reversed(range(8)))]
    for j in range(len(stations)-1):
        for i in range(8): faces.append((j*8+i,j*8+(i+1)%8,(j+1)*8+(i+1)%8,(j+1)*8+i))
    faces.append(tuple(range((len(stations)-1)*8,len(stations)*8)))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o)
    return finish(o,mat,bevel)

def export_part(name,dimensions):
    o=export(name,dimensions);o.hide_render=True
    manifest[-1]['nominal_dimensions_m']=list(o.dimensions)
    manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,
        collision='None; journal-owned scenery, excluded from navigation',pivot='Centre; Blender -Y is UE forward +Y')
    return o

# A long faceted pressure body, chamfered prow and stern, inset service spine.
loft('Carrier pressure body',[(-52.5,12,3.8),(-44,20,6),(-32,21.8,6),(32,21.8,6),(45,19,5.5),(52.5,14,3.6)],dark)
for y in range(-36,37,8):
    for side in (-1,1):
        box('Dorsal ceramic armour',(side*8.2,y,6.07),(15.9,7.78,.14),stone,.04)
        box('Shoulder conductor',(side*15.85,y,6.16),(.075,7.32,.035),gold,.005)
        for offset in (12.2,):
            x=side*offset
            box('Recessed dorsal service field',(x,y,6.16),(6.9,5.5,.04),dark,.012)
            box('Removable ceramic service panel',(x,y,6.205),(6.55,5.15,.06),stone,.016)
            for dy in (-2.52,2.52):box('Service-panel conductor',(x,y+dy,6.247),(6.4,.055,.025),gold,.004)
            for dx in (-3.18,3.18):box('Service-panel edge',(x+dx,y,6.247),(.055,4.85,.025),gold,.004)
            for dx in (-2.85,2.85):
                for dy in (-2.15,2.15):box('Captive panel latch',(x+dx,y+dy,6.252),(.16,.32,.03),dark,.009)
            # Asymmetric service channels distinguish access lids from paving.
            for j in range(4):box('Dorsal recessed intake',(x+side*1.9,y-1.25+j*.8,6.245),(.8,.11,.02),dark,.004)
        # Rectangular service hatches sit flush in the broad side of the chamfered shell.
        box('Side ceramic field',(side*21.82,y,0),(.10,7.78,5.5),stone,.022)
        box('Side inset register',(side*21.88,y,.35),(.04,5.8,2.1),dark,.008)
        for k in range(7):
            box('Service heat exchanger',(side*21.92,y-2.55+k*.85,.35),(.045,.11,1.72),gold,.01)
        for dy in (-3.55,3.55):
            box('Access latch',(side*21.92,y+dy,-1.8),(.055,.22,.55),dark,.018)
        box('Ventral ceramic armour',(side*7.8,y,-6.08),(15.1,7.78,.16),stone,.025)
    box('Recessed dorsal mechanism',(0,y,5.99),(.48,7.65,.07),dark,.01)
    for x in (-.18,.18):box('Dorsal guide',(x,y,6.035),(.035,7.4,.025),gold,.003)
# A low tapered dorsal machinery house breaks the former flat-deck silhouette.
dorsal=loft('Dorsal systems house',[(-28,1.8,.45),(-20,5,1.5),(15,5,1.5),(25,2.2,.45)],dark,.045)
dorsal.location.z=6.2
for y in (-15,-5,5):
    box('Dorsal ceramic roof',(0,y,7.72),(7.45,9.72,.12),stone,.03)
    for side in (-1,1):
        box('Systems-house side panel',(side*5,y,6.2),(.12,9.65,1.3),stone,.022)
        box('Systems-house recessed vent',(side*5.08,y,6.2),(.035,8.5,.75),dark,.008)
        for k in range(12):box('Systems-house vent fin',(side*5.11,y-3.85+k*.7,6.2),(.035,.10,.6),gold,.008)
        box('Systems-house roof conductor',(side*3.3,y,7.8),(.08,9.35,.03),gold,.005)
# Tapered end plating continues the profile instead of a floor-tile cuboid.
loft('Prow ceramic shell',[(-52.49,12.015,3.815),(-44,20.015,6.015),(-40.15,20.59,6.015)],stone,.015)
loft('Stern ceramic shell',[(40.15,20.06,5.702),(45,19.015,5.515),(52.49,14.022,3.62)],stone,.015)
# Midship cargo docking collars and three differently sized maintenance recesses.
for side in (-1,1):
    for y in (-23,0,23):
        box('Docking collar shadow',(side*21.9,y,-.1),(.10,4.3,4.4),dark,.03)
        for z in (-2.1,1.9):box('Docking sill',(side*21.98,y,z),(.04,4.05,.10),gold,.012)
        for dy in (-1.97,1.97):box('Docking jamb',(side*21.98,y+dy,-.1),(.04,.10,3.85),gold,.012)
        for k in range(5):box('Recessed docking shutter',(side*21.97,y,-1.6+k*.75),(.045,3.45,.62),stone,.025)
hull=export_part('SM_Aurelion_KIT_CarrierHull',[44,105,12.1])

# Reusable nacelle: protected ceramic armour, exposed radiator bank and recessed exhaust.
loft('Nacelle body',[(-30,3.2,2.7),(-25,5.8,3.8),(23,5.8,3.8),(30,4.1,3)],dark)
for y in range(-20,21,5):
    box('Nacelle crown',(0,y,3.78),(8.65,4.8,.14),stone,.035)
    box('Nacelle keel',(0,y,-3.77),(8.4,4.8,.16),stone,.035)
    for side in (-1,1):
        box('Nacelle side shield',(side*5.8,y,0),(.14,4.8,3.5),stone,.025)
        box('Nacelle recessed bank',(side*5.89,y,0),(.035,4.1,2.5),dark,.008)
        for k in range(8):box('Radiator fin',(side*5.93,y-1.8+k*.51,0),(.035,.10,2.2),gold,.012)
        box('Nacelle conductor',(side*3.8,y,3.88),(.085,4.4,.03),gold,.005)
for y in (-27,27):
    # Ring lies in X/Z; +Y face is aft in Blender and UE -Y.
    for radius,width,mat in ((2.55,.30,stone),(2.23,.09,gold),(1.9,.12,dark)):
        ring('Recessed thrust iris',0,30.05 if y>0 else -30.08,0,radius,width,mat)
    for k in range(12):
        a=k*math.tau/12
        o=box('Iris vane',(math.sin(a)*1.4,30.08 if y>0 else -30.09,math.cos(a)*1.4),(.18,.06,.72),gold,.014);o.rotation_euler[1]=a
engine=export_part('SM_Aurelion_KIT_CarrierNacelle',[12,60,8])

# Broad bridge/cockpit module: stepped ceramic brow with dark panoramic glazing.
loft('Bridge pressure body',[(-10,9,2.6),(-6,15.5,4.6),(5,17.3,4.6),(10,14,3.8)],dark)
for side in (-1,1):
    for y in (-4,0,4):
        box('Bridge roof plate',(side*5.9,y,4.6),(11.35,3.83,.16),stone,.04)
        box('Bridge roof inlay',(side*10.8,y,4.71),(.09,3.45,.03),gold,.006)
    o=box('Bridge forward glazing',(side*3.3,-8,3.67),(6.1,3.35,.07),dark,.025);o.rotation_euler[0]=math.atan(.5)
    for x in (side*.25,side*3.3,side*6.35):
        o=box('Glazing mullion',(x,-8,3.72),(.075,3.35,.04),gold,.006);o.rotation_euler[0]=math.atan(.5)
    box('Bridge lower fairing',(side*3.3,-9.58,2.96),(6.15,.14,.12),stone,.018)
bridge=export_part('SM_Aurelion_KIT_CarrierBridge',[35,20,10])
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))

# Assembly studio: source coordinates correspond to stable presentation offsets.
hull.hide_render=False;engine.hide_render=False;bridge.hide_render=False
engine.location=(26,-5,2.5)
twin=engine.copy();twin.data=engine.data;scene.collection.objects.link(twin);twin.location=(-26,-5,2.5)
bridge.location=(0,-53,2.5)
scene.world=bpy.data.worlds.new('Carrier studio');scene.world.color=(.2,.2,.2)
target=Vector((0,-5,0))
for pos,power,size in [((30,-70,75),180000,45),((-70,-10,35),130000,45),((30,70,60),200000,40)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(-100,-140,100));camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=145;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1800;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'carrier-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Rescue-Carrier.blend'));bpy.ops.render.render(write_still=True)
