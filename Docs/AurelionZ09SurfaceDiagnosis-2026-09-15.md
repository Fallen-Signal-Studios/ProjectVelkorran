# Z09 fine-surface diagnosis

The gallery baffle's fine border remains visible in base color with all original
static-mesh components hidden and one temporary copy at the saved front-layer
pose. The isolated copy with `disallow_nanite` requested shows the same border
pattern. This weakens placement interference and Nanite-specific explanations
for this particular pattern; it does not establish every renderer path or
exclude intersections elsewhere in the kit.

At a closer camera position, the fine borders are continuous. A 3200 x 1800
capture from the original camera also resolves the lines more clearly than
the 1600 x 900 capture. The source border is 14 mm wide, approximately 1.26
pixels at 10.6 m for a front-facing surface at 1600 pixels and 80 degrees FOV,
before accounting for perspective orientation. Pixel coverage is a contributing
factor. Higher-resolution captures are diagnostic evidence, not an implemented
runtime rendering fix or a performance recommendation.

The next art iteration should compare fewer, broader primary accents at normal
gameplay resolution, retaining fine engraving for close views. It must validate
lit moving views and preserve the intended Aurelion detail; this diagnostic
alone does not justify changing all shared materials or disabling Nanite.
The pier artifacts and carrier roof remain separately unresolved.

Runs:

- `Z09SurfaceIsolation-20260915-091424-7c3b0b44`: context, isolated and requested
  non-Nanite fallback base-color captures. Exit 0, no Python errors.
- `Z09SurfaceCoverage-20260915-091755-13559ee6`: close camera and doubled
  resolution base-color captures. Exit 0, no Python errors.

Both runs restored component visibility and removed their temporary actors,
returning to 3,140 actors. No map or asset was saved. Niagara/background rocks
remain visible because they are not the original static-mesh components being
isolated; their appearance is not an environment change. The initial saved
scene remained the source of component visibility for each run.

The isolated/fallback images are not pixel-identical: the conservative baffle
rectangle x=[757,1190), y=[359,821) contains 25,355 changed pixels of 200,046,
with mean absolute RGBA channel delta 0.04473 on the 0-255 scale. This numerical
comparison supports a visually similar result, not exact equivalence.

Scripts: `diagnose_z09_surface_isolation.py` and
`diagnose_z09_surface_coverage.py`. Captures and run receipts are retained in
`Docs/Validation/AurelionZ09SurfaceDiagnosis-2026-09-15/`.
This is diagnostic progress; the alignment estimate remains unchanged.
