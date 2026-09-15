# Gallery wall detail refinement

The wall-side, wall-end and baffle meshes now use one primary modeled panel
border rather than an additional parallel 14 mm line. Their stepped panel beds,
stone courses and edge pilasters remain. Oblique gold conductors widen from
24 to 36 mm, and three 18 mm service marks replace five 12 mm marks. This follows
the pixel-coverage diagnosis in `AurelionZ09SurfaceDiagnosis-2026-09-15.md`.

The three meshes each have 10,732 source triangles, down from 13,740. This is a
source count, not a measured runtime performance improvement. The lintel is
unchanged. The Blender source, generator and three FBXs are updated. The clean
FBX round trip passed; all four limited axis-aligned coplanar-face audits found
zero overlaps. Mesh bounds, UV layers, material slots and collision policy are
unchanged.

`refine_z09_wall_details.py` reimports only the three owned mesh assets. It does
not change placements, collision, shared materials, Nanite settings or save
the map. All 78 original wall placement envelopes are checked after reimport.

Initial run `Z09WallDetailRefinement-20260915-092234-da6ee1ea` saved the imported
meshes and passed geometry checks, but an overly strict clean-editor-map
assertion failed: reimport had dirtied the open map. It did not save that map
or produce review captures. The script now verifies the on-disk map hash and
records transient dirty state rather than treating that state as a disk edit.

Fresh review `Z09WallDetailsReview-20260915-092418-a6b98b7b` loaded the saved mesh
assets without reimport, passed all 78 wall placement checks, retained the map
hash and exited 0 without Python errors. Entry and close lit screenshots at
1600 x 900 were inspected: the primary border and larger service marks are
clearer, while the nearby pier artifacts remain. This is a retained art
refinement, not complete anti-aliasing, lighting, material or live-route
qualification. The alignment estimate is unchanged.

Fresh architecture run `Z09WallDetailsFresh-20260915-092638-63e0c387` passed
all 91 architecture reports and exited 0 without Python errors. Review images,
map-file preservation and wall-fit/reload evidence are retained under
`Docs/Validation/AurelionZ09WallRefinement-2026-09-15/`.
