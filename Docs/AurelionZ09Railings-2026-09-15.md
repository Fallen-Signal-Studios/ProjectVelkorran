# Replace doubled approach rails with a fitted Aurelion balustrade

The wound-gallery approach bridge still carried eight stretched vendor
`SM_Scifi_Railing_4m` instances in the visual-only HISM on
`Aurelion_Art_M12_Z09_72_ee4638`. They formed four doubled runs: each pair
shared rotation and scale and sat 11 cm apart along its local thickness.

`Z09RailAudit-20260915-114428-c7785879` re-measured the saved map with the new
read-only `audit_z09_railings.py`; its baseline matched the kit's recorded
`rail-baseline.json` field for field, exited 0 and reported no Python errors.
The component is NoCollision; native approach barriers remain authoritative.

## Source

`build_z09_railing_kit.py` authors a true-size 4.75 x 0.22 x 1.30 m module,
the measured union envelope of each doubled pair, so placement uses unit scale
rather than the vendor's stretched scale. Three dressed ivory posts repeat the
gallery's engaged piers with recessed flutes and bronze registers; a dark grip
with gold conductors, a lower tie, twelve balusters and four pointed upper
registers keep the metalwork open. 18,500 source triangles, two UV layers,
three material slots, no collision hulls; Nanite uses position precision 10 and
full fallback geometry.

The first source render showed three defects, corrected before any import:

- Balusters stopped 3 cm below the grip. They now run from inside the tie into
  the grip.
- Lower ferrules matched the tie depth, giving 24 same-facing coplanar
  overlaps. Ferrules are now 39 mm deep against the 45 mm tie; the audit
  reports zero overlaps.
- Pointed register legs ended in open space beyond the balusters. The
  registers are now recessed within baluster depth and seat into both outer
  balusters.

`verify_z09_guardrail_joints.py` ray-tests the exported FBX: 103 samples per
baluster from tie to grip with no gap, all eight register legs present inside
and absent beyond their balusters, and 24 clear mid-gap sightline samples. The
clean FBX round trip also passed. These are visual-mesh checks only.

## Placement

`rail-fit.json` maps the eight originals to four placements at each pair's
midpoint. The existing actor, component, transforms and collision settings are
retained; only the mesh and instances change.

`Z09RailPreview-20260915-114609-71381ab6` passed every envelope comparison
(maximum world-bound error 0.0000307 cm), unchanged 3,140 actor/collision
states, no Python errors and no guardrail import warnings. Its south approach
camera sat behind geometry and was moved onto the deck; the rail-detail and
gallery-return views were inspected. No map was saved from the preview.

Save run `Z09RailSaved-20260915-114945-7cd06d3e` backed up and saved the map
with the same fit results, unchanged 3,140 actor/collision states and no Python
errors. Its on-deck, rail-detail and gallery-return captures were inspected:
balusters meet the grip, registers read within each bay and both runs frame
the doorway approach.

Fresh reload `Z09RailFresh-20260915-115205-7f55c5a5` passed the full preceding
Z09 architecture chain plus the guardrail check, with clean map packages, 107
JSON reports besides the launcher records, exit 0 and no Python errors. Captures,
fit and verification receipts are in
`Docs/Validation/AurelionZ09Railings-2026-09-15/`. These are static placement
and asset checks, not a live route or collision-contact test.

Scripts: `audit_`, `check_`, `preview_`, `save_`, `verify_` and
`review_z09_railings.py` under `Scripts/Editor/`. `verify_z09_railings.py`
extends the Z09 chain after the slit fissures.

Module joints place two end posts back to back, as in the Z08 guardrail.
Lighting, live traversal and collision contact, performance and the broader
90% alignment goal remain open; the alignment estimate is unchanged.
