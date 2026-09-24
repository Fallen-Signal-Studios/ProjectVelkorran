"""Blender 4.5 source: double-faced 4 x 3 m Z12 upper berth sidelight.

Fits the two upper side-coffer cells flanking each saved 8 m armored view.
The saved lower service registers, full native wall collision and central rib
stay authoritative. The frame is a real open ring; pane is separate in Unreal.
"""
import json
from pathlib import Path

import bpy
from mathutils import Vector

source=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(source.read_text().split('# Four metre bay:')[0],str(source),'exec'))
ROOT=Path(__file__).resolve().parent/'Z12UpperSidelight'
ROOT.mkdir(exist_ok=True)
black=material('M_Aurelion_ViewBlackStone',(.022,.027,.032),.18,.30)
steel=material('M_Aurelion_DarkSteel',(.055,.061,.067),.72,.32)
light=material('M_Aurelion_ViewWarmConduit',(.76,.48,.19),.25,.20)
glass=material('M_Aurelion_ViewGlass',(.31,.39,.42),.06,.08)
light_bsdf=light.node_tree.nodes.get('Principled BSDF')
light_bsdf.inputs['Emission Color'].default_value=(.95,.68,.32,1)
light_bsdf.inputs['Emission Strength'].default_value=1.5
glass_bsdf=glass.node_tree.nodes.get('Principled BSDF')
glass_bsdf.inputs['Transmission Weight'].default_value=.92
glass_bsdf.inputs['Alpha'].default_value=.24
glass.surface_render_method='DITHERED'

# The load ring reaches the same 3.99 x 2.99 m cell boundary as the removed
# coffer. It is open from x +/-1.64 and z +/-1.15, leaving 3.28 x 2.30 m
# visible glass. All details repeat on the room and scenic faces.
for x in (-1.80,1.80):
    box('Black-stone through-stile',(x,0,0),(.39,.43,2.99),black,.014)
    for z in (-1.225,-.73,-.24,.25,.74,1.235):
        box('Dressed ivory stile course',(x,0,z),(.30,.47,.46),stone,.016)
    for face in (-1,1):
        y=face*.249
        box('Vertical inner reveal',(x-.17 if x<0 else x+.17,y,0),
            (.055,.018,2.75),dark,.004)
        box('Fine warm conductor',(x-.17 if x<0 else x+.17,y+face*.018,0),
            (.014,.012,2.61),gold,.002)
        for z in (-1.10,-.55,0,.55,1.10):
            box('Captive stile lock',(x,y+face*.015,z),(.16,.026,.055),steel,.006)
            box('Lock witness',(x,y+face*.033,z),(.075,.012,.012),gold,.002)

for z in (-1.34,1.34):
    box('Black-stone through-transom',(0,0,z),(3.99,.43,.31),black,.013)
    box('Honed ivory outer bearing',(0,0,z),(3.98,.48,.21),stone,.011)
    for face in (-1,1):
        y=face*.257
        box('Continuous dark glazing seal',(0,y,z),(3.35,.022,.047),dark,.004)
        box('Ancient gold edge index',(0,y+face*.014,z),(3.23,.010,.013),gold,.001)
        for i in range(7):
            x=(i-3)*.45
            box('Replaceable bearing key',(x,y+face*.023,z),(.31,.032,.11),steel,.007)
            box('Keyed ivory face',(x,y+face*.045,z),(.24,.018,.057),stone,.004)

for face in (-1,1):
    y=face*.25
    for sx in (-1,1):
        for sz in (-1,1):
            # Machined structural knees occupy only the aperture corners.
            x=sx*1.60
            z=sz*1.12
            box('Corner load block',(x,y,z),(.21,.13,.20),black,.012)
            box('Ivory corner cheek',(x-sx*.13,y+face*.073,z-sz*.13),
                (.27,.045,.11),stone,.012)
            box('Conductor termination',(x-sx*.11,y+face*.102,z-sz*.12),
                (.13,.018,.014),light,.003)
            for dx in (-.045,.045):
                box('Captive corner fastener',(x+dx,y+face*.117,z),
                    (.018,.012,.018),gold,.002)
    # The sill register is a small manufactured interface, not an objective UI.
    for i in range(9):
        x=(i-4)*.24
        box('Sill service vent',(x,y+face*.015,-1.230),(.11,.018,.023),dark,.002)
        if i%2==0:
            box('Vent gold index',(x,y+face*.029,-1.216),(.036,.008,.008),gold,.001)

frame=export('SM_Aurelion_KIT_Z12UpperSidelightFrame_4x3',[3.99,.50,2.99])
manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in frame.dimensions],
                    collision='None: visual ring over retained native wall collision',
                    preserve_fallback_geometry=True,position_precision=10)

# Physically separate translucent leaf. The retained native wall blocks play,
# and this pane never carries collision, shadow or Nanite.
box('Sealed glass leaf',(0,0,0),(3.28,.025,2.30),glass,.003)
pane=export('SM_Aurelion_KIT_Z12UpperSidelightPane_4x3',[3.28,.025,2.30])
manifest[-1].update(nominal_dimensions_m=[round(v,5) for v in pane.dimensions],
                    collision='None: transparent visual over retained native wall',
                    nanite_enabled=False)

(ROOT/'manifest.json').write_text(json.dumps(dict(
    source='Blender 4.5 editable procedural kit',
    design_reference='Docs/ArtReferences/AurelionArchitecture-2026-09-24/Z12-Upper-Sidelight.png',
    modules=manifest,
    placement=dict(cell_size_m=[3.99,.50,2.99],
                   upper_cell_centers_y=[46900,48100],
                   wall_x=[-2100,2100],center_z=450,
                   lower_service_registers_unchanged=True,
                   native_wall_collision_unchanged=True),
),indent=2),encoding='utf-8')

scene.world=bpy.data.worlds.new('Z12 sidelight studio')
scene.world.color=(.022,.029,.041)
for pos,power,size,color in [
    ((-5,-6,6),1850,5,(1,.82,.63)),
    ((5,4,4),1650,5,(.72,.82,1)),
]:
    bpy.ops.object.light_add(type='AREA',location=pos)
    lamp=bpy.context.object
    lamp.data.energy,lamp.data.size,lamp.data.color=power,size,color
    lamp.rotation_euler=(Vector((0,0,0))-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(5.2,-8.5,3.4))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type,camera.data.ortho_scale='ORTHO',5.6
scene.camera=camera
scene.render.engine='CYCLES'
scene.cycles.samples=32
scene.cycles.use_denoising=True
scene.render.resolution_x,scene.render.resolution_y=1400,1000
scene.render.filepath=str(ROOT/'upper-sidelight-source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Upper-Sidelight.blend'))
bpy.ops.render.render(write_still=True)
print('Z12_UPPER_SIDELIGHT_SOURCE_PASS')
