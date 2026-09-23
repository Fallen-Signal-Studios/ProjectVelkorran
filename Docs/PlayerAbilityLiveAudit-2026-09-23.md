# Player Echo ability audit — 23 September 2026

The current ten-ability roster is the approved replacement in
`Docs/CampaignV2ChangeLog.md`, rather than a mismatch with the August TDD's
prototype names. This audit covers the abilities granted in the current M12
Tarrik and earned E2 Selene states. It distinguishes a live input/cast/projectile
smoke pass from proof that a stationary validation Enforcer actually took damage.

## Changes

- Repaired Cinder Sticky Grenade's saved A/A montage pair to A/B. Its saved
  weapon metadata now names `WI_Velkorran` and `WI_Cinderline` instead of the
  obsolete sample-pack rifle. The native grenade remains universal.
- Added an owner-local cast Niagara burst after successful authored montage
  playback for all ten abilities. Tarrik uses scaled Fire effects; Selene uses
  Ice for Verity/Stillpoint/Staccato/Dispatch and Lightning for Axiom.
- Bound native Selene projectile flight, field, recall, and impact Niagara to
  the three project-owned projectile Blueprints. Requiem's project-owned line
  Blueprint presents its chained Fire detonation nodes. The existing Tarrik
  grenade and Hunger projectile Blueprints retain their explosion/impact effects.
- Corrected Verity's Wake's live lane collision: its wide sweep finds targets,
  while a narrow sweep decides whether a wall blocks forward travel. Per-target
  line of sight remains required, and a centered wall still stops the wave.
  At M12 E2, the prior 500 cm wide blocker test touched a low guard at release,
  then the edge of the full-height `Z04_Wall_S1` one meter later, although the
  center aim line passed both walls.
- Aligned Cinder Sticky Grenade movement collision with the character aim trace
  by ignoring attached and child weapon actors when the projectile launches.

## In-engine evidence

The PIE probe uses the ready player pawn, its inventory/wield state, semantic
controller input, and GAS. Only Echo refill and the stationary Enforcer target
are test fixtures. Each ability is activated twice to observe the A/B pair.
An exact projectile Blueprint class is checked where one should exist.

| Context | Observed result |
| --- | --- |
| Selene universal Stillpoint with Verity, Staccato, and Axiom | Six of six activations spent 35 Echo, alternated casts, spawned the ice projectile and field Niagara, and reduced the target shield from 250 to 126. |
| Selene Verity's Wake | After the collision repair, both A/B activations spent 30 Echo, spawned the wave and Ice cast FX, and reduced the E2 target shield to 180/170; source damage receipts included the 50-point Wake hit and Frost ticks. |
| Selene Staccato Zero and Axiom Null Pulse | Both A/B pairs spent 30 Echo, showed their cast Niagara, and damaged the target. These are authored hitscan/pulse payloads, so no projectile actor is expected. |
| Selene Dispatch | Both A/B pairs spent 90 Echo, spawned the returning projectile with Ice flight/hit and Lightning recall FX, and dealt 70 shield damage. |
| Tarrik universal Cinder Sticky Grenade with Velkorran | Both A/B casts spent 35 Echo, spawned the exact grenade and Fire explosion FX, and damaged the target. |
| Tarrik Velkorran's Hunger | A focused fresh-mission replay spent 50 Echo twice, alternated A/B, spawned the exact projectile and Fire FX; the second cast dealt 75 shield damage. |
| Tarrik Cinder Slam | Both A/B casts spent 90 Echo, showed the Fire shockwave and dealt 75 shield damage to a close M12 Enforcer. |
| Tarrik Cinder Judgement | Both A/B casts spent 50 Echo, showed the Fire muzzle effect and dealt a 90 direct shield hit plus 56.25 splash to a close M12 Enforcer. As an authored hitscan attack, no projectile actor is expected. |
| Tarrik Cinderline Requiem | Both A/B casts spent 90 Echo, spawned the exact line actor with Fire node FX, and dealt a 100 direct hit, 60 chained hit, and two 5-point follow-up hits to a close M12 Enforcer. |
| Tarrik grenade with Cinderline | After attached-weapon collision was ignored, both A/B casts spent 35 Echo, spawned the exact grenade and Fire explosion FX, and reduced the 900 cm target shield to 208.3/208.25, matching the Velkorran context. |

The first full Tarrik pass was partial: a live knockdown/get-up animation
interrupted Hunger's first input before Echo commit. A clean focused Hunger pass
then completed A/B. A closer Cinderline run had a similar live interruption of
Judgement's second input. These are not counted as successful casts.

Evidence: `Saved/Validation/Aurelion/SeleneAbilityContextFinal-20260923-074741-821790c1`,
`Saved/Validation/Aurelion/WakeCenterlineFinal-20260923-080835-386d1089`,
`Saved/Validation/Aurelion/TarrikAbilityContextFinal-20260923-081155-c14b9ab8`,
`Saved/Validation/Aurelion/HungerFocusedPIE-20260923-081520-ec7637dc`,
`Saved/Validation/Aurelion/CinderlineGrenadeWeaponFix-20260923-083212-edb992fc`,
`Saved/Validation/Aurelion/SlamNearImpact-20260923-084710-f9ad60ca`,
`Saved/Validation/Aurelion/JudgementNearImpact-20260923-084537-1dcf9134`,
`Saved/Validation/Aurelion/RequiemNearImpact-20260923-084851-786df1af`.
The initial full Tarrik lane placed some synthetic targets behind M12 cover;
the focused close-range replays above are the impact qualification for those
three abilities.

Visible PIE captures were taken during Tarrik's Cinderline grenade cast and
Selene's Verity's Wake cast:
`Saved/Validation/Aurelion/GrenadeVisualPIE-20260923-083741-60ada759/Tarrik-CinderStickyGrenade-Cinderline-cast.png` and
`Saved/Validation/Aurelion/WakeVisualPIE-20260923-084009-4c5c4656/Selene-VeritysWake-Verity-cast.png`.
They show palette-aligned Fire/Ice accents on the dressed protagonists and the
current gold/cyan HUD. The bursts are restrained in these frames; the objective
label remains a flat dark plaque, and the level still includes conspicuous
graybox surfaces. These images do not establish a 90% visual-alignment score.

## Automated and asset checks

- The UE 5.7 Development Editor build and nine
  `ProjectVelkorran.Campaign.SelenePayload` tests passed. The new
  `WakeOverCoverClearance` regression confirms that lateral cover cannot kill the
  entire wave; the existing `WakeLaneAndWall` still confirms a centered wall
  protects a downstream enemy. Report:
  `Saved/Validation/20260923-080736-461dd037/AutomationReport/index.json`.
- The UE 5.7 Development Editor build and three
  `ProjectVelkorran.Campaign.Tarrik` tests passed after the grenade collision
  change. Report:
  `Saved/Validation/20260923-083018-de551c5c/AutomationReport/index.json`.
- Ten `ProjectVelkorran.Campaign.Transactions.Actions` tests passed after a
  stable-source rerun, including the authored Echo cast playback case:
  `Saved/Validation/20260923-083652-f75d4735/AutomationReport/index.json`.
- A clean editor reload checks all ten distinct A/B pairs, cast Niagara,
  projectile class bindings, phase/impact Niagara, and canonical grenade
  metadata. It passed with no issues:
  `Saved/Validation/Aurelion/PlayerAbilityCleanReload-20260923-083401-5da34a63/player-ability-assets.json`.

The live probe does not certify multiplayer prediction, packaged builds,
every animation pose/FX combination, or every encounter angle. The Fab
Fire/Ice/Lightning Niagara packs are local project dependencies excluded by
the repository's existing ignore rules. This slice does not assign a new
percentage to the broader M12/M13 90% visual/gameplay goal.

## Zero-Echo rejection and immediate recovery

The same PIE probe now has an optional `SOV_PLAYER_ABILITY_NEGATIVE_ECHO=1`
phase. It sends each semantic ability input with the ready protagonist at zero
Echo, observes for 1.5 seconds, then restores 100 Echo and sends the same
input normally. The zero-Echo check requires no debit, no cast montage, no
projectile, and no source-damage receipt on the newly spawned aimed target.
Damage to other encounter actors is recorded separately because an earlier
Stillpoint field can continue ticking after its successful cast; it is not
evidence that the new zero-Echo input activated. The ordinary follow-up still
requires the correct A/B montage, cast Niagara, expected Echo spend, and
projectile actor where appropriate.

`TarrikFullLowEchoPIE-20260923-140908-ccff051e` passed all 12 repetitions
across his six weapon contexts. `SeleneFullLowEchoIsolatedPIE-20260923-141606-cd49bd39`
passed all 14 repetitions across her seven contexts after loading an earned
E2 checkpoint and using its normal retry. The first Selene run stopped on a
probe false positive: the previous Stillpoint field damaged two unrelated
encounter enemies during the next zero-Echo window. The revised run kept
three such unrelated receipts visible while requiring the newly aimed target
to remain unharmed; all 14 checks passed. A focused Hunger and Wake run also
passed before the full matrices. The ability inputs were routed through the
normal player controller; only the Echo level and stationary validation target
were fixtures. These runs establish rejection at zero Echo and immediate
activation after refill for the current roster. They do not establish every
positive cast's impact, interruption/death behavior, or resource-state
correctness under client prediction.

Focused [live cancellation checks](Validation/PlayerEchoInterruptPIE-2026-09-23.md)
subsequently passed Tarrik's delayed Hunger release and Selene's held Axiom
charge twice each. They verified one committed debit, no payload before
release, cleanup of active/Busy state, and a working B recast with target
damage. Wake's instantaneous release offers no active post-input cancellation
window. Natural damage, weapon-change and death interruptions remain open.
