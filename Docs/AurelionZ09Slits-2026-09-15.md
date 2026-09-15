# Replace suspended strips with synchronized wall fissures

The three decorative blue strips suspended over the gallery are replaced by
vertical Eclipse fissures on three adjacent inset panels of the east wall.
They repeat one authored form deliberately, following the August TDD's Eclipse
language of synchronization and repeated forms becoming too exact. This is a
static visual interpretation; it does not implement a new synchronization
mechanic or narrative response.

The custom source derives from the existing Eclipse fracture construction and
bakes proportions before export. The mesh is about 0.647 x 0.026 x 2.782 m,
with 16,520 source triangles, two UV layers and three materials. The violet
segments are wider than the small scar version to remain legible on the taller
form. The clean Blender FBX round trip passed. No shared material was changed.

The original marker actor/component identities remain, with unit-scale poses
at X=775.5 cm, Z=-1212 cm, yaw 90 degrees. Their Y centers are 28001.786,
28198.214 and 28394.643 cm, derived from the saved wall-panel centers. The former
decorative cube collision is replaced with NoCollision and disabled navigation
participation. Native floor, walls and connectors are unchanged; all other
3,137 actor transforms/collision states are compared in the placement script.

Initial preview `Z09SlitPreview-20260915-101519-b3c9f242` passed placement checks
and exited 0 without Python errors, but its east view showed the first fissure
partly hidden by a pier. The group was shifted to the next three clear panels.
The verifier now rejects overlap between the fissure bounds and the surveyed
native pier bounds. This is a geometric placement check, not proof of visibility
from every camera or live collision contact.

Source, original marker baseline and manifest are in
`Art/Source/Aurelion/Z09SlitKit/`; builder is `build_z09_slit_kit.py`.
PavingIvory, EclipseIntrusion and EclipseSeam remain the shared materials.
Nanite uses position precision 10 and full fallback geometry; no collision hulls
are authored. The fissures are shallow visual overlays and do not cut holes
through the native wall or implement Chaos destruction.

Fine branch-tip aliasing, lighting, live narrative and traversal qualification,
performance, supporting cast and the broader environment rebuild remain open.
The alignment estimate remains unchanged.

Revised preview `Z09SlitClearPanels-20260915-101925-830162e1` passed the
placement, material, collision and pier-overlap checks and exited 0 without
Python errors. The east-context image was inspected: all three repetitions
are visible between the piers. This does not qualify every gameplay camera.

`Z09SlitSaved-20260915-102222-62ab70c1` backed up and saved the map. Fresh reload
`Z09SlitFresh-20260915-102512-74ddfc9b` passed all 95 architecture reports.
Both exited 0 without Python errors. Before/after gallery views, east-panel
detail and fit/reload evidence are in
`Docs/Validation/AurelionZ09Slits-2026-09-15/`. Actor count remains 3,140.
