# Companion primary attack repair

Fresh saved-content inspection found a second allowlist regression after the
native melee migration. Both M12 and M13 still curated Tarrik's old Narrative
sword combo and Selene's old Verity Twin Blade combo, while their weapon items
now grant `GA_Tarrik_MeleeLight` and `GA_Selene_MeleeLight`. The companion snapshot
requires a weapon grant to appear in the mission's curated list. An equipped
weapon therefore did not imply that its actual primary attack was permitted.

`CompanionContract-20260919-182919-aefd62d2` records the mismatch for both heroes
and missions. The editor census found no resonance target components on the
placed enemy actors; the previously considered default `requires_player_finish`
property does not explain this saved-content mismatch.

`align_companion_primary_grants.py` derives each primary attack from the actual
weapon's `Narrative.Input.Attack` grant and validates its native melee definition.
It replaces only the known legacy primary in each companion profile, preserving
the defense and unarmed entries, character identity and anchors. Full profile
text comparisons reject any other changes. No progression abilities are added.
Backups precede each save. The native melee authoring recipe now calls the same
alignment after changing weapon grants, preventing recurrence on regeneration.

`CompanionPrimaryRepair-20260919-183329-94f445f0` saved all four replacements in
the two mission definition assets. No map, weapon or attack definition was saved.
The existing native light definitions retain their authored montage chains,
including Selene's Twin Blade animations. Runtime damage remains pending.

Baseline full gate: `20260919-182516-2dc7bde8`, build without SkipBuild and all
719 matching automation tests passing. Fresh route and post-change gate pending.

## Fresh gameplay result

`CompanionPrimaryRoute-20260919-183439-ce4bd05a` reloaded all four matching
mission/weapon pairs and observed the new curated grants on actual companion
instances. Entry, E1, E2, meeting, rescue, E4 entry and E4A passed. Both weapon
montages now execute: Tarrik's `AM_Sword_3P_1H_Attack_1_Tarrik` and Selene's
`AM_VerityTwin_01`. The earlier route recorded neither. This verifies restored
weapon attack selection and animation execution, not damaging hit contact.

The native damage-share observer still recorded zero applied companion damage.
Targets were moving during swings; observed montage distances ranged from
134.55–173.29 cm for Tarrik and 79.79–276.68 cm for Selene. Neither a range nor a
collision cause is established. Hit contact and damage remain open. E4B ended
on an actual player defeat; no M13 completion is claimed for this run.

The public checkpoint retry loaded this session's earned generation 20 arena-entry
save (`M12_E4_QuarantineCrucibleB`). One native success callback, a new world,
Tarrik ready at 100 health and the nineteen-beat journal were recorded in
`EarnedCheckpointRetry/reload.json`. This is a point-in-time readiness check,
not stable recovery acceptance. The encounter later showed its failed/retry
state while the player remained idle; no combat retry was executed. Cleanup
initially encountered renewed viewport capture after travel, then released the
cursor and stopped PIE through the toolbar. No post-load gameplay pass is claimed.

Post-change gate `20260919-185242-7bc8b752` passed the build invocation without
SkipBuild, all 719 matching automation tests, report coverage and source
integrity. No tracked edits occurred during validation. The fresh route editor
exited normally, with no Python errors. Syntax and whitespace checks passed.
The creator's M12 map retained SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
