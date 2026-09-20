# E4B retry loses the configured thermal director

Verified in `Saved/Validation/Aurelion/E4BRetryBindingVerified-20260920-155430-a983f264`.
The public checkpoint load restored genuine generation 20, M12_E4_QuarantineCrucibleB,
ArenaEntry, Tarrik, Standard. The two original checkpoint banks were copied unchanged
and their SHA256 values recorded. Ordinary look/Interact inputs activated the authored
Retry control; no StartEncounter, actor transform, collision, resource or proof writes occurred.

For eight seconds after the native retry entered Active, the replacement Elite stayed alive.
Its thermal component had `EncounterDirector == nullptr`. Its frost anchor DID restore to
the unique authored TargetPoint_9 with tag Aurelion.Crucible.CleanFrost, and LastError was empty.
MovePartner, FrostSetup and HeatConfirm all returned null from GetCurrentThermalTarget
throughout that observation. This prevents all three contextual controls from resolving a
usable target after retry. The original movement-stall investigation cannot continue through
this restored route until that separate native defect is repaired.

The source explains the mismatch:

- USovAurelionThermalFractureComponent treats EncounterDirector as an optional authoring
  constraint. InitializeBindings resolves the unique registering director into BoundDirector
  but does not assign the optional EncounterDirector property.
- That optional property is not SaveGame. A replacement Elite does not retain the level
  instance's authored pointer.
- USovAurelionEliteThermalFracture restores the frost anchor by saved unique tag, so the
  anchor restoration is working and should not be replaced.
- ASovAurelionRequestActor::GetCurrentThermalTarget requires the optional property to equal
  its own stable ThermalDirector, despite having already resolved the current participant.
- ThermalReplacementRequiresFreshInput manually assigns EncounterDirector on both test
  actors, so it does not cover the actual replacement configuration. The anchor reconstruction
  test also does not test request target resolution.

## Implemented repair, explicitly approved by the user

The user approved the E4B native retry binding repair on September 20. The repair
publishes EncounterDirector only after InitializeBindings validates standalone authority,
the owner ASC, unique participant registration, the thermal proof type and any existing
explicit constraint. A non-null wrong director and ambiguous ownership remain rejected.
The reference is also republished when the internal binding already matches.

The phase director now caches FractureSource only after successful initialization. A
component that is not ready on its first attempt is retried on the next active tick;
a successfully bound component retains the existing identity fast path. Frost-anchor
restoration, saved schema, request identity fencing and thermal receipt rules are unchanged.

ThermalReplacementRequiresFreshInput now serializes the derived Elite component with
the actual SaveGame archive flags, constructs its replacement without authored references,
and restores the anchor through Load. It checks delayed roster publication, all three
thermal controls, wrong explicit directors, duplicate ownership, recovery after ambiguity,
and the already-bound reference path. Existing assertions still require fresh input after
replacement or attempt change and forbid fabricated journal/fracture proof. Separate
role restoration coverage continues to reject duplicate frost anchors.

Full validation `20260920-160354-3c80f5b6` passed the native build, all 722 matching tests,
coverage reporting and source-integrity checks.

## Evidence qualifications

`E4BCheckpointObstruction` stopped because the restored encounter requires its normal retry.
`E4BCheckpointReplay` then hit the missing director assertion immediately after retry.
`E4BRetryBindingReadback` stopped on an unavailable Python NPC readiness accessor and is not
settled-state evidence. The final `E4BRetryBindingVerified` uses reflected, available APIs and
captures the persistent null binding over eight seconds. Its failed result is an intentional
gameplay readiness assertion, not a successful E4B completion.

The earlier full route's movement samples show Selene already at her clean mark, accepted
MovePartner and FrostSetup requests, and an Elite approximately 68 cm from the stationary
player. This is evidence of likely hostile capsule obstruction, not companion-follow failure.
The later retained-session sweep was taken after the player returned to the arena entry;
it cannot establish the original stall's cause. No movement/collision fix is claimed.

## Post-fix live checkpoint verification

`E4BRetryBindingFixed-20260920-160541-7b770dcd` restored the same unchanged generation-20
banks and entered the authored Retry control through ordinary held interaction. At the
first post-retry sample (0.046 seconds), the replacement Elite had the correct director,
the original unique frost anchor, and all three controls resolved its exact component.
LastError was empty.

Normal inputs then earned MovePartner (6.406 seconds), FrostSetup (11.672 seconds,
refreshed at 16.938), and HeatConfirm (18.219). The native thermal receipt completed at
18.250 seconds, applied 99 payoff poise damage, broke poise and exposed the Core. A real
Cinderline Core hit broke that zone at 18.484 seconds, after the thermal receipt.
No direct transforms, health, collision, abilities, campaign proof or checkpoint-bank
contents were changed. The run's asset-integrity check passed.

The full combat replay FAILED at 37.609 seconds because Tarrik died. It is not an E4B
victory or full-mission pass. The binding/control/thermal/Core repair is verified; combat
survival and the earlier intermittent capsule-obstruction investigation remain open.
The terminal screenshot was reviewed: enemies crowd Tarrik in melee, with the HUD's
critical-health vignette visible. A large corpse-interaction outline also warrants a
separate player-facing visual polish pass. This screenshot does not qualify overall
combat animation or AAA visual quality. The editor exited normally without saving M12.
## Subsequent ordinary E4B victory

`InteractionBracketsVisible-20260920-161801-848a4a9a/E4B/e4b-input-continuation.json`
records a completed ordinary-input replay from the same generation-20 checkpoint.
It finished in 33.344 seconds with the real frost/heat payoff, a subsequent Cinderline
Core break, all five required hostiles defeated, all seven protected characters alive,
and Tarrik alive at 100 health. Native journal sequence 20 is ThermalFracture for the
same attempt (38C08B314FE077A6F1B269BFE590A68E). The content hash check passed.
This verifies E4B victory after the binding repair. The earlier fatal run remains retained;
it is not rewritten as a pass, and the intermittent movement stall is not claimed fixed.
Later mission scenes, travel and M13 remain outside this replay's proof.

The containing interaction-HUD observer reported failure because it found no fully
on-screen corpse focus before the gameplay driver finished. That observer failure does
not invalidate the independently completed gameplay report; it provides no bracket
visual acceptance. A separate review restores this run's earned victory autosave.
