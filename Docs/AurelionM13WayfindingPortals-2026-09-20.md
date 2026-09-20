# M13 grounded wayfinding housings

The three existing destination labels now have custom floor-supported Aurelion
housings. Their words, placement, text material and direction cues are unchanged.
The new modules provide recessed lettering beds, ceramic octagonal surrounds,
bronze inlays, inset orientation lenses, fluted posts, joint sleeves, fasteners,
rear service cassettes and ventilation slots.

Two Blender-authored sizes share the visual design: a 4.05 m chamber housing
and a 3.05 m housing reused in the gallery and departure concourse. Each has
18,096 rendered triangles, two UV channels, imported normals, Nanite enabled and
full fallback geometry. Editable Blender source, FBX exports and a studio render
are retained under `Art/Source/Aurelion/WayfindingPortals`.

Floor traces in `M13SignMountAudit-20260920-092750-3c054c29` established the
supports at Z=-1800 cm for the chamber and Z=0 for the other two signs. Upward
collision traces did not find an anchor, so the design uses floor-supported
posts. That does not establish the absence of visible ceiling geometry.

The first unsaved preview showed the imported mesh facing backward and covering
the text. The corrected yaw-180 placements in
`M13WayfindingFacing-20260920-093526-6d68ac6b` were inspected in all three approach
views. The front lettering remains visible inside its frame and the supports
meet the floor. The known chamber editor-render artifacts remain outside this
increment; no global lighting or rendering changes were made.

Each mesh contains two post collision hulls and one header hull, preserving a
3.36 m central opening. Minimum headroom is 2.24 m in the gallery/concourse and
3.24 m in the chamber. Fifteen isolated editor sweeps test clear passages at
three lateral positions per housing with a 55 cm radius/100 cm half-height
capsule, plus positive post/header collision. These isolate new geometry;
they do not replace a companion/navigation or campaign route test.

The initial fresh-load save attempt detected collision was not ready and stopped
before saving. The authoring path now drains mesh compilation before placement
and verifies three loaded convex hulls per mesh. Fresh run
`M13WayfindingFreshCollision-20260920-094037-5edf41f0` passed all fifteen sweeps,
saved only M13, reloaded all three housings, preserved existing actors'
transforms/collision state and the three text labels, and verified M12 unchanged.
The prechange M13 package is backed up inside that run.

The live-frame reviewer now optionally accepts multiple named camera views,
retains separate ordinary/high-resolution images, and inventories the saved
housings in a restored M13 world. Its default single-view behavior remains.
`review_m13_wayfinding.py` uses the existing earned CP9 load flow; it moves only
the PIE review camera, never the pawn or campaign journal.

Live run `M13WayfindingLive-20260920-094321-6794c49e` completed with status
`captured`, restored CP9 successfully, and found all three saved housings with
query-and-physics collision. All three ordinary 1696 x 862 game frames were
visually inspected: lettering fits without occlusion and the posts meet their
floors. The gallery and departure frames remain dark around the housing trim;
these checks establish readable destination signs, not final lighting quality.
The chamber frame is cleaner than the earlier editor preview. These fixed
camera views do not establish physical navigation or companion clearance.

Baseline full build, 720 tests, coverage and source integrity passed in
`20260920-092529-fd9c7652` without SkipBuild. Python syntax checks passed for the
authoring and review scripts. No C++ or mission-journal semantics were changed.
Final full build, 720 tests, coverage and source integrity passed in
`20260920-094902-8342bd39` without SkipBuild; the wrapper exited successfully.
M12, the creator's grenade assets and the local GASPALS plugin are preserved.
This is a wayfinding presentation increment, not AAA or 90% acceptance of M13.
