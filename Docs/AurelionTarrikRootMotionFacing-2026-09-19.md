# Tarrik companion root-motion facing

Fresh-route observation isolated a content mismatch: the player Tarrik movement
component permits physics rotation during animation root motion, while the owned
Tarrik companion did not. The companion uses the existing smoothed controller
focus to face an enemy, so suppressing that rotation freezes his facing during a
root-motion swing even when his target moves across him.

## Before

`FreshTarrikContactRoute-20260919-232739-fd7128c5` reached E4A through the normal
campaign input drivers. Its passive contact observer recorded 183 samples, 25
with authored blade endpoints, no observer errors and no companion damage.
Tarrik drew Velkorran and activated native light melee with
`AM_Sword_3P_1H_Attack_1_Tarrik`. His yaw stayed at -174.510 degrees throughout
that montage. The target began roughly 26 degrees off his facing and crossed to
roughly 70–85 degrees during the active window. Separation during the sampled
active poses was approximately 96–107 cm. These sampled poses are not exact
collision-sweep evidence.

That route passed E4A and E4B but later failed the M13 hold-scene driver. It is
not a full-route pass. Direct editor quit during travelled PIE also produced an
editor teardown ensure; future cleanup must end PIE before requesting quit.

## Saved change

`enable_tarrik_companion_root_motion_facing.py` changes only
`BP_AurelionTarrikCompanion`'s CharacterMovement setting
`allow_physics_rotation_during_anim_root_motion` from false to true.
`TarrikRootMotionFacing-20260919-234526-56d6d2ff` saved the asset, confirmed the
player already uses true and remained unchanged, and exited normally.
Attack definitions, damage, ranges and source code are unchanged.

## Verification scope

Fresh after-run: `TarrikFacingAfter-20260919-235104-ebe73560` passed entry, E1,
both E2 receivers, meeting, rescue, E4 entry and E4A through the real Tarrik
handoff. The requested E4A diagnostic endpoint stopped the chain correctly.
The passive contact observer does not command the companion or apply damage.
The diagnostic wrapper requests an E4A endpoint from the normal route chain;
later stages are explicitly not qualified by that shortened run. The default
chain still runs all eight stages, and CP9 probing requires the final stage.

The contact observer captured 161 samples, 36 with a montage, no errors and no
Tarrik damage receipts. Every sampled live movement setting was true. His first
swing changed yaw from -1.46 through -18.07, -26.11 and -28.99 to -32.30 degrees,
matching controller yaw. This verifies the turning correction, not reliable
melee contact. A second swing closed from approximately 180 cm to 78 cm during
its attack, also without damage. The first swing's sampled montage positions
jumped from .205 to .631 seconds, so that window cannot support detailed trace
conclusions. Native trace timing/pose contact remains an open issue.

The general observer separately recorded three Selene damage receipts after the
handoff, including a fatal Linkbound hit. PIE returned to the editor world before
editor close. No map was saved, and the protected M12 disk hash is unchanged.

Baseline full gate: `20260919-232427-286930b4`. Post-change full gate:
`20260919-235836-9fdefbd3`, build without SkipBuild, 719 passing tests, coverage
and source integrity. No tracked edits occurred during the gate. Documentation
was updated with the completed runtime results afterwards.

## Separate presentation finding

The large rectangular interaction overlay visible at the arrival terminal is
the native accessibility interactable-bounds outline, enabled by default. It is
not authored in the holographic widget. Changing its shape requires source work
outside the content-only handoff; disabling a player's accessibility setting is
not a substitute for redesigning that presentation.
