# M13 overhead ring replacement

Replaced the 96 noncolliding basic cubes owned by
`Aurelion_Art_M13_Z10_14_bc37a6` with three custom curved cornice modules, one
for each 32, 40 and 50 metre radius. Each ring retains its 32 sector centers,
rotations and elevation. The new meshes use unit scale and an 11.25 degree arc.
The separate scenic/gameplay actors remain untouched.

The editable Blender source and deterministic generator are under
`Art/Source/Aurelion/Z10CeilingRings` and `build_z10_ceiling_rings.py`.
Each module has stepped stone cornices, inset frieze bays, carved load keys,
raised gold registers and broad gold conductors. Each has 10,328 source
triangles, two UV channels, Nanite with full fallback geometry and no collision.
The components also have navigation relevance disabled. No C++ or mission
journal semantics changed.

`fit_m13_ceiling_rings.py` checks the original 96 transforms against the
recorded baseline, imports only the owned meshes and previews without saving
the map. `save_m13_ceiling_rings.py` uses those reviewed assets, backs up M13,
saves only that map, reloads, then checks all 96 placements, all other actor
transforms/component collision states, and M12's disk hash.

Preview `CeilingRingPreview-20260920-051525-41aa28ae` passed the fit checks.
The Blender render and three in-engine views (chamber, ceiling, ring detail)
were inspected. The resulting bands join around the chamber without the old
straight-block silhouette. These are editor game-view captures, not runtime
performance measurements or final AAA visual acceptance. Surrounding supports,
lighting and existing fine edge artifacts remain separate work.

Prechange full build/automation gate: `20260920-050521-9ae3e6e4` (719 passed).

Saved run `CeilingRingSaved-20260920-051731-f83223fc` passed the reload and
preservation checks. Its chamber render was inspected, and the temporary camera
was removed with the original 1,315 actors restored. M12 retains SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
Postchange gate `20260920-051948-7debdd2b` passed the editor build, all 719 matching
automation tests, report coverage and source integrity. SkipBuild was not used;
the build target was up to date. No packaged build or postchange campaign
playthrough is claimed.
