# Z04 relay floor and balcony kit

The complete vendor floor batch is replaced with 160 fitted room bays, one dressed balcony deck, two wide ascent modules and two cover caps. The room covers 62 x 38 m at Z=0; the balcony covers 14 x 11 m at Z=300 cm. Both ramps remain six metres wide with a three-metre rise over twelve metres of horizontal run. The caps retain their native 420 cm and 220 cm top heights.

The stone floor uses cut slab joints and continuous backing. Balcony and ramp edges have full-depth fascia, recessed registers and narrow conductors; their undersides have recessed fields and transverse/longitudinal courses. The floor uses the already reviewed textured `M_AurelionKit_Z03FloorStone` material without modifying it. Native collision and all room gameplay actors are retained.

## Evidence

- `Z04FloorBaseline-20260914-233228-112bf909` verified the saved vendor batch against the earlier full inventory and recorded all 186 world transforms before replacement.
- Five clean-FBX geometry/UV checks passed. Each module has two UV channels, Nanite position precision 10 and full fallback geometry.
- All five scoped coplanar audits report zero overlaps. This audit covers same-facing axis-aligned convex faces, not every possible surface defect.
- `verify_z04_floor_profiles.py` independently checks 78 clean-FBX tile-centre walking heights across room paving, ramps, balcony and caps.
- `check_z04_floors.py` checks the saved module assignments, transforms, room grid and 240 contacts against the unchanged native floor, ramp, balcony and cover collision. The previous roof/wall checks retain the surrounding native room geometry.

Preview `Z04FloorsPreview-20260914-233632-897c9507` completed with exit 0 and no Python errors; all 240 native contacts passed. Entry, south ramp, balcony, west ramp and underside captures were inspected. The joins are continuous in these views, and the underside presents a complete coffered surface. The balcony is still brightly lit; lighting grain, old rails, crates and guidance strips remain visible. A fresh fetch found origin/main unchanged at `ac3cc446`.

Saved run `Z04FloorsSaved-20260914-234050-12a6be7e` completed with exit 0 and no Python errors. All five saved captures were inspected and matched the reviewed treatment; the prior map was backed up before saving.

Fresh run `Z04FloorsFresh-20260914-234425-2ed2ff40` completed with exit 0 and no Python errors. All 81 architecture reports passed, including the 240 native floor contacts, with 3140 actors and a clean map.

This pass does not qualify live combat/traversal, full-map visual fidelity or packaged performance. Z04 rails, piers, props, scanner/receiver fixtures and annotations remain unfinished. The supported slice estimate remains 63.75%; the full goal remains open.
