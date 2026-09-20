# Custom M13 chamber wall module

The Fifth Chamber's 639 stock spaceship wall panels are replaced with the owned
`SM_Aurelion_KIT_Z10ChamberCoffer` module. Its modeled stone frame, recessed
coffers, oblique gold channels, keyed joints, and relief registers extend the
existing Aurelion kit. Existing chamber ribs and other architecture remain.

Editable Blender source, FBX, studio render, and geometry manifest are in
`Art/Source/Aurelion/Z10ChamberWall`. Rebuild with Blender 4.5:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --background --factory-startup --python Art/Source/Aurelion/build_z10_chamber_wall.py
```

The module has 9,432 triangles, two UV channels, and three existing project
materials: PavingIvory, Gold, and Reveal. Nanite is enabled. It is a visual-only
replacement with no generated collision; the original component's collision
state and the separate gameplay collision remain authoritative.

The first preview rejected the FBX Y-axis handedness reversal before changing
the map. The corrected preview
`M13ChamberCofferPreview-20260920-014252-718c2d33` verifies all 639 instance
transforms and all actor transforms/collision states are preserved. The imported
mesh fits inside the original local bounds within a 1 mm comparison tolerance.
The chamber approach and closer wall render were inspected before saving.

`preview_m13_chamber_coffers.py` imports and previews without saving a map.
`save_m13_chamber_coffers.py` reuses that reviewed mesh, backs up M13, saves only
M13, reloads, and rechecks the transforms/collision and protected M12 hash.
This presentation change does not alter mission journal semantics or revision.

Saved readback `M13ChamberCofferSaved-20260920-014616-954e195c` passed and the
editor exited normally. The saved chamber render was inspected. Full validation
`20260920-014811-560deef4` passed the build, 719 matching automation tests, report
coverage, and source integrity without SkipBuild. The prechange full gate was
`20260920-013042-3b8f5e8f`.

The images are editor game-view captures. GPU profiling, moving-camera shadow
quality, full mission traversal, and final AAA art acceptance are not established
by this geometric replacement or the automated test suite.

## Gameplay-distance accent revision

The primary conductor now measures 6.5 cm across (previously 3.8 cm), the course
inlay 5.5 cm (previously 2.6 cm), and the stone border 7 cm (previously 4.3 cm).
Paired frame flutes are 2.5 cm wide. Four 7.5 cm relief registers replace five
4.5 cm registers, retaining the carved-panel design with broader primary marks.
The source mesh is now 9,056 triangles. Its outer envelope, materials, two UV
channels, Nanite precision/full fallback settings and collision-free role remain.

The Nanite/full-fallback comparison
`M13NaniteComparison-20260920-045718-11b40e64` retained the broken dark edges in
both views; its log confirms r.Nanite changed from 1 to 0. No renderer toggle was
saved. Earlier gallery evidence in `AurelionZ09SurfaceDiagnosis-2026-09-15.md`
supports broader primary accents at normal resolution. This revision improves
their visual weight but does not resolve the remaining fine-edge artifact.

`M13CofferReadability-20260920-050140-bfcba359` reimported only the owned mesh with
a backup, verified unchanged bounds, all 639 instance transforms and actor/collision
states, and preserved both map hashes. The studio and close in-engine renders were
inspected. Fresh run `M13CofferFreshReadback-20260920-050509-283bd114` verified the
same asset hash, two UV channels, full fallback, Nanite precision and no generated
collision; its chamber render was inspected. Both use 64-frame capture preparation
and restore the original value afterward. These are static editor captures, not
moving-camera or GPU performance acceptance.

Neither map was saved. M12 SHA256 remains B7CEEAB5...; M13 remains
`7a234a36ad967b95ea342b069de3987aca1da1ce1fe9671a45f9b8823031169d`.
Full gate `20260920-050521-9ae3e6e4` passed the build, all 719 matching automation
tests, coverage and source integrity without SkipBuild. The preceding full gate
was `20260920-045436-3fb743c4`. Broader architecture, lighting, live combat and
visual acceptance remain open; this is not a 90% alignment claim.
