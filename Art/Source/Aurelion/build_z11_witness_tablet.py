"""One compact Z11 Aurelion witness tablet visual for five retained scene actors.

Source coordinates are metres, Z-up, bottom pivot. The native request actor's
Body remains the only collider and its original Visual component transform is
retained. This is a visual mesh, not a new interaction or quest station.
"""
from pathlib import Path
import json
import math

import bpy
from mathutils import Vector

source = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0], str(source), 'exec'))
ROOT = Path(__file__).resolve().parent / 'Z11WitnessTablet'
ROOT.mkdir(exist_ok=True)

black = material('M_Aurelion_ObservationBlackStone', (.018,.022,.029), .10, .29)
screen = material('M_Aurelion_QuietDisplay', (.075,.17,.19), .12, .23)
screen.node_tree.nodes.get('Principled BSDF').inputs['Emission Color'].default_value=(.35,.72,.72,1)
screen.node_tree.nodes.get('Principled BSDF').inputs['Emission Strength'].default_value=.35

base_box = box
def box(name, loc, size, mat=stone, bevel=.006):
    return base_box(name, loc, size, mat, min(bevel, min(size)*.4))

def profile(name, levels, mat, bevel=.006):
    """Closed fitted rectilinear shell with controlled tapers, not stacked cubes."""
    verts=[]
    for z,w,d,cy in levels:
        for x,y in ((-w/2,-d/2),(w/2,-d/2),(w/2,d/2),(-w/2,d/2)):
            verts.append((x,y+cy,z))
    faces=[(3,2,1,0)]
    for k in range(len(levels)-1):
        a=4*k;b=4*(k+1)
        for i in range(4):
            j=(i+1)%4
            faces.append((a+i,a+j,b+j,b+i))
    top=4*(len(levels)-1)
    faces.append((top,top+1,top+2,top+3))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj)
    return finish(obj,mat,bevel)

# All dimensions fit the existing 70 x 50 cm Body after the current 0.7454
# component scale. The existing mesh's world bottom is 20 cm, not map zero.
box('Foundation shadow shoe',(0,0,.021),(.78,.50,.042),dark,.008)
box('Fitted ivory foot',(0,0,.071),(.755,.475,.075),stone,.013)
box('Underfoot ancient gold datum',(0,-.241,.080),(.69,.008,.008),gold,.001)
profile('Polished black load core',[(.105,.66,.40,0),(.28,.58,.355,.013),
        (1.03,.48,.315,.032),(1.21,.54,.36,.036)],black,.012)
profile('Inset front register',[(.16,.50,.035,-.206),(.29,.43,.033,-.190),
        (.99,.34,.029,-.153),(1.13,.39,.033,-.159)],dark,.006)

# Ivory edge leaves and seam locks create an original silhouette without
# adding a large prop beyond the inherited collision/interaction envelope.
for side in (-1,1):
    for z,w,d,h in ((.26,.056,.085,.25),(.66,.044,.074,.55),(1.04,.045,.073,.30)):
        x=side*(.277 if z<.4 else .245 if z<.9 else .248)
        box('Age-cut ivory vertical leaf',(x,-.159,z),(w,d,h),stone,.009)
    for z in (.38,.83,1.11):
        x=side*(.271 if z<.5 else .234 if z<1 else .251)
        box('Captive black keyed seam',(x,-.205,z),(.051,.013,.014),dark,.0015)
        box('Gold service contact',(x,-.214,z),(.018,.007,.006),gold,.001)
    box('Inset side stone return',(side*.301,.027,.65),(.036,.24,.83),stone,.008)
    box('Side conductor shadow',(side*.322,.013,.65),(.008,.155,.79),dark,.001)
    box('Fine side gold conductor',(side*.328,.012,.65),(.004,.126,.75),gold,.001)

# Narrow front conductors form readable, functional lines from the touch face
# to the foot. These are precise channels, not decorative runes or heraldry.
for side in (-1,1):
    x=side*.115
    box('Front conductor reveal',(x,-.232,.65),(.022,.012,.73),dark,.001)
    box('Front conductor inlay',(x,-.240,.65),(.007,.006,.69),gold,.001)
    for z in (.33,.60,.87):
        box('Front conductor keyed contact',(x,-.245,z),(.024,.006,.008),gold,.001)

# Angled reading head: a compact captive black bezel with a sloped touch
# surface. Local -Y is the operable face; the source is reviewed after Unreal
# applies the five actors' existing -90-degree component rotations.
profile('Head cradle',[(1.155,.53,.38,.031),(1.25,.64,.46,-.008),
        (1.45,.66,.48,-.014),(1.515,.58,.43,.0)],black,.014)
for z,w,d in ((1.226,.61,.44),(1.475,.625,.445)):
    box('Head black compression ring',(0,-.011,z),(w,d,.022),black,.004)
bezel=box('Tilted black touch bezel',(0,-.269,1.352),(.555,.026,.250),black,.010)
bezel.rotation_euler[0]=math.radians(19)
glass=box('Recessed cool white display',(0,-.284,1.356),(.452,.008,.170),screen,.003)
glass.rotation_euler[0]=math.radians(19)
for side in (-1,1):
    box('Ivory display arris',(side*.256,-.285,1.355),(.017,.022,.205),stone,.003).rotation_euler[0]=math.radians(19)
    box('Top gold signal pin',(side*.203,-.300,1.455),(.020,.009,.010),gold,.001)
box('Minimal interaction datum',(0,-.294,1.356),(.155,.009,.008),gold,.001).rotation_euler[0]=math.radians(19)
for x in (-.113,-.075,.075,.113):
    box('Subtle status tick',(x,-.295,1.314),(.011,.009,.006),stone,.001).rotation_euler[0]=math.radians(19)

# Closed rear service spine, inspection keys and a stepped top cap remain
# convincing from the observation window and side aisles.
box('Rear black service plate',(0,.211,.714),(.43,.035,.93),black,.008)
for z in (.37,.62,.87,1.08):
    box('Rear captive ivory latch',(0,.236,z),(.24,.020,.033),stone,.004)
    for side in (-1,1):
        box('Rear gold latch pin',(side*.102,.250,z),(.011,.005,.011),gold,.001)
box('Head crown stone cap',(0,0,1.535),(.58,.424,.040),stone,.010)
box('Head crown gold witness line',(0,-.218,1.530),(.42,.009,.008),gold,.001)

# The first player-height Unreal preview showed that a full-width head became
# a bulky retail kiosk even inside the native collider. A narrower 70/75%
# horizontal fit keeps the authored height and reading plane but lets the
# table, chairs and stellar window own the room.
for obj in parts:
    obj.location.x *= .70
    obj.location.y *= .75
    obj.scale.x *= .70
    obj.scale.y *= .75
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)

asset='SM_Aurelion_KIT_Z11WitnessTablet'
module=export(asset,[.546,.444,1.555])
manifest[-1].update(
    collision='None: retained request actor Body owns all physical interaction and collision',
    nanite_enabled=False,
    original_actor_visual_scale=.7454,
    world_max_footprint_m=[round(.546*.7454,4),round(.436*.7454,4)],
    preserve_actor_and_component_transforms=True,
    protected_request_actor_count=5)
(ROOT/'manifest.json').write_text(json.dumps(dict(
    reference='Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z11-Witness-Tablet.png',
    survey='Saved/Validation/Aurelion/Z11WitnessTabletSurvey-20260924-0620/z11-witness-tablet-survey.json',
    placement_status='Fitted to five M13 Z11 request visuals; guarded editor save/reload and player-height render review passed. A fresh post-fit end-to-end interaction run remains open.',
    modules=manifest),indent=2),encoding='utf-8')

scene.world=bpy.data.worlds.new('Z11 witness tablet studio')
scene.world.color=(.065,.066,.073)
for pos,power,color in (((3,-4,5),950,(1,.72,.52)),((-3,2,3),700,(.55,.76,1)),((2,3,6),600,(1,.94,.81))):
    bpy.ops.object.light_add(type='AREA',location=pos)
    light=bpy.context.object;light.data.energy=power;light.data.size=2
    light.data.color=color
    light.rotation_euler=(Vector((0,0,.8))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2.5,-4.5,2.5))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,.85))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=2.25
scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1150;scene.render.resolution_y=1450
scene.render.filepath=str(ROOT/'tablet-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z11-Witness-Tablet.blend'))
bpy.ops.render.render(write_still=True)
print('Z11_WITNESS_TABLET_SOURCE_COMPLETE')
