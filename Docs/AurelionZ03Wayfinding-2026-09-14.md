# Z03 wayfinding and obsolete decoration

Forty-four stretched vendor floor pieces are replaced by custom one-metre route registers with cut-stone margins, inset light apertures, fine conductors and service fasteners. Each module is 12 cm wide and 7.5 mm deep, placed 1 mm below the floor datum, with no collision. The guide moves from X=7000 to X=7200 to clear the centre cover edge and stays inside the bridge approach.

The floating destination text is replaced by a compact wall-mounted sign beside the exit. The custom 1.6 m plaque has dressed edges, fine gold registers and a dark text field. The same sign actor holds both plaque and text; no new actors are added.

Five standalone setup annotations are hidden in game while remaining available to the editor: zone dimensions, entry marker, two scanner placeholders and CP2 marker. The two native scanner labels remain visible, with their runtime SCANNING/DETECTED/OFFLINE logic intact.

Six obsolete decorations are hidden in game and have collision disabled: two narrow canopy pylons, two stretched railing upper spans and two broad gold floor bars. Their transforms and asset references are retained for reversibility. Disabling their collision is intentional; retaining collision after hiding these shapes would leave invisible obstacles. The new piers, walls, floors, cover, ramps, guards and ceiling remain in place. The preceding ceiling checker explicitly accommodates these six retired decorations; the new checker requires all six to be hidden and non-colliding.

## Validation and remaining work

Both FBX assets pass round-trip geometry, bounds, two-UV-channel and material checks. Both scoped source coplanar audits report zero overlaps after correcting the plaque edge overlap. An initial layout preview exposed high text alignment and the centreline cover conflict; both were corrected before saving.

Revised preview `Z03WayfindingRevised-20260914-220913-0babf7f8` completed without Python errors, with all four views reviewed. Saved run `Z03WayfindingSaved-20260914-221239-fe25b28c` backed up the map, saved the pass and produced four matching reviewed views. All 44 guide floor contacts passed; minimum clearance between the guide edge and the three native cover blocks is 194 cm. Existing architecture retains Nanite precision 10, full fallback geometry and two UV channels on the two new meshes.

The first fresh check, `Z03WayfindingFresh-20260914-221559-b8b7f704`, correctly failed: hidden state persisted, but the old BlockAll/Custom profiles restored collision on all six retired decorations. `Z03CollisionProfiles-20260914-221823-638c3e2a` recorded the reloaded values and saved explicit NoCollision profiles. The standard placement script and checker now require the profile as well as disabled collision, so future application does not repeat this issue.

Fresh run `Z03WayfindingFreshVerified-20260914-221949-9a09cd84` completed without Python errors and with all 78 architecture reports passing, 3140 actors and a clean map. The six retired profiles remained NoCollision after reload.

These are static authoring checks. Live route guidance, scanner readability and packaged performance remain unqualified. Scanner fixture meshes, full-map dressing, character likeness, combat presentation and Chaos destruction remain open. This pass does not establish AAA quality or increase the supported 63.75% slice estimate.
