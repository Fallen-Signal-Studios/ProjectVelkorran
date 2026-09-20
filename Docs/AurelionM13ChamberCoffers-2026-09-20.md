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
