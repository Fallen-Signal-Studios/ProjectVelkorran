"""Blender 4.5 source for the single Z11 observation table and six matching chairs.

The visual modules fit the existing hidden Z11 table/chair collision envelopes.
"""
import math
from pathlib import Path

helper = Path(__file__).with_name("build_architecture_kit.py")
exec(compile(helper.read_text().split("# Four metre bay:")[0], str(helper), "exec"))
ROOT = Path(__file__).resolve().parent / "Z11ObservationFurniture"
ROOT.mkdir(exist_ok=True)
black = material("M_Aurelion_BlackStone", (.022, .025, .030), .06, .55)
black.node_tree.nodes.get("Principled BSDF").inputs["Coat Weight"].default_value = .04

# Table: one physical piece in the existing 5 x 2 x .8 m hidden collider.
# Three fitted stone leaves retain the sense of one continuous surface while
# allowing fine age-polished joints to read in the conversation close-ups.
box("Under-slab dark shadow", (0, 0, .707), (4.81, 1.72, .115), dark, .027)
for x in (-1.64, 0, 1.64):
    box("Dressed tabletop leaf", (x, 0, .767), (1.615, 1.91, .066), black, .026)
    for y in (-.927, .927):
        box("Continuous protective arris", (x, y, .731), (1.602, .016, .014), gold, .003)
    for offset in (-.56, .56):
        box("Faint rubbed edge witness", (x + offset, -.953, .754),
            (.027, .008, .013), gold, .003)
for x in (-2.435, 2.435):
    box("Table end stone cap", (x, 0, .744), (.07, 1.86, .075), black, .014)
    box("Narrow cap reveal", (x * .986, 0, .704), (.018, 1.75, .018), gold, .004)
# The restrained ivory rim gives the conversation surface a cut-stone edge
# without making the table a glowing altar. All added details remain inside
# the measured 5 x 2 x .8 m fallback collision envelope.
for y in (-.951, .951):
    box("Ivory protective long arris", (0, y, .778), (4.79, .018, .030), stone, .004)
    for x in (-1.64, 0, 1.64):
        box("Seating datum key", (x, y * .970, .7995), (.075, .018, .002), gold, 0)
for x in (-2.452, 2.452):
    box("Ivory end arris", (x, 0, .777), (.018, 1.84, .030), stone, .004)
for radius in (.215, .284):
    bpy.ops.mesh.primitive_torus_add(major_segments=64, minor_segments=8,
        location=(0, 0, .7965), major_radius=radius, minor_radius=.004)
    bpy.context.object.name="Inlaid witness circumference"
    finish(bpy.context.object, gold, 0)
box("Inlaid contrary line", (0, 0, .7995), (1.10, .006, .002), gold, 0)
for y in (-.89, .89):
    box("Continuous lower edge", (0, y, .662), (4.77, .065, .044), black, .009)
    box("Edge light channel", (0, y * 1.038, .660), (4.70, .014, .012), gold, .003)
for x in (-1.56, 1.56):
    # Twin pedestal support leaves generous knee room at all six seats.
    for y in (-.54, .54):
        box("Pedestal foot stone", (x, y, .050), (.55, .55, .10), black, .014)
        box("Pedestal foot shoe", (x, y, .105), (.43, .46, .020), gold, .004)
        box("Angled pedestal bearer", (x, y, .385), (.32, .31, .54), black, .022)
        for side in (-1, 1):
            box("Pedestal dressed side cheek", (x+side*.166, y, .379),
                (.010, .048, .475), stone, .003)
        box("Pedestal service contact", (x, y-.160, .493),
            (.184, .006, .010), gold, .002)
    box("Pedestal cross member", (x, 0, .625), (.39, 1.40, .07), dark, .012)
    for y in (-.68, .68):
        box("Pedestal socket collar", (x, y, .619), (.45, .07, .035), gold, .005)
for x in (-2.36, 2.36):
    for y in (-.74, .74):
        box("Hand-polished corner pin", (x, y, .797), (.035, .035, .003), gold, .001)
table = export("SM_Aurelion_KIT_Z11ConversationTable", [4.95, 1.92, .801])
manifest[-1].update(
    nominal_dimensions_m=[round(v, 6) for v in table.dimensions],
    position_precision=10,
    preserve_fallback_geometry=True,
    collision="None: native hidden Z11_Conversation_Table remains the physical owner",
)

# Chair faces +Y; the north row rotates 180 degrees in the level.
for x in (-.265, .265):
    for y in (-.285, .285):
        box("Chair floor shoe", (x, y, .045), (.085, .085, .09), black, .010)
        box("Leg gold bearing", (x, y, .105), (.055, .055, .045), gold, .007)
        box("Slender dressed leg", (x, y, .258), (.067, .070, .27), black, .014)
    box("Chair side rail", (x, 0, .414), (.067, .62, .070), dark, .009)
box("Seat structural pan", (0, 0, .443), (.637, .65, .091), black, .024)
box("Seat relief bevel", (0, .008, .497), (.590, .605, .027), gold, .008)
box("Seat polished contact", (0, .011, .516), (.560, .579, .050), black, .024)
for y in (-.253, .253):
    box("Seat fine horizontal witness", (0, y, .543), (.492, .008, .003), gold, .001)
for x in (-.309, .309):
    box("Ivory seat cheek", (x, 0, .472), (.013, .580, .055), stone, .004)
    for y in (-.226, .226):
        box("Seat captive gold pin", (x*1.033, y, .465),
            (.006, .013, .013), gold, .001)
# Angled back with broad human contact surface and two nested frames.
back = box("Chair back structural shell", (0, -.340, .831), (.622, .092, .700), black, .015)
back.rotation_euler.x = math.radians(-9)
back = box("Chair back inset", (0, -.293, .832), (.538, .036, .578), dark, .006)
back.rotation_euler.x = math.radians(-9)
back = box("Chair back contact slab", (0, -.269, .835), (.484, .033, .526), black, .006)
back.rotation_euler.x = math.radians(-9)
for x in (-.275, .275):
    box("Back seam upright", (x, -.327, .832), (.019, .022, .605), gold, .004).rotation_euler.x = math.radians(-9)
for x in (-.310, .310):
    box("Dressed ivory back cheek", (x, -.350, .828),
        (.014, .035, .670), stone, .004).rotation_euler.x = math.radians(-9)
box("Back shoulder cap", (0, -.398, 1.169), (.616, .104, .047), black, .014)
box("Age-cut ivory shoulder arris", (0, -.399, 1.186), (.570, .030, .012), stone, .003)
for x in (-.29, .29):
    box("Arm stone root", (x, -.095, .584), (.070, .455, .064), black, .017)
    box("Arm gold separation", (x, -.095, .618), (.041, .400, .011), gold, .003)
    box("Hand contact", (x, -.055, .635), (.055, .365, .029), black, .010)
    box("Functional arm end pin", (x, .120, .634), (.016, .013, .012), gold, .002)
chair = export("SM_Aurelion_KIT_Z11ConversationChair", [.65, .78, 1.2])
manifest[-1].update(
    nominal_dimensions_m=[round(v, 6) for v in chair.dimensions],
    position_precision=10,
    preserve_fallback_geometry=True,
    collision="None: native hidden Z11_Chair seat/back remain the physical owners",
)

(ROOT / "manifest.json").write_text(json.dumps(dict(
    reference="Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z11-Conversation-Furniture.png",
    placement_status="Refined versioned visuals for the existing seven Z11 furniture actors; native fallback geometry owns collision",
    modules=manifest), indent=2))

# Assemble the six-chair arrangement inside the editable source for art review.
for x in (-2.5, 0, 2.5):
    for y, angle in ((-2.2, 0), (2.2, 180)):
        duplicate = chair.copy()
        duplicate.data = chair.data
        scene.collection.objects.link(duplicate)
        duplicate.location = (x, y, 0)
        duplicate.rotation_euler.z = math.radians(angle)
chair.hide_render = True
scene.world = bpy.data.worlds.new("Observation studio")
scene.world.color = (.06, .06, .065)
target = Vector((0, 0, .6))
for position, power, size, color in [
    ((-6, -4, 6), 900, 4, (1.0, .72, .53)),
    ((6, 4, 5), 800, 4, (.64, .78, 1.0)),
    ((0, 1, 7), 450, 5, (1.0, .95, .79)),
]:
    bpy.ops.object.light_add(type="AREA", location=position)
    lamp = bpy.context.object
    lamp.data.energy = power
    lamp.data.size = size
    lamp.data.color = color
    lamp.rotation_euler = (target - lamp.location).to_track_quat("-Z", "Y").to_euler()
bpy.ops.object.camera_add(location=(-6.5, -7.5, 4.2))
camera = bpy.context.object
camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 9
scene.camera = camera
scene.render.engine = "CYCLES"
scene.cycles.samples = 48
scene.cycles.use_denoising = True
scene.render.resolution_x = 1600
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
scene.render.filepath = str(ROOT / "observation-furniture.png")
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / "Aurelion-Observation-Furniture.blend"))
bpy.ops.render.render(write_still=True)
