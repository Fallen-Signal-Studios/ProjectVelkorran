# Corruption engineering contract

This implementation follows the August 2026 TDD v2 §§7.7–7.8. It provides authoritative, mission-scoped exposure, source handles, band state, GAS ownership, checkpoint persistence and accessible presentation requests. Narrative owns disk serialization and campaign facts. The supplied source producer is a real environmental contact volume.

## Native ownership

| File / class | Responsibility |
| --- | --- |
| `Public/Components/SovCorruptionComponent.h`, `Private/Components/SovCorruptionComponent.cpp` | Exposure records, permission checks, contact handles, band transitions, owned GAS effect, presentation, save/load barrier. Constructed on `ASovPlayerCharacterBase`. |
| `Public/Corruption/SovCorruptionProfile.h`, `Private/Corruption/SovCorruptionProfile.cpp` | Explicit source identity, target identities, rate/contact amount, spatial policy, mission caps, remedy, presentation, accessibility, checkpoint and canon consequence contract. |
| `Public/Corruption/SovCorruptionSourceVolume.h`, `Private/Corruption/SovCorruptionSourceVolume.cpp` | Authority overlap producer, radius/falloff/visibility verification, stale handle repair and source cleanup. |
| `Public/Effects/SovGameplayEffect_CorruptionBand.h`, `Private/Effects/SovGameplayEffect_CorruptionBand.cpp` | One infinite owned GAS effect granting the current band tag. The component removes only the handle it applied. |
| `Private/Corruption/SovCorruptionMath.h` | Production-used finite tuning validation, hysteresis, cap arithmetic and radial falloff. |
| `Private/Tests/SovCorruptionRuntimeTests.cpp`, `SovCorruptionRuntimeTestFixtures.h/.cpp` | Real world, collision, player component, Narrative ASC and campaign state integration fixtures. |
| `Tests/Portable/SovCorruptionMathTests.cpp` | Engine-independent boundary checks of the production math. |

All `Public` and `Private` paths above are relative to `Source/ProjectVelkorran`.

## Contact and exposure

An active mission is mandatory. A profile is dormant unless its source ID, allowed Tarrik/Selene identities, exact mission permission, non-clear cap, finite tuning, remedy text, presentation ID, ordinary information and reduced-effects substitute validate. No opening mission is implicitly permissioned for Eclipse exposure.

`AcquireSource` accepts an actual `ASovCorruptionSourceVolume` in the same world. The volume must overlap the player, satisfy radius/falloff and pass its authored visibility policy. The player's ASC must still own that avatar, be alive, and carry the active mission's protagonist identity. A raw source ID or a client request cannot manufacture contact.

One handle represents one source/profile/mission contact. Repeated acquisition returns the same handle. Instant contact exposure applies once while a profile has continuing contact; overlapping fields with the same profile can each contribute their sustained rate. Every contribution respects the smaller profile/mission cap and the global 0–100 limit. Sources release handles on leave, reassignment and destruction; the component independently removes stale or invalid handles.

Default bands are Trace at 1, Intrusion at 25, Contest at 50 and Overwrite risk at 80, with 5 exposure points of downward hysteresis. Thresholds and hysteresis must be finite and strictly ordered. A cap ends immediately below the next band's representable threshold. These are engineering defaults requiring mission tuning.

Leave-field profiles dissipate at their configured recovery rate once all contacts for that profile have ended. Other remedy types use an authority `CleanseExposure` call from the successful native or Blueprint remedy action. An authored `EscapeBeatId` additionally disables that source and removes its gameplay exposure when the existing campaign beat is complete. Source loss alone does not invent completion of a destroy-node, break-link or objective remedy.

## GAS, story facts and feedback

The component grants exactly one of these tags through its own `USovGameplayEffect_CorruptionBand` instance:

- `Sov.State.Corruption.Trace`
- `Sov.State.Corruption.Intrusion`
- `Sov.State.Corruption.Contest`
- `Sov.State.Corruption.OverwriteRisk`

Clear removes that owned instance. Actual Intrusion vulnerability, Contest ability pressure, and Overwrite failure clocks or boss phases must be authored against these states using the existing GAS and campaign systems. The native band effect itself changes no attributes, input, camera or damage. Corruption is not implemented as a second health or poison attribute.

A canon-persistent profile names an existing non-interactive, non-cinematic consequence beat. Only that profile's own contribution and permitted cap can qualify for its consequence threshold. Aggregate exposure from a different source cannot supply that proof. The request then passes through `USovCampaignStateComponent::CompleteBeat`, preserving prerequisites, protected facts and idempotence. Gameplay cleansing leaves those durable campaign records intact. Restoring exposure does not replay the consequence request.

`OnPresentationRequested` supplies the band, exposure, profile IDs, remedy text, information text and suggested intensity. `SetReducedEffects(true)` preserves the numeric state and remedy, substitutes the authored accessible information, and sets suggested distortion intensity to zero. Presentation callbacks do not drive gameplay state. These APIs introduce no input delay, reversed movement, automatic dialogue selection or fabricated system UI. The player accessibility settings/HUD/audio/VFX integration must bind this contract and verify equivalent information.

## Checkpoint and destination restore

Only profiles explicitly setting `bPersistExposureAtCheckpoint` enter the SaveGame record. Records contain profile identity, source mission, exposure and whether the profile was in contact. The saved band preserves hysteresis. Runtime actor pointers, source handles and active-effect handles are rebuilt. Invalid schema, duplicate profile IDs, invalid profile contracts, non-finite values and exposure totals above 100 fail validation.

Managed protagonist loading restores PawnRecord before the controller's destination campaign record. `Load` therefore holds the saved exposure without applying it to the temporarily active source/null mission. While this barrier is pending, ticks, source acquisition and cleanse calls cannot mutate exposure; another checkpoint preserves the pending data.

The campaign controller performs this order:

1. Restore the matching protagonist snapshot and controller campaign record.
2. Successfully admit `PendingMission` through `CampaignState->BeginMission`.
3. Call `Corruption->FinishCampaignRestore(PendingMission, Error)` and check success.
4. Publish character readiness and release transition input.

The barrier accepts a valid, dormant fresh pawn as well as a loaded pawn. It validates ownership, exact protagonist identity, the authoritative mission and restored source permissions; removes exposure belonging to other missions; restores hysteresis; and requires the owned band effect to exist before reporting success. A bad snapshot keeps managed initialization from completing. Continuing real contact suppresses a duplicate instant award; that suppression expires when the actual field contact ends.

An already-ready pawn's encounter restore finalizes synchronously in `Load`. A legacy initialization path can finish on its first ready tick once campaign state exists. It remains dormant while no mission is available. Legacy paths do not provide the managed controller's before-input readiness guarantee; campaign maps should use the native campaign game mode/controller.

## Content integration gates

For each authored Eclipse encounter:

1. Create a validated `USovCorruptionProfile`; explicitly name the allowed mission, protagonist, cap, remedy and both information presentations.
2. Place/configure `ASovCorruptionSourceVolume`, its position and occluding collision. Use a distinct profile `SourceId` for a distinct exposure contract. Reusing an ID for different profile assets is rejected while exposure exists.
3. Connect successful non-leave remedies to `CleanseExposure`, or assign a valid durable escape beat. Validate the remedy remains achievable when the source owner dies or a checkpoint is retried.
4. Author required combat constraints as GAS consumers of the band tags and mission-specific failure/phase logic through the existing mission system.
5. Bind the presentation request and persisted accessibility setting. Test remedy comprehension with minimum visual effects.
6. Decide checkpoint persistence explicitly. Canon consequences and gameplay exposure are separate decisions.

Enemy-hit and command-link exposure producers, contaminated NPC/companion target types, and machinery interaction producers still need explicit native or authored integrations. The Corruption damage channel is not automatically routed into this environmental exposure component. Source contracts for those integrations must verify a real damage/link/interaction receipt and reuse this ownership model; a freely callable amount award is insufficient.

## Validation and current evidence

The portable production math test was compiled with `g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=undefined -fno-sanitize-recover=all` and executed successfully in this workspace. It covers finite threshold validation, hysteresis, source caps, invalid amounts and radial falloff. The full portable runner is `python3 Scripts/Test-NativePolicies.py`.

Seven Unreal integration tests are authored under `ProjectVelkorran.Campaign.Corruption`:

- `PermissionAndContact`: mission gating, actual overlap, instant contact deduplication, forged out-of-field rejection and escape.
- `SnapshotAndAccessibility`: committed snapshot visibility during callbacks, band/remedy parity, owned effect removal, contact restore deduplication and invalid schema rejection.
- `ConsequenceMissionCap`: one source cannot borrow another source's band to commit a fact; cleanse preserves the committed fact.
- `ProfileValidation`: explicit source and accessible substitute requirements.
- `OcclusionCapAndSourceLoss`: real wall visibility, cap enforcement and destruction cleanup.
- `DestinationRestoreBarrier`: null-mission deferral, pending resave, pre-readiness restoration, hysteresis and no consequence replay.
- `MissionIsolationAndReadyRestore`: fresh dormant initialization, synchronous ready-pawn load, pending source-mission preservation and removal of old-mission exposure.

UE5.7, UnrealHeaderTool, editor runtime and project Content are unavailable in this workspace. The UObject/replication/collision/GAS tests have **not run**, and the Unreal module has **not been compiled** here. Run a full Development Editor build, this automation group, and the campaign handoff/retry groups on the project workstation. Then play a permissioned field through death/retry, Tarrik/Selene travel, source destruction, blocked remedies and reduced-effects presentation. Authored band mechanics and content remain required before declaring an Eclipse encounter playable end to end.
