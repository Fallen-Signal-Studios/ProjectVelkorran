# Relay-overlook custom piers

The four vendor columns in `Aurelion_Art_M12_Z04_18_d57dbf` now use the existing
custom Blender module `SM_Aurelion_KIT_Z08EngagedPier`. Its cut-stone courses,
stepped plinth/capital, narrow recessed conductors and mechanical access panel
continue the Aurelion architecture vocabulary. No duplicate mesh or scaled
variant was created.

The source remains `Art/Source/Aurelion/Z08ColumnKit/Aurelion-Crucible-Pier.blend`
and its existing FBX: 21,064 triangles, two UV channels, three materials and no
collision hulls. The saved mesh file hash is unchanged by the placement script.
Its existing automatic Nanite precision and fallback settings are preserved;
this pass does not claim the newer precision-10/full-fallback configuration.

## Placement and scope

All four instances use unit scale and the existing 1.2 x 1.0 x 7.0 metre module.
They face inward at X=3975 (yaw -90) and X=10025 (yaw 90), at Y=-11700 and
Y=-10500, with their feet at Z=0. Each measured world envelope matches its
original vendor instance within 0.02 cm. The bounds are X=3925..4025 or
9975..10075, Y=centre +/-60, and Z=0..700 cm.

Actor/component transforms, actor collision and all other actors' captured
collision/transform state remain unchanged. The replacement component explicitly
uses `NoCollision`, with no material overrides. Native room obstruction remains
authoritative. The map retains 3140 actors.

These structural piers stay intact. This does not implement the separately
requested destruction of appropriate cover or nonstructural partitions.

## Evidence

- `Z04PiersPreview-20260915-014615-6684d791` stopped on an overly strict new
  assertion that assumed this older shared mesh used precision 10. No map was
  saved. The checker was corrected to inspect and preserve the reused asset's
  actual settings instead of silently modifying all its other placements.
- `Z04PiersPreviewVerified-20260915-014808-b226e36a`: placement checks passed,
  four images inspected (west base, west capital, east, room), exit 0 and no
  Python errors.
- `Z04PiersSaved-20260915-015130-283971af`: map saved after preserving a backup,
  four saved-version images inspected, exit 0 and no Python errors.
- `Z04PiersFresh-20260915-015414-3388d5d9`: reopened saved M12; all 86
  architecture verification reports passed, 3140 actors and clean map,
  exit 0 with no Python errors.

The review covers local fit and presentation. It does not establish final AAA
art quality, dynamic lighting/performance, complete campaign traversal or 90%
TDD alignment. The companion attack repair still needs a successful fresh
handoff and source-attributed enemy damage; its latest E1 defeat is preserved
in `AurelionCompanionCombat-2026-09-13.md`.
