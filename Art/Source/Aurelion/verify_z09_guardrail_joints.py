"""Ray-test exported guardrail joints and sightlines; visual mesh only."""
from pathlib import Path
import bpy,json
from mathutils import Vector
root=Path(__file__).resolve().parent/'Z09RailingKit'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'SM_Aurelion_KIT_Z09Guardrail.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH')
w=4.75
def hit(x,z):return o.ray_cast(Vector((x,-1,z)),Vector((0,1,0)),distance=2)[0]
bays=[-w/2+(bay+.5)*w/4 for bay in range(4)]
# Every baluster is continuous from the tie into the grip; the former 3 cm gap sat at 1.23-1.26 m.
heights=[round(.23+i*.01,2) for i in range(103)]
for x in bays:
    for bx in (x-.29,x,x+.29):
        gaps=[z for z in heights if not hit(bx,z)];assert not gaps,(bx,gaps)
# Register legs end inside the outer balusters instead of hanging past them.
for x in bays:
    for side in (-1,1):
        assert hit(x+side*.26,.9+(.03/.145)*.18),(x,side,'register leg missing')
        assert not hit(x+side*.33,.88),(x,side,'register extends past baluster')
# Open metalwork: mid-gaps stay clear below the register and above the tie.
clear=[dict(x=round(x+s*.145,4),z=z) for x in bays for s in (-1,1) for z in (.35,.55,.75)]
assert not any(hit(r['x'],r['z']) for r in clear)
(root/'joint-verification.json').write_text(json.dumps(dict(status='passed',continuous_balusters=12,samples_per_baluster=len(heights),seated_register_legs=8,clear_sightline_samples=len(clear),qualification='Exported visual mesh rays only; no collision is authored and native barriers remain authoritative.'),indent=2))
print('Z09_GUARDRAIL_JOINTS_PASS')
