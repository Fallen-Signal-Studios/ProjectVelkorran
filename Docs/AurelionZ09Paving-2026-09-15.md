# Wound-gallery paving

The 56 gallery floor tiles and four approach tiles receive custom basalt paving
on their original art actor. Two true-size meshes fit the existing footprints:
4 x 55/14 m and 3 x 4.81 m. Each has eight individually beveled slabs, expansion
joints, a recessed backing and small gold joint registers. The retained route
markings remain separate. Each mesh has 2,820 source triangles, two UV layers,
three material slots and no collision hulls.

The original component is reused for the main gallery; one additional HISM
component holds the approach cuts. All 60 instances have unit scale. Their XY
bounds and top surfaces match the original tiles within 0.02 cm. Visual thickness
increases downward to 12 cm; the walking datum stays unchanged. NoCollision and
disabled navigation participation preserve native traversal geometry.

`Z09FloorAudit-20260915-092958-2980e31f` recorded the saved visible floor census
and nearby static-mesh bounds, including hidden collision. The gallery floor
collider spans X=-800..800, Y=25350..30850 and Z=-1560..-1500 cm. Twenty-five
nearby native collision components, including connector geometry, are checked
for unchanged mesh, bounds, collision and visibility. Broad-phase overlap alone
does not establish ownership or prove live contact. Existing approach tile
footprints, including their pre-existing transition overlap, are preserved.

Blender source and FBXs are in `Art/Source/Aurelion/Z09FloorKit/` and are rebuilt
by `build_z09_floor_kit.py`. Clean FBX round-trip checks passed. Both limited
same-facing axis-aligned coplanar audits reported zero overlaps.

`Z09FloorPreview-20260915-093334-edebb1d1` passed all 60 placement and 25 native
neighbor checks and exited 0 without Python errors. Entry and approach images
were inspected. The new slab layout and retained route lines are legible; this
does not qualify final materials, lighting, live traversal or performance.
Piers, gallery props and other environment work remain unfinished. The alignment
estimate remains unchanged.

`Z09FloorSaved-20260915-093558-4d0c21d7` backed up and saved the map after the
same placement checks; its close paving image was inspected. Fresh reload
`Z09FloorFresh-20260915-093854-45a81359` passed all 92 architecture reports.
Both runs exited 0 without Python errors. Evidence is retained under
`Docs/Validation/AurelionZ09Paving-2026-09-15/`. Actor count remains 3,140.
This static verification is not a new live campaign traversal.
