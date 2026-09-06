# Checkpoint-safe status restoration

## Scope and evidence

This slice extends the exact tree of merged `main` commit
`a8533c27136e78cbf3b72d00b1ae7f2f4393817e` (PR #33). It addresses the v2 TDD
status save policy and deterministic checkpoint reconstruction requirements
(sections 6.7, 11.8, 15.9 and 15.11). It does not implement a parallel save store.
Narrative still owns actor/component records, save slots and restore orchestration.

Source work and host checks are complete; UE 5.7 compilation, UHT, native
automation, authored assets and campaign playthrough remain acceptance gates.
This is not a measured increase in overall TDD completion or console readiness.

## Findings and disposition

| Finding in merged main | Change in this slice | Dependency / risk |
| --- | --- | --- |
| Status capture/restore APIs existed but nothing called them from Narrative saves. | `USovStatusComponent` implements the existing savable interface and serializes `SavedCheckpointState`. | Existing actor/component serializer; old saves need an explicit missing-record rule. |
| Malformed records cleared live statuses, then skipped bad rows and returned success. | Detached preflight validates the entire semantic snapshot before actor mutation; rejected component loads propagate failure. | Native save hooks default to no-op/accept for other systems; native runtime tests required. |
| Restore reran attack resistance and reset effect level to one; stacks could create two GEs. | One GE per record, using captured effective magnitude, duration, stack count and level directly. | Author the persistent effect contract described below. No fresh attack, immunity or reward pass runs. |
| Capturing before a queued restore committed lost pending status data. | Capture preserves the accepted queued snapshot; failed retries retain it. | Actor/ASC and player readiness must eventually complete; queue acceptance is not runtime completion. |
| GE removal/application and delegates could invalidate live maps or rebind the avatar. | Detach exact owned handles before cleanup; reserve mutation and check ASC/attribute/life/player-initialization identity across restore callbacks. | Late callback rejection cannot undo unrelated callbacks or roll back the world. |
| Pawn component loads precede explicit campaign resource restoration. | Native character loads clear old owned effects and stage incoming state; `RestoreResources` completes status restoration before encounter release. Initial players finish through character readiness. | Standalone actor APIs remain immediate when ready. Generic already-ready Narrative loads have a one-shot next-tick fallback. |
| Default debuffs could remain on loading older saves with no status component record. | Missing old component record means an empty status snapshot, including replacement of a previously queued snapshot. | Missing *semantic data inside a present component record* is malformed, not a legacy empty save. |

## Save and authoring policy

- Manual saves and checkpoint/retry captures use the definition's existing
  `CheckpointBehavior`. No new manual-save policy enum is introduced.
- All five native generic statuses retain `ClearOnCheckpoint`. Saving does not
  cleanse them; loading the snapshot does. Persistent behavior requires an
  explicit authored override.
- `PersistRemainingDuration` stores remaining gameplay seconds. Save age and
  time spent loading do not consume that duration. `PersistFullDuration` stores
  the effective applied duration, including an authored override and resistance.
  Infinite statuses remain explicitly infinite.
- Records store definition primary ID and schema, exact request tag, stacks,
  effective magnitude, effect level, source ability tags and duration policy.
  Source actors, effect handles, timer handles and attack GUIDs are not saved.
  Restored actor attribution is intentionally absent; ability provenance remains.
- Semantic schema stays at version 1. `EffectLevel` and definition schema are
  additive reflected fields defaulting to 1. Earlier builds had no integrated
  disk status record; these defaults do not recover previously lost runtime data.
  A definition rename, incompatible schema or policy change requires an explicit
  migration, not silently retargeting a saved record to another definition.
- Unknown required definitions, duplicates, mismatched identities/policies,
  unsupported schemas, invalid stacks and nonfinite values fail the snapshot.
  Limits are 64 statuses, 16 source ability tags per record and 256 KiB per
  serialized status component. Only descendant `Sov.Ability.*` provenance is valid.
- Persistent GEs must match timed/infinite duration and must not aggregate with
  unrelated effects. Periodic effects must disable execution on application.
  Their period starts afresh on restore; sub-period tick phase is not serialized.
  Nonperiodic execution calculations are not admitted for persistent statuses.
- Continuous nonperiodic modifiers to Health, Shield, Stamina, Poise, Echo or
  their maxima are not supported by this persistence adapter and fail validation.
  The existing resource snapshot stores resolved currents, so installing those
  modifiers afterward would apply their contribution twice. Supporting arbitrary
  resource-modifier persistence requires a coordinated base/current snapshot
  migration in the later resource/save-ownership slice. Periodic damage and
  non-resource modifiers/constraint tags are not excluded by this rule.
- Arbitrary authored GE components, additional effects and removal callbacks can
  still have side effects. These checks are an admission contract, not proof
  that every Blueprint GE is safe. Review authored persistent effects in-engine.
  Effects must not assume the original source actor or its captured attributes
  survive restoration. Blueprint save/load event overrides must call their
  native parent implementations.
- Generic corruption is explicitly excluded. Campaign `USovCorruptionComponent`
  and the opt-in legacy corruption prototype retain their existing owners/policies.

## Restore contract

Narrative preflights component bytes before mutating the actor, freezes the
requested actor record, pins component lifetimes and keeps exact component
identity through callbacks. The native hooks have default implementations so
Blueprint-only and unrelated native savables retain their existing event path.
Duplicate current/saved component names and replaced required components fail.
`ArNoDelta` remains enabled when capturing full Narrative records.

The status adapter decodes into a detached fixed native component, never an
authored subclass. Ordinary tagged subclass properties can be skipped; a custom
native serializer suffix not understood by the adapter is rejected. Input length,
archive limits and post-decode semantic checks are defense in depth, not a claim
that all hostile reflected-array allocation patterns are sandboxed.

Resource completion is distinct from ASC binding. Do not make player readiness
part of `IsInitialized`: player readiness itself depends on project components
being initialized. A valid early snapshot remains queued until the appropriate
readiness/resource boundary. Same-frame deferred death cleanup is drained before
installing saved effects, so its old timer cannot erase the restored set.

During installation, nested restore/apply/remove/cleanse is rejected. Semantic
capture sees the full accepted snapshot, while native capture cannot commit a
half-restored save. Cleanup removes only owned handles and timers; no broad
state-tag removal is used. Restore does not grant recovery immunity or execute
an extra periodic tick. Presentation reports removed/restored state without
replaying attack/reward lifecycle events.

Preflight failure preserves live state and the caller's last good capture.
Failure after a mutable callback is reported, newly installed owned effects are
cleaned up, and queued data is retained for diagnosis/retry. General atomic
world rollback, failed async-travel recovery and save transaction ownership are
not part of this slice. Generic deferred failures are visible through the native
load-acceptance query/logging; the earlier queue-acceptance return is not a
retroactive guarantee that initialization succeeded.

## Files and validation

Runtime owners:

- `Source/ProjectVelkorran/Public/Components/SovStatusComponent.h`
- `Source/ProjectVelkorran/Private/Components/SovStatusComponent.cpp`
- `Source/ProjectVelkorran/Private/Components/SovStatusCheckpointLifecycle.cpp`
- `Source/ProjectVelkorran/Private/Campaign/SovEncounterSnapshotLibrary.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSavableComponent.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/Subsystems/NarrativeSaveSubsystem.cpp`

Host checks performed:

- `python3 -m unittest discover -s Scripts/Tests -p 'Test*.py'`: 30 passed.
- `python3 Scripts/Test-NativePolicies.py`: 39 portable suites passed under GCC,
  C++17, warnings as errors and undefined-behavior sanitizer. These existing
  portable suites do not execute the new UE serializer/GAS code.
- `git diff --check` and independent source reviews of save, lifecycle and status
  ownership boundaries. Native fixtures preserve single-initialization Mac
  worlds and explicit script execution guards for dynamic callbacks.
- Source inventory: 294 unique native registrations (274 on the merged base),
  including 20 new checkpoint cases: 14 serialization/runtime and 6 lifecycle
  boundary tests. No duplicate test names were found.

New native tests under `Private/Tests/SovStatusCheckpoint*` exercise real
Narrative actor records and actual `SaveGameToMemory`/`LoadGameFromMemory` round
trips; policy variants, effect levels/stacks/provenance, elapsed duration,
repeated resistance/load/expiry, early queued recapture, malformed semantic and
archive data, missing old records, unsupported GE authoring, periodic timing,
avatar replacement, nested mutation/capture and unrelated corruption-tag effects.
Boundary tests cover explicit resource completion and readiness staging.
These tests are authored, not executed in this workspace.

Run the UE 5.7 build/UHT gate before interpreting automation:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -BuildOnly
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -SkipBuild -TestFilter ProjectVelkorran.Campaign.Status.Checkpoint
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -SkipBuild -TestFilter ProjectVelkorran.Campaign
```

Also rerun Narrative save/migration, protagonist switching, encounter retry,
status/corruption, hero Echo and the previously passing Mac cases. Exercise both
heroes and persistent-ASC handoff, failed readiness, dead-to-revived retry,
same-world reload, streamed NPC restoration, HUD removal and authored GEs.
Include non-unity builds, malformed-length native fuzz tests and Development
game targets. Keep the PR draft until engine evidence is attached.

## Next slices

1. Save/travel transaction ownership and failure recovery, including resource
   base/current semantics, deferred finisher outcomes and failed handoff rollback.
2. Remaining combat continuations: drone DeviceDisabled behavior, passive
   Shield/Poise owner checks and Cinder Judgement eye-to-muzzle obstruction.
3. Editor-only test-module isolation and packaged-game validation, preserving
   the existing Mac fixture fixes and accounting for all missing/not-run tests.
