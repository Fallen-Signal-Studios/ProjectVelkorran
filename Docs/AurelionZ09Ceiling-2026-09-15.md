# Wound-gallery coffered ceiling

The 56 flat vendor ceiling tiles are replaced on their original HISM component
by a custom Aurelion octagonal coffer. It uses stepped stone recesses, narrow
gold channels, corner bosses and perimeter beams, matching the new gallery
walls and piers. Each unit is 4 x 55/14 x 0.55 metres, with 13,020 source
triangles, two UV layers and three materials. Blender source and FBX are in
`Art/Source/Aurelion/Z09CeilingKit/`; `build_z09_ceiling_kit.py` rebuilds them.

The underside stays at Z=-900 cm, preserving six metres of headroom. Relief
extends upward to -845 cm, deliberately increasing visual roof thickness.
The full footprint remains 55 x 16 metres. The roof-neighbor survey included
hidden static meshes and instanced geometry. No local structure occupied the
new relief volume; broad background sky/cloud bounds were not treated as local
roof obstructions. Sixteen native walls, lintels and ribs end at the original
underside. Their mesh, collision, visibility and bounds are checked unchanged.

All 56 instances use unit scale. The original actor/component transforms and
3,140-actor count remain unchanged. The visual mesh has no collision hulls,
uses NoCollision, disables navigation participation and retains Nanite with
position precision 10 and full fallback geometry. Structural ceiling members
are not designated campaign destructibles.

The Blender FBX round trip passed. The same-facing axis-aligned coplanar audit
found zero overlaps; that limited check does not establish all possible
intersections or runtime performance. Preview run
`Z09CeilingPreview-20260915-085930-30c1695d` passed placement checks and exited
0 without Python errors. Entry and close ceiling images were inspected.

Fine trim artifacts remain visible, particularly on the adjacent walls and
piers. Lighting, materials, the remaining gallery surfaces, live traversal and
performance still need qualification. This is an architecture replacement,
not final AAA acceptance or a new 90% TDD alignment claim.

`Z09CeilingSaved-20260915-090517-cad2053a` backed up and saved the map; the
reverse image was inspected. Fresh editor run
`Z09CeilingFresh-20260915-090747-773db228` passed all 91 architecture reports,
including the 56 ceiling instances and sixteen native neighbors. Both runs
exited 0 without Python errors. Review images and fit/reload reports are in
`Docs/Validation/AurelionZ09Ceiling-2026-09-15/`. The alignment estimate remains
unchanged; no new live campaign traversal was performed for this ceiling pass.
