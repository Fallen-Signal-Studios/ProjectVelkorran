# M13 chamber supports

The 244 stock garage-beam instances on `Aurelion_Art_Z10_Chamber_ribs` now use
the owned `SM_Aurelion_KIT_Z10ChamberRib` module. The upright and diagonal
placements retain their transforms and scale. The replacement stays inside
the original mesh bounds and uses its bottom-center pivot.

The custom support has a solid stone spine, four fluted staves on its broad
faces, recessed gold conductors, dressed side channels, stepped capitals and
raised load registers. It contains 9,964 triangles, two UV channels, Nanite
with full fallback geometry, and no generated collision. Existing component
collision and separate gameplay collision are unchanged.

Editable Blender source, FBX, render, baseline and manifest are in
`Art/Source/Aurelion/Z10ChamberRibs`; the deterministic generator is
`Art/Source/Aurelion/build_z10_chamber_ribs.py`. Use Blender 4.5 with
`--background --factory-startup --python` to run it.

`inspect_m13_rib_fit.py` records the old mesh bounds and all instances.
`fit_m13_chamber_ribs.py` verifies the baseline, imports the mesh, checks local
containment and all actor/collision states, then captures an unsaved preview.
`save_m13_chamber_ribs.py` backs up M13, saves only M13, reloads, and checks the
same placements plus M12's unchanged disk hash. No C++ or journal revision
semantics changed.

Audit `RibFitAudit-20260920-052223-8e818fea` established the original pivot and
bounds. Preview `ChamberRibPreview-20260920-052513-c4ef53df` passed all 244
placement checks. The Blender render and chamber, close support, and angled
brace views were inspected before saving. The separate existing wall-edge
artifacts remain visible; this replacement does not resolve those or establish
final AAA visual acceptance. Runtime performance and the complete campaign
route have not been requalified by these editor captures.

Prechange full build/automation gate: `20260920-051948-7debdd2b`, 719 passed.

Saved run `ChamberRibSaved-20260920-052703-2af64d58` passed the reload,
containment and preservation checks; its chamber render was inspected. M12
retains SHA256 `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
Postchange full validation `20260920-052847-e6bba01c` passed the editor build,
all 719 matching automation tests, report coverage and source integrity.
SkipBuild was not used; the target was up to date. No packaged build was run.
