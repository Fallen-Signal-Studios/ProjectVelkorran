"""One-metre ceremonial assent console, centered in its measured visual envelope."""
from pathlib import Path
from mathutils import Matrix
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z10AssentConsole';ROOT.mkdir(exist_ok=True)
lens=material('M_Aurelion_UplightLens',(.92,.85,.70),0,.22)
p=lens.node_tree.nodes.get('Principled BSDF');p.inputs['Emission Color'].default_value=(.92,.85,.70,1);p.inputs['Emission Strength'].default_value=2
box('Dressed stone footing',(0,0,-.555),(1,.7,.09),stone,.012)
box('Inset footing moulding',(0,0,-.493),(.94,.64,.034),gold,.005)
box('Recessed structural pedestal',(0,.06,-.115),(.72,.43,.72),dark,.012)
for x in (-.40,.40):
    box('Fluted stone pier',(x,0,-.13),(.17,.58,.71),stone,.012)
    box('Pier crown',(x,0,.245),(.19,.62,.055),stone,.008)
    box('Pier gold fillet',(x,-.3,-.12),(.02,.012,.59),gold,.002)
    for side in (-1,1):
        box('Pier shadow flute',(x+side*.049,-.292,-.12),(.012,.012,.58),dark,.002)
box('Central service coffer',(0,-.163,-.13),(.54,.016,.58),stone,.008)
for z in (-.3,-.23,-.16,-.09,-.02):box('Recessed service register',(0,-.174,z),(.30,.008,.015),dark,.002)
path('Coffer gold arch',[(-.24,-.18,-.39),(-.24,-.18,.04),(0,-.18,.14),(.24,-.18,.04),(.24,-.18,-.39)],.018,.009,gold,.002)
# The work surface slopes upward away from the user, retaining the full 1.2m envelope.
theta=math.atan(.32)
verts=[(x,y,z+.32*y) for z in (.30,.48) for y in (-.35,.35) for x in (-.5,.5)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
mesh=bpy.data.meshes.new('Inclined stone worktop');mesh.from_pydata(verts,[],faces);mesh.update()
o=bpy.data.objects.new('Inclined stone worktop',mesh);scene.collection.objects.link(o);finish(o,stone,.007)
def field(name,x,y,sx,sy,mat,z=.487,depth=.004):
    o=box(name,(x,y,z+.32*y),(sx,sy,depth),mat,.001);o.rotation_euler.x=theta;return o
field('Recessed optical bed',0,0,.87,.56,dark)
for x in (-.44,.44):field('Gold optical border',x,0,.012,.57,gold,.489)
for y in (-.28,.28):field('Gold optical border',0,y,.88,.012,gold,.489)
for x in (-.285,0,.285):
    field('Inset ceramic touch plate',x,-.06,.21,.29,stone,.492)
    field('Touch plate horizontal glyph',x,-.06,.09,.012,gold,.496)
    field('Touch plate vertical glyph',x,-.06,.012,.12,gold,.496)
    for i in range(4):field('Status register',x-.065+i*.043,.17,.025,.032,lens,.492)
for x in (-.34,.34):
    for z in (-.32,-.05,.15):box('Rear service fastener',(x,.281,z),(.032,.018,.032),gold,.004)
# FBX handedness reflects Y in Unreal; face the lower-Y player approach.
for piece in parts:piece.matrix_world=Matrix.Diagonal((1,-1,1,1))@piece.matrix_world
console=export('SM_Aurelion_KIT_Z10AssentConsole',[1,.7,1.2])
manifest[-1].update(nominal_dimensions_m=list(console.dimensions),position_precision=10,preserve_fallback_geometry=True,collision='None: native assent interface collision remains authoritative')
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Assent console studio');scene.world.color=(.15,.15,.15)
target=Vector((0,0,0))
for pos,power in [((-2,-3,3),450),((2,2,3),350)]:
    bpy.ops.object.light_add(type='AREA',location=(pos[0],-pos[1],pos[2]));o=bpy.context.object;o.data.energy=power;o.data.size=3;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(1.5,2,1.4));o=bpy.context.object;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=1.9;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1100;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'assent-console.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Assent-Console.blend'));bpy.ops.render.render(write_still=True)
