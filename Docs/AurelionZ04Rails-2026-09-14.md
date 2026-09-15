# Z04 relay ramp and balcony rails

Ten fitted custom runs replace all 35 vendor rail instances on the existing art actor. Four twelve-metre ramp runs use vertical stone posts, slender metal infill, sloped grips and narrow gold registers. Six balcony runs retain the original perimeter and both six-metre access openings. Ramp grip height stays 130 cm above the slope; balcony rails remain 110 cm high.

The revised recipe seats post capitals and infill into the grip. Ramp foot shoes follow the actual slope rather than sitting as flat boxes above it. Three corner runs have their bases shortened by eight centimetres; narrow grip tongues reach the perpendicular grip face at 4.96 cm so the joints meet without overlapping the post bases. The original native guards remain unchanged and all new rail meshes are noncolliding.

## Verification

- `Z04RailBaseline-20260914-235015-00b7423a` verified all 35 saved vendor transforms before replacement.
- All seven clean-FBX geometry and UV checks passed. The modules use two UV channels, Nanite precision 10 and full fallback geometry.
- 175 clean-FBX grip-height probes and 102 foot-seating probes passed.
- Individual and assembled scoped coplanar audits report zero overlaps. These check same-facing axis-aligned convex faces; they are not exhaustive mesh-intersection tests.
- The engine checker compares each new run with its original group, preserving transverse route widths and explicitly accounting for the trimmed corners. It checks both ramp clear widths at 554 cm, both balcony openings at 600 cm, and 55 post-foot contacts against native floors.

Engine preview `Z04RailsPreview-20260914-235926-e32adf71` and saved-map run `Z04RailsSaved-20260915-000908-e5d650e2` both completed with terminal exit 0 and no Python errors. All five views from each run were inspected: south ramp, landing, corner, west ramp and a close foot detail. The saved view confirms joined grips and sloped foot seating. Floor overexposure, lighting grain and the unfinished surrounding props remain visible. The map was backed up before saving.

Fresh saved-map run `Z04RailsFresh-20260915-001225-d5bde519` completed with terminal exit 0 and no Python errors. All 82 architecture reports passed, including the new rail check; the map remained clean with 3140 actors. Live traversal, camera clearance and packaged performance remain unqualified. The inherited Z03 source recipe has the earlier capital and foot geometry; this pass corrects Z04 only, and the corresponding Z03 correction requires its own asset rebuild and checks.

Z04 piers, props, fixtures, guidance and final lighting still need work. Full-map fidelity, supporting character likeness and campaign Chaos rollout remain open. The supported slice estimate remains 63.75%.
