# Companion co-actions

This slice supplies a native mission-scoped companion action on Narrative's existing NPC goal, activity and path-following architecture. It does not replace a companion's combat/follow activities, add approval statistics, or make companions invulnerable. Blueprint owns interaction presentation and the authored extraction Sequence; C++ owns permission, movement completion, interruption and the durable mission fact.

## M02 extraction authoring

1. Use the native `USovOneDegreeMissionDefinition` schema as the basis of the mission asset. Its sequence is `Escape` → `LyricAtExtraction` → `LyricExtraction` → `VossDirection`. Existing manually copied assets must add this prerequisite themselves.
2. Add `USovCompanionComponent` to Lyric's Narrative NPC character. Set `CompanionId` to `Lyric`. The NPC must be alive, possessed by `ANarrativeNPCController`, and have an initialized Narrative ASC and active activity component.
3. Place `ASovCoActionAnchor` with `AnchorId = M02_LyricExtraction`, `MissionId = M02_OneDegree`, `CompletionBeat = LyricAtExtraction`, and `RequiredCompanionId = Lyric`. Place its mark at Lyric's intended navigation-agent location, normally the feet on navigable ground. IDs must identify one live companion and one anchor.
4. Route the authored contextual interaction to `CanRequestCoAction` / `RequestCoAction` on authority. Show failures and retry affordances from `OnCommandStateChanged`. Do not complete `LyricAtExtraction` from the interaction or Sequence: ordinary `CompleteBeat` rejects a missing native receipt.
5. When the native beat commits, begin the authored `M02_Extraction` Sequence. Its full successful completion may record playback and complete `LyricExtraction` using the existing campaign APIs. Native arrival and cinematic presentation are distinct beats so skipping presentation cannot create an arrival.
6. Assign `RequiredEncounter` when Lyric's defeat should fail that active encounter. Register required actors with the existing encounter/checkpoint authoring. Defeat fails the encounter; entry retry restores authored participants. This component does not resurrect a companion or supply a separate save system.

All co-action beat definitions require `bRequiresCoActionProof`, `RequiredCompanionId` and `RequiredCoActionAnchorId`. Co-action beats cannot also be cinematic or choice beats. Initial request, continued movement, and final receipt each check the mission, prerequisites, state requirements, protagonist knowledge, live possessed player, companion identity and range. Movement lock, death, poise break, scripted interactions and unrelated Busy contributions interrupt or reject the request. The player must remain inside the anchor's request range.

## Lifecycle and recovery

The component adds one transient `USovCoActionGoal` and one reusable, unsaved `USovCoActionActivity`. It uses the existing controller's activity slot and one path request. It releases only its own Busy contribution, goal and path request; another activity's movement is not stopped by a stale path callback. Interruption resolution waits until the component tick to avoid recursively entering Narrative's activity selection from `EndActivity`.

Arrival requires a successful path result, actual position within the mark radius, and the configured hold time. A matching live request creates a synchronous one-use receipt. The campaign transaction commits the companion ID, anchor ID and request GUID in its existing journal before firing mission notifications. A duplicate or canceled request cannot replay completion. Load validation rejects missing, mismatched or reused saved proof.

A failed path or timeout attempts at most one configured hidden fallback. The fallback must project onto navigation, fit the companion's capsule, and keep source and destination capsule samples occluded from every available player camera. Missing camera information, visible samples, no navigation, collision or another failed route cause a visible failure/retry result. `HoldAtMarkSeconds` must be less than `TimeoutSeconds`; a fallback permits one additional timeout window. No fallback is required for normal operation.

In-progress commands, goals and path request IDs are deliberately transient. Loading cancels movement and returns the component to Idle while previously committed campaign facts remain saved through Narrative. The mission may offer the contextual request again if no arrival fact was committed. Companion movement/follow/attack behavior outside this bounded action remains authored through Narrative activities.

## Validation

Four Unreal automation tests are provided under `ProjectVelkorran.Campaign.Companion`:

- `NativeArrivalProof`: prerequisite gating, blocked generic and forged completion, real activity-slot ownership, one-use arrival, durable proof and malformed saved proof rejection.
- `CancelLoadAndStaleCallbacks`: owned Busy cleanup, preserved external tag contribution, ignored late path result, command retry and transient load cleanup.
- `TimeoutInterruptionAndDefeat`: deferred interruption, bounded failure without a valid fallback, no invented mission completion, and encounter failure on required companion defeat.
- `HiddenFallbackVisibility`: real world traces reject visible teleport endpoints and require occlusion.

These fixtures use a transient world, real Narrative goal/activity components and a real NPC ASC. The arrival test delivers a controlled path-completion callback at the native boundary; it does not claim to test Recast pathfinding. The test fixtures exclude authored character/mission assets.

Unreal Engine is unavailable in the current workspace, so these automation tests and UHT/UBT compilation have not been run here. Run the `ProjectVelkorran.Campaign.Companion` group through `Scripts/Validate-Unreal.ps1` with UE 5.7, then play the authored M02 route. Required content checks: reachable mark; physical obstruction; moving away; poise interruption; destruction/streaming removal; source/destination visibility during fallback; save before/during/after action; required companion defeat and entry retry; extraction Sequence completion and skip after prior viewing. Confirm one arrival fact, one extraction, and no retained Busy or owned movement after every exit.
