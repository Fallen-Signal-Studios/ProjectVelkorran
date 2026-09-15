# Wound-gallery wall kit

The Z09 survey found 78 generic wall instances across side walls, end walls,
freestanding baffles and doorway heads. The custom Blender kit supplies four
fitted modules with two dressed faces, stone courses, recessed panel fields,
edge pilasters and narrow gold conductors. This is the wall portion of the full
gallery pass; floor, ceiling, piers, railings, wound presentation, lighting and
live-route qualification remain separate unfinished work.

`Z09GalleryReview-20260915-082323-96973b3d` captured the saved room from entry,
middle and reverse cameras and refreshed the mesh census. The layout contract
retains the approximately 55 x 16 m gallery and shallow approach descent. No
gravity reversal or new gameplay sequence is introduced.

Source is `Art/Source/Aurelion/Z09WallKit/Aurelion-Z09-Walls.blend`, with generator
`build_z09_wall_kit.py`. Four FBX files passed a clean Blender round trip and each
reported zero same-facing axis-aligned coplanar overlaps. That audit does not
prove absence of every oblique/intersecting surface. The initial audit invocation
used an incorrect relative path; corrected runs produced the retained reports.

The fit records all original 78 world transforms in `wall-baseline.json` and
maps each source index to its fitted unit-scale module. Three additional HISM
components on the same art actor accommodate the four sizes, keeping 3,140
actors. No new collision or navigation obstruction is authored. The fit checker
allows at most 1 cm of visual expansion per original bound, including edge trim.
Every other actor's transform/collision state is checked during placement.

`Z09WallsPreview-20260915-083113-6c88ba11` imported the owned meshes but stopped
before map saving on a checker error: Unreal's Transform constructor expected a
Rotator rather than a quaternion. The checker now performs that conversion.

The kit does not implement Chaos destruction. Making a baffle destructible later
requires explicit native obstruction ownership, real combat admission, safe
debris and checkpoint restoration. The appearance of a thin partition alone is
not evidence that its gameplay boundary can be removed.

## Saved result

`Z09WallsFit-20260915-083338-3d401f76` passed the corrected fit checks; entry and
middle views were reviewed. `Z09WallsSaved-20260915-083609-a268d6fa` backed up and
saved the map, retaining the original art actor and all other actor states. Its
reverse view confirmed the dressed rear baffle faces. Both runs exited 0 without
Python errors. The fine trim still shows some aliasing at distance; the generic
piers, ceiling and current lighting remain visibly unfinished.

`Z09WallsFresh-20260915-083828-f502e017` reloaded the saved map and passed all 89
architecture verification reports, including all 78 fitted wall envelopes. It
exited 0 without Python errors. This is static saved-map verification, not a new
live traversal, mission, destruction or performance qualification. No alignment
percentage increase is claimed. Review images and reports are retained in
`Docs/Validation/AurelionZ09Walls-2026-09-15/`.
