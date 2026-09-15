# Z03 room, ramp and bridge floors

Four custom Blender modules replace the 91-instance vendor floor batch with 85 unit-scale placements: 72 room bays, ten bridge bays, two continuous twelve-metre slopes and one twelve-metre landing. The existing art actor retains ownership, with four HISM components grouping module sizes. Native floor, ramp, landing and guard collision remains unchanged.

The slopes follow the native 3 m rise / 12 m run. Cut stone walking slabs, recessed joints, continuous fascia, narrow gold registers and dark underside fields replace the former thin, overlapping platform tiles. The landing is 40 cm deep; side registers extend 2 mm beyond each original side. Room and bridge modules are 7.5 cm deep. The original room and bridge XY envelopes and walking heights are retained.

The first floor preview looked excessively smooth. An owned `M_AurelionKit_Z03FloorStone` duplicate retains the existing stone texture inputs and roughness while increasing base texture blend from 0.12 to 0.65 and fine normal blend from 0.12 to 0.22. Shared kit materials are unchanged. This improves visible stone variation but does not resolve the room's bright illumination or all image noise.

## Evidence and limits

- Four clean FBX round trips pass geometry, bounds, UV and material-slot checks. Each mesh has two UV channels, Nanite precision 10, full fallback geometry and no added collision.
- Four scoped coplanar audits report zero overlaps. The clean-FBX profile checker confirms 36 walking-plane contacts across all four modules.
- The editor checker accounts for all 85 placements, unit scale, original room and bridge bounds, floor material assignment and the retained native room state.
- 108 native floor contacts cover 72 room centres and twelve positions on each ramp and the raised landing. These are static support-plane checks, not player traversal qualification.
- Material preview `Z03FloorStonePreview-20260914-214646-769de65f` completed with no Python errors. Approach, middle, landing, underside and bridge views were inspected.
- Saved run `Z03FloorsSaved-20260914-215018-7298b8a4` backed up the map and saved the replacement; all five saved views were inspected. Fresh run `Z03FloorsFresh-20260914-215343-ebeb675e` completed with 77 passing architecture reports, 3140 actors, a clean map and no Python errors.

Live traversal, final material and lighting quality, Chaos destruction and packaged performance remain unqualified. Technical labels, guidance strips, remaining fixtures and the Z04 frontage still need work. The supported slice estimate remains 63.75%; the 90% goal is open.
