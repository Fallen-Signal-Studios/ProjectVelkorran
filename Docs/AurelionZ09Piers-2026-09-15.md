# Wound-gallery piers

Eight generic vendor piers are replaced by a six-metre variant of the custom
Aurelion fluted pier. The source generator reuses the authored seven-metre pier
construction, baking the vertical proportion into the geometry before FBX export.
The earlier shared mesh remains unchanged. Runtime instances use unit scale and
turn their detailed faces toward the gallery.

Source: `Art/Source/Aurelion/Z09PierKit/Aurelion-Z09-Pier.blend` and
`build_z09_pier_kit.py`. The FBX round trip passed at 1.2 x 1.0 x 6.0 m with 21,064
source triangles, two UV layers, three materials and no collision hulls. The
same-facing axis-aligned coplanar-face audit found zero overlaps; it does not
qualify every possible surface intersection or runtime cost.

`pier-baseline.json` retains the old asset, materials, actor/component transforms
and eight original instance transforms. Replacement bounds match those original
envelopes within 0.02 cm. The original actor and HISM component remain, all actor
transforms/collision states are compared before and after placement, and the
new visuals use NoCollision with navigation participation disabled. These are
structural piers and are not campaign destructibles.

`Z09PiersPreview-20260915-084424-9aa9043c` passed the fit checks, exited 0 and
reported no Python errors. Entry and close-detail screenshots were inspected.
The piers now match the gallery wall vocabulary, while fine trim/conductor
rendering artifacts remain visible in Unreal. Final material/light quality,
the rest of Z09, live-route acceptance and performance remain unfinished.

`Z09PiersSaved-20260915-084737-b48c16ce` saved the map after backing it up and
passing the placement checks. The reverse view was inspected. Fresh reload
`Z09PiersFresh-20260915-084954-1481984d` passed all 90 architecture reports,
including the eight pier envelopes. Both runs exited 0 without Python errors.
This is saved-map geometry verification; it is not a new live campaign traversal
or a 90% TDD alignment claim. The alignment estimate remains unchanged.

Review images and fit/reload reports are retained in
`Docs/Validation/AurelionZ09Piers-2026-09-15/`.
