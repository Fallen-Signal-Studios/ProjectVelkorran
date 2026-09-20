"""Crownmark chamber centerpiece: dressed stone, bronze cage and suspended seal.

Preview/source stage until the fitted Unreal replacement is reviewed.
"""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z10CrownmarkCore';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.72,.92,1),.05,.22)
shader=lens.node_tree.nodes.get('Principled BSDF')
shader.inputs['Emission Color'].default_value=(.72,.92,1,1)
shader.inputs['Emission Strength'].default_value=2

def drum(name,z,radius,height,mat,vertices=96):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=height,location=(0,0,z))
    o=bpy.context.object;o.name=name
    return finish(o,mat,.007)

for name,z,r,h,m in [
    ('Dressed circular footing',-.94,.66,.12,stone),
    ('Footing bronze fillet',-.865,.625,.025,gold),
    ('Recessed base register',-.80,.585,.10,dark),
    ('Seal pedestal',-.72,.61,.06,stone),
    ('Inner pedestal inlay',-.677,.47,.016,gold),
    ('Recessed crown register',.82,.535,.10,dark),
    ('Crown stone cornice',.90,.60,.06,stone),
    ('Crown bronze crest',.953,.56,.03,gold)]:
    drum(name,z,r,h,m)

# Six stone supports describe an open reliquary rather than a solid lab cylinder.
for i in range(6):
    angle=math.tau*i/6
    def orient(o):
        o.location=Vector((o.location.x*math.cos(angle)-o.location.y*math.sin(angle),
                           o.location.x*math.sin(angle)+o.location.y*math.cos(angle),o.location.z))
        o.rotation_euler.z=angle
    for radius,width,depth,height,z,mat,name in [
        (.51,.13,.14,1.45,.055,stone,'Fluted reliquary pier'),
        (.59,.03,.016,1.26,.055,gold,'Pier bronze arris'),
        (.58,.074,.013,1.10,.055,dark,'Pier recessed flute'),
        (.53,.19,.19,.075,-.61,gold,'Pier foot collar'),
        (.53,.19,.19,.075,.75,gold,'Pier crown collar')]:
        orient(box(name,(0,radius,z),(width,depth,height),mat,.004))
    for z in (-.49,-.25,0,.25,.50):
        orient(box('Pier cut register',(0,.592,z),(.066,.013,.012),gold,.001))
    orient(box('Base illuminated register',(0,.588,-.805),(.16,.02,.024),lens,.002))

# A double broken seal, mounted on a narrow central plinth, reads from the approach.
seal_start=len(parts)
drum('Seal stem socket',-.59,.15,.14,gold,48)
box('Seal supporting spine',(0,.06,-.29),(.07,.075,.58),gold,.004)
for radius,width,mat in [(.43,.043,gold),(.365,.022,stone)]:
    for begin,end in [(18,162),(198,342)]:
        points=[(radius*math.sin(math.radians(a)),0,.19+radius*math.cos(math.radians(a)))
                for a in range(begin,end+1,3)]
        path('Open crownmark seal',points,width,.035,mat,.003)
path('Central crownmark', [(-.24,-.035,.08),(-.19,-.035,.33),(0,-.035,.12),(.19,-.035,.33),(.24,-.035,.08)],.035,.03,gold,.003)
path('Crownmark lower point',[(-.19,-.035,.015),(0,-.035,-.15),(.19,-.035,.015)],.024,.03,gold,.002)
for x in (-.11,0,.11):
    box('Seal luminous point',(x,-.045,-.015),(.024,.015,.024),lens,.003)
# The existing chamber statue occupies the central cavity. Place the seal behind
# it (FBX reflects Y in Unreal), not through the figure's torso and legs.
for piece in parts[seal_start:]:piece.location.y-=.4

core=export('SM_Aurelion_KIT_Z10CrownmarkCore',[1.32,1.32,1.968])
manifest[-1].update(nominal_dimensions_m=list(core.dimensions),position_precision=10,
    preserve_fallback_geometry=True,collision='None: retain native Crownmark interaction/collision')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Crownmark studio');scene.world.color=(.15,.15,.15)
target=Vector((0,0,0))
for position,power in [((-2,-3,3),500),((2,1,3),400)]:
    bpy.ops.object.light_add(type='AREA',location=position);o=bpy.context.object
    o.data.energy=power;o.data.size=3;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(1.8,-3,1.5));o=bpy.context.object
o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=2.7;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1100;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'crownmark-core.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Crownmark-Core.blend'))
bpy.ops.render.render(write_still=True)
