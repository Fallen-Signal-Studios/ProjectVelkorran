# Corruption engineering contract

This implementation follows the August 2026 TDD v2 §§7.7–7.8. It provides authoritative, mission-scoped exposure, source handles, band state, GAS ownership, checkpoint persistence and accessible presentation requests. Narrative owns disk serialization and campaign facts. The native producers now cover environmental fields, accepted enemy hits, existing command links, contaminated friendly signals and compromised machinery. Native remedies cover leave-field recovery, link severing, node destruction, protecting a distinct signal, countermeasure interaction and completed campaign objectives.

## Native ownership

| File / class | Responsibility |
| --- | --- |
| `Public/Components/SovCorruptionComponent.h`, `Private/Components/SovCorruptionComponent.cpp` | Exposure records, permission checks, contact handles, band transitions, owned GAS effect, presentation, save/load barrier. Constructed on `ASovPlayerCharacterBase`. |
| `Public/Corruption/SovCorruptionProfile.h`, `Private/Corruption/SovCorruptionProfile.cpp` | Explicit source identity, target identities, rate/contact amount, spatial policy, mission caps, remedy, presentation, accessibility, checkpoint and canon consequence contract. |
| `Public/Corruption/SovCorruptionSourceVolume.h`, `Private/Corruption/SovCorruptionSourceVolume.cpp` | Environmental overlap producer, optional real destructible source node, falloff/visibility and source cleanup. |
| `Public/Corruption/SovCorruptionSourceComponent.h`, `Private/Corruption/SovCorruptionSourceComponent.cpp` | Binds actual Narrative ASC damage/death events, existing command-link membership/sever state, and contaminated friendly proximity; retains resolved mission identities through Narrative component saves. |
| `Public/Corruption/SovCorruptionInteractableComponent.h`, `Private/Corruption/SovCorruptionInteractableComponent.cpp` | Extends Narrative's completed interaction with machinery exposure, a countermeasure, and a continuous protect-signal interval that resets on actual damage. |
| `Public/Effects/SovGameplayEffect_CorruptionBand.h`, `Private/Effects/SovGameplayEffect_CorruptionBand.cpp` | One owned GAS effect grants the current band tag and applies bounded vulnerability/recovery pressure through existing attributes. The component removes only the handle it applied. |
| `Private/Corruption/SovCorruptionMath.h` | Production-used finite tuning validation, hysteresis, cap arithmetic and radial falloff. |
| `Private/Tests/SovCorruptionRuntimeTests.cpp`, `SovCorruptionRuntimeTestFixtures.h/.cpp` | Real world, collision, player component, Narrative ASC and campaign state integration fixtures. |
| `Tests/Portable/SovCorruptionMathTests.cpp` | Engine-independent boundary checks of the production math. |

All `Public` and `Private` paths above are relative to `Source/ProjectVelkorran`.

## Contact and exposure

An active mission is mandatory. A profile is dormant unless its source ID, allowed Tarrik/Selene identities, exact mission permission, non-clear cap, finite tuning, remedy text, presentation ID, ordinary information and reduced-effects substitute validate. No opening mission is implicitly permissioned for Eclipse exposure.

`AcquireSource` accepts an actual `ASovCorruptionSourceVolume` in the same world. The volume must overlap the player, satisfy radius/falloff and pass its authored visibility policy. The player's ASC must still own that avatar, be alive, and carry the active mission's protagonist identity. A raw source ID or a client request cannot manufacture contact.

One handle represents one source/profile/mission contact. Repeated acquisition returns the same handle. Instant contact exposure applies once while a profile has continuing contact; overlapping fields with the same profile can each contribute their sustained rate. Every contribution respects the smaller profile/mission cap and the global 0–100 limit. Sources release handles on leave, reassignment and destruction; the component independently removes stale or invalid handles.

Default bands are Trace at 1, Intrusion at 25, Contest at 50 and Overwrite risk at 80, with 5 exposure points of downward hysteresis. Thresholds and hysteresis must be finite and strictly ordered. A cap ends immediately below the next band's representable threshold. These are engineering defaults requiring mission tuning.

Leave-field profiles dissipate at their configured recovery rate once all contacts for that profile have ended. Other remedy types use native verified producer or interaction paths. `CleanseExposure` is retained only as a native reset/testing primitive; it is no longer exposed as an unchecked Blueprint gameplay remedy. An authored `EscapeBeatId` additionally disables that source and removes its gameplay exposure when the existing campaign beat is complete. Source loss alone does not invent completion of a destroy-node, break-link or objective remedy.

## GAS, story facts and feedback

The component grants exactly one of these tags through its own `USovGameplayEffect_CorruptionBand` instance:

- `Sov.State.Corruption.Trace`
- `Sov.State.Corruption.Intrusion`
- `Sov.State.Corruption.Contest`
- `Sov.State.Corruption.OverwriteRisk`

Clear removes that owned instance and its modifiers. Generic damage immunity and Corruption-channel immunity prevent new exposure and suspend owned combat pressure. Trace remains informational. The native band effect adds an Intrusion resistance penalty of 5 percentage points and, at Contest or Overwrite, multiplies the existing Stamina regeneration rate by 0.5. Accepted harmful status duration also increases by 10% at Intrusion and 25% at Contest/Overwrite through `ResolveIncomingStatusDuration`; the native freeze/chill producer uses this adapter. Authored Blueprint status producers should call the same adapter after their accepted damage/status receipt. Immunity and beneficial durations must not use it. The adapter requires the component's current owned band effect, so a stale replicated state or loose tag cannot manufacture vulnerability. Resistance and regeneration are bounded profile defaults, selected only from sources permissioned for the relevant band. The Exertion system consumes that same Stamina attribute; corruption owns no second regeneration timer. These are prototype combat settings for tuning. Corruption is not implemented as a second health or poison attribute.

A canon-persistent profile names an existing non-interactive, non-cinematic consequence beat. Only that profile's own contribution and permitted cap can qualify for its consequence threshold. Aggregate exposure from a different source cannot supply that proof. The request then passes through `USovCampaignStateComponent::CompleteBeat`, preserving prerequisites, protected facts and idempotence. Gameplay cleansing leaves those durable campaign records intact. Restoring exposure does not replay the consequence request.

`OnPresentationRequested` supplies the band, exposure, profile IDs, remedy text, information text and suggested intensity. `SetReducedEffects(true)` and the persisted Narrative game-user-settings preference preserve the numeric state and remedy, substitute the authored accessible information, and set suggested distortion intensity to zero. Clock activity and exact remaining seconds are provided identically in both presentations. Presentation callbacks do not drive gameplay state. These APIs introduce no input delay, reversed movement, automatic dialogue selection or fabricated system UI. The player accessibility settings/HUD/audio/VFX integration must bind this contract and verify equivalent information.

## Checkpoint and destination restore

Only profiles explicitly setting `bPersistExposureAtCheckpoint` enter the SaveGame record. Records contain profile identity, source mission, exposure, continuing contact suppression, elapsed Overwrite time and its one-shot expiry latch. Restored timer data must match that exact profile permission and duration. Pending interaction/protection progress is canceled on load, while completed source remedies and consumed machinery interactions persist in their existing Narrative component records. The saved band preserves hysteresis. Runtime actor pointers, source handles and active-effect handles are rebuilt. Invalid schema, duplicate profile IDs, invalid profile contracts, non-finite values and exposure totals above 100 fail validation.

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
2. Choose the explicit profile `SourceKind` and corresponding native producer below. Use a distinct `SourceId` for a distinct contract; a different asset cannot reuse an active ID.
3. Configure the real remedy producer or a valid durable escape beat. A destroy-node field requires its actual `SourceNode`; that node must carry a matching native source component and a Narrative ASC that receives real damage/death. A removed node cannot turn the field into an unowned active source.
4. Tune the native band defaults. For Overwrite encounter failure, explicitly opt in on that profile's exact mission permission and set the real encounter ID and duration. Boss-specific phase choreography remains authored content through existing campaign state.
5. Bind the presentation request and persisted accessibility setting. Test remedy comprehension with minimum visual effects.
6. Decide checkpoint persistence explicitly. Canon consequences and gameplay exposure are separate decisions.

## Producer and remedy setup

| Source kind | Native producer and proof | Native remedy |
| --- | --- | --- |
| Environment | `ASovCorruptionSourceVolume`; actual overlap, distance and configured visibility. | Leave-field dissipation; exact completed campaign escape beat; configured countermeasure; or matching destructible node death. |
| EnemyAttack | `USovCorruptionSourceComponent` on the attacking character. Requires its current ASC's actual nonperiodic accepted hostile Corruption transaction, valid effect context, no successful Guard/Deflection, an accepted Corruption channel, spatial contract, and a new transaction ID. | The profile's explicit remedy. Merely declaring the damage channel is insufficient to expose without the component and mission permission. |
| CommandLink | The same source component finds the owner's existing `USovCommandLinkComponent`. The exposed player must actually belong to its active link and satisfy range/visibility. | The observed link instance must reach real `Severed` state. This disables that source for the mission and removes its profile exposure; an inactive or missing link is not a fabricated sever. |
| ContaminatedAlly | The same component requires a living friendly actor carrying the profile's explicit contamination tag. Hostile, neutral, untagged and dead actors cannot pretend to be this producer. | A `ProtectSignal` corruption interactable points at that actor. Completed Narrative interaction begins a configurable 1–60 second interval. The player must remain nearby with visibility, the same mission and possession, and a living friendly signal. Real incoming damage to the signal resets progress. Completion disables that source and cleanses its contribution. |
| Machinery | `USovCorruptionInteractableComponent`, `CompromisedMachinery` action, machinery profile. Native Narrative completion checks actual interactor/controller ownership, authority, range, visibility and mission. | A `Countermeasure` interactable uses a profile whose escape is `AuthoredCountermeasure`. It may reference the exact source actor to disable as well as cleanse. Machinery and countermeasure interactions commit once per mission and persist in Narrative saves. |

An actor carrying a contamination state can be a companion or ordinary NPC. This supplies exposure *from* an actually contaminated ally to the permissioned protagonist; it does not give the game a second generic NPC poison meter. Existing authored story facts remain the authority for why that actor is contaminated.

## Overwrite clock

Each `FSovCorruptionMissionPermission` can explicitly allow encounter failure, name `OverwriteEncounterId`, and set `OverwriteSeconds` from 1 to 600. A source cannot borrow another profile's exposure to qualify: its own contribution must reach Overwrite. The clock advances only while the matching, unambiguous active `ASovEncounterDirector` owns the exposed protagonist. It never fails an unrelated encounter, starts a generic global game-over, changes input or chooses dialogue.

The component publishes remaining time and the remedy in the regular and reduced-effects presentation requests, including a zero-time notification before routing expiry through `FailEncounter`. Corruption immunity pauses the clock. Falling below the qualifying exposure resets elapsed time. An expiry latch prevents repeat failure calls; elapsed time/latch are saved only for checkpoint-persistent profiles. Profiles without explicit clock permission retain the band for authored boss-phase use and cannot silently fail a mission.

Scripted pressure is delivered by mission-permitted fields, links, machinery, or native damage, admitted by the existing campaign's active protagonist identity, including approved in-mission handoff. A raw script cannot manufacture an exposure amount or erase a canon consequence through this Blueprint surface.

## Validation and current evidence

The portable production math test was compiled with `g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=undefined -fno-sanitize-recover=all` and executed successfully in this workspace. It covers finite threshold validation, hysteresis, source caps, invalid amounts and radial falloff. The full portable runner is `python3 Scripts/Test-NativePolicies.py`.

Thirteen Unreal integration tests are authored under `ProjectVelkorran.Campaign.Corruption`:

- `PermissionAndContact`: mission gating, actual overlap, instant contact deduplication, forged out-of-field rejection and escape.
- `SnapshotAndAccessibility`: committed snapshot visibility during callbacks, band/remedy parity, owned effect removal, contact restore deduplication and invalid schema rejection.
- `ConsequenceMissionCap`: one source cannot borrow another source's band to commit a fact; cleanse preserves the committed fact.
- `ProfileValidation`: explicit source and accessible substitute requirements.
- `OcclusionCapAndSourceLoss`: real wall visibility, cap enforcement and destruction cleanup.
- `DestinationRestoreBarrier`: null-mission deferral, pending resave, pre-readiness restoration, hysteresis and no consequence replay.
- `MissionIsolationAndReadyRestore`: fresh dormant initialization, synchronous ready-pawn load, pending source-mission preservation and removal of old-mission exposure.

The additional native integration tests are `NativeAttackProducer`, `NativeLinkAndRemedy`, `NativeBandPressure`, `NativeMachineryAndProtection`, `NativeOverwriteClock`, and `NativeDestroyNode`. They exercise real GAS transactions, link severing, the Narrative interaction path, attribute ownership, protection interruption and existing encounter failure. The expanded portable math suite additionally covers accepted-hit boundaries, protection reset/clamping and Overwrite clock arithmetic.

UE5.7, UnrealHeaderTool, editor runtime and project Content are unavailable in this workspace. The UObject/replication/collision/GAS tests have **not run**, and the Unreal module has **not been compiled** here. Run a full Development Editor build, this automation group, and the campaign handoff/retry groups on the project workstation. Then play a permissioned field through death/retry, Tarrik/Selene travel, source destruction, blocked remedies and reduced-effects presentation. Art, audio, UI bindings, mission profiles, source placement and boss-specific phases remain editor integration work. The native producers, remedies, bounded default pressure and permissioned failure clock are implemented; an Unreal build and real encounter playthrough remain mandatory before declaring this playable end to end.
