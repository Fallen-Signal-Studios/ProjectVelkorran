# Z03 service-route and bridge rails

The full 50-instance vendor railing batch is replaced by sixteen custom runs: four continuous twelve-metre sloped rails, two twelve-metre landing rails and ten 4.2-metre bridge sections. The original art actor remains, with two additional HISM components grouping the new module sizes. Native route guards, ramp and landing collision remain unchanged.

`build_z03_rail_kit.py` authors the three modules in Blender. Posts remain upright along the 3 m rise / 12 m run rather than tilting a level railing mesh. Dark grips and slender balusters carry the main silhouette; dressed ivory posts and narrow gold registers match the preceding architecture. Every Unreal instance has unit scale and zero pitch/roll.

Ramp endpoints now follow the intended twelve-metre run and meet the 430 cm landing handrail height. This trims the old tilted visual overhang by approximately 15.76 cm at either end and raises its maximum visual height by approximately 1.87 cm. The rounded grip corner is about 0.073 cm below its ideal sharp corner. These are deliberate visual corrections, not changes to the native physical guard. Existing cross-route envelopes remain exact; the service route has at least 354 cm clear between visual rail envelopes and the outgoing bridge at least 554 cm.

## Evidence and limits

- All three FBX modules pass clean round-trip geometry, bounds, finite-coordinate, UV and material-slot checks.
- Three scoped axis-aligned coplanar checks report zero overlaps.
- `verify_z03_rail_profiles.py` checks 75 downward clean-FBX contacts along the three handrail centrelines against their intended heights. It does not claim runtime collision or exhaustive joint verification.
- `check_z03_rails.py` verifies all sixteen poses, complete accounting for all fifty original instances, width envelopes, intended slope bounds, unit scale, Nanite settings and unchanged native room geometry. The modules have two UV channels, Nanite precision 10, full fallback geometry and no added collision.
- Preview `Z03RailPreviewVerified-20260914-211521-60ed7e4e` completed without Python errors. Approach, middle, landing and bridge views were inspected.
- Saved run `Z03RailsSaved-20260914-211926-ebedd352` backed up the map, saved and captured four reviewed views; screenshot warmup returned to its original value and the temporary camera was removed. Fresh run `Z03RailsFresh-20260914-212228-ad730cf7` completed with 76 passing architecture reports, 3140 actors, a clean map and no Python errors.

The ramp deck/underside, other floor surfaces, technical signage and broader scene lighting still need work. The generic Z04 frontage is particularly visible from the new bridge view. These static results do not qualify live player movement, scanner readability, full-map art quality, Chaos destruction or packaged performance. The supported slice estimate remains 63.75%; the 90% goal is still open.
