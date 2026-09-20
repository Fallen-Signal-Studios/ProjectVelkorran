# Crownmark chamber centerpiece

The stock research-unit casing at `Z10_Integrated_Crownmark` is replaced with
an owned Aurelion mesh: six fluted ivory supports, bronze collars and registers,
stepped circular base/crown, illuminated base marks, and a broken seal behind
the existing figure. The former glass component is hidden. The central figure,
native blocking component, actor transform and mission interactions are retained.

Editable Blender source, FBX, studio render and manifest are in
`Art/Source/Aurelion/Z10CrownmarkCore`. Rebuild with Blender 4.5 and
`Art/Source/Aurelion/build_z10_crownmark_core.py`. The mesh has 33,708 triangles,
two UV channels, Nanite with a full fallback mesh, and no generated collision.
It uses the existing PavingIvory, Gold, Reveal and UplightLens materials.

The mesh is placed at `(0,35100,-1680)` cm with unit scale. Its 132 x 132 x 196.8 cm
envelope remains inside the original decorative body bounds. The fitting script
checks native collision/transform data separately and checks all actor transforms
and component collision states before and after the visual replacement.

The first preview, `CrownmarkCorePreview-20260920-043552-9d1763c9`, exposed the seal
intersecting the existing figure. The revised source offsets the entire seal
assembly 40 cm toward the back, accounting for FBX Y reflection.
`CrownmarkCoreClearance-20260920-043845-cb769546` passed the fitting checks and its
close in-engine render was inspected before the save run. Both reports remain
under `Saved/Validation/Aurelion`.

`fit_m13_crownmark_core.py` imports/previews without saving the map.
`save_m13_crownmark_core.py` uses the reviewed mesh, backs up M13, saves only M13,
reloads it, verifies the placement/hidden glass/native collision, and checks M12's
disk hash. This changes presentation only, so mission journal revision semantics
are unchanged. The previews are fixed editor game-view captures, not a new
campaign playthrough, GPU profile or final AAA visual acceptance. Chamber shadow
aliasing, surrounding stock architecture and the broader refinement goal remain.

Saved run `CrownmarkCoreSaved-20260920-044037-f43475e0` passed reload and preservation
checks; the saved close render was inspected. M12 retains SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
Full validation `20260920-044133-a90ec85f` passed the build and all 719 matching
automation tests, report coverage and source integrity. The editor build target
was up to date; SkipBuild was not used. The prechange gate was
`20260920-042906-5bf739c3`. No packaged build or full post-change mission replay
is claimed.
