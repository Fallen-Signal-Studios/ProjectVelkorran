# Campaign save slots and recovery

The native campaign path implements the TDD v2 §§11.8–11.9, 15.9, 15.16, 18.5 and 19.7 storage policy through `USovSaveSubsystem`. It uses Narrative's existing `UNarrativeSave` records and preserves the configured save subclass, including subclass fields. `USovCampaignSaveGame` is a versioned disk envelope around those serialized records, not a second actor/checkpoint model.

## Implemented behavior

| Requirement | Native behavior |
|---|---|
| Three rolling autosaves | Three logical slots, oldest valid generation replaced first; generation order survives process restart. Queued native boundaries defer while the world is unsafe and coalesce to the latest coherent boundary. |
| Ten manual slots | `SaveManual(0..9)` rejects other indices and unsafe world state. `ListSlots()` exposes actual valid mission/location/time/difficulty metadata. |
| One checkpoint quick-retry slot | `WriteCheckpoint` writes logical checkpoint 0 and queues the corresponding rolling autosave. `LoadSlot(Checkpoint, 0)` follows the normal validated level-load path. Encounter-local quick retry continues to use the existing director entry snapshot. |
| Safe-state admission | Standalone authority, ready matching protagonist/ASC, living grounded pawn, valid campaign and Technique state, no transition/mutation, no active/restoring encounter, no Busy/weapon change/guard/deflection/break/traversal, no cinematic or dialogue/interaction, and no living hostile within 1,500 cm or targeting the player. This is actual runtime state, not a caller-provided `bSafe` boolean. |
| Captured arena entrances | ArenaEntry/BossRetry requests may recognize a director-proven quiescent entry. Only that director's verified frozen registered participants are exempted from nearby-threat rejection. Player Busy and other blockers still reject. Manual saves keep ordinary admission. Other threats and blockers still reject. |
| Stable capture | Narrative creates a temporary candidate by duplicating its configured save subclass and transactionally capturing actors/components. Failed captures retain the previous live object. Duplicate actor GUIDs reject capture. Required components include class metadata. |
| Transform presence | Movable actor records explicitly mark a captured transform, including identity at world origin with unit scale. Controller/PlayerState and mission-travel suppression clear that flag. Legacy records without the field retain the original identity-as-omission behavior and continue to restore nonidentity transforms. |
| Lookup-only actors | `NarrativeStableActor` provides GUID lookup without opting into world serialization. Only `NarrativeSavableActor` creates an enumerated world record. Lookup-only actors still participate in duplicate-GUID rejection, and explicit owner-driven record capture retains their stable GUID. |
| Write integrity | Header, required-asset paths, Narrative payload and portable settings participate in a CRC integrity check. Platform write and exact byte readback must both succeed before a save-success event. CRC detects accidental corruption; it is not a signature or an anti-tamper security feature. |
| Last-good retention | Each logical slot uses two physical banks. The inactive bank receives the new envelope; the previous valid bank is never overwritten in that transaction. A torn or denied write cannot remove both valid generations. |
| Corrupt newest save | Reads select the highest valid compatible bank. A load request that encounters a damaged bank reports `RecoveryAvailable` with the candidate metadata and waits for a second explicit `bAcceptRecoveredBank` request. `FindRecoveryAutosave` preflights compatible autosaves for a recovery choice. Corrupted bytes are copied and readback verified to a separate recovery file before their bank may be reused. Files are never deleted by this implementation. |
| Failure decision | Failed disk writes retain the captured envelope and pause standalone gameplay if this subsystem can acquire the pause. `RetryFailedWrite` retries that snapshot. `AcknowledgeSaveFailure` is explicit continuation without saving and releases only the subsystem's own pause. Neither path claims a save before verified success. Acknowledging a failed critical checkpoint creates one native continuation receipt for that exact boundary, mission, protagonist and world, expiring after 60 seconds. The irreversible owner consumes it once; it cannot bypass unrelated saves. |
| Account namespace | Names contain a hash of the selected stable platform account ID and use Unreal's local user index. A deterministic offline local profile supports offline play. Signed-in frontends call `SelectPlatformUser` before selecting a campaign. An active user's campaign cannot be relabeled as another account. |
| Version compatibility | Initial external campaign schema is 1.0. Same-schema patch/build changes load. December prototype/older-major saves, unknown newer schema and wrong product/account fail closed. No unshipped historical format is silently fabricated into v2. |
| Required versus optional records | Existing Narrative actors and savable components default to required. `IsOptionalSaveRecord` explicitly opts cosmetic/DLC records out. Missing optional class packages are logged and skipped; absent or mismatched required components stop restore. Canon state always remains required and is replay-validated before world mutation. |
| Settings | Save metadata records difficulty and a validated portable gameplay-settings subset. Loading preserves current account accessibility preferences. Explicit difficulty restoration uses the settings subsystem's existing validated API. |
| World/destruction persistence | Placed destroyed savable actors retain GUID tombstones; destroyed dynamic records are removed. Unloading levels capture their existing savable actor records. Failed unload capture blocks later full-save success until a valid recapture/reload, rather than silently saving stale state. |
| Restore dependencies | Existing actor/component interfaces expose native `GetSaveRestorePhase`; no parallel world store is introduced. Structural actors restore before encounter/companion phases. Each actor's components restore in phase order, so canon can precede presentation. Unknown phase values, duplicate required component names and required class mismatches reject before the affected actor is mutated. |

## Native flow and framework integration

`SaveManual` and `WriteCheckpoint` return a typed result plus an error. UI binds `OnSaveCompleted` for the success indicator or actionable disk-error prompt. `RetryFailedWrite` and `AcknowledgeSaveFailure` are the two native responses to a disk failure; storage denial remains visible and does not become a success through a cosmetic callback.

`LoadSlot` validates integrity, product/schema/account, required asset packages, map/mission/pawn definition, the serialized canon journal and player/world record identities before travel. It decodes the configured Narrative subclass. A successful request returns `LoadStarted`, not completed gameplay. The GameInstance subsystem retains the decoded object across travel and provides it to Narrative through `OnInitialSaveRequested` before ordinary actor loading. GameMode rejects a failed initial load and mismatched mission. The managed controller restores its existing per-protagonist snapshot and calls `NotifyCampaignReady` only at its readiness barrier. `OnLoadCompleted(Success)` occurs there. Failed or timed-out initialization reports recovery and preserves all source files.

`QueueAutosave` accepts the existing typed boundary enum: mission start, arena entry/exit, before choice, canon gate, intended boss retry, long transition, hub exit and explicit checkpoint. Native campaign/encounter owners call these boundaries; authored level events use the same API for traversal/cinematic/hub transitions. Do not write an unresolved choice to the campaign graph before the BeforeChoice checkpoint succeeds. `WriteCheckpoint` is synchronous specifically so irreversible callers can stop on failure.

A two-bank commit is used because Unreal's generic platform save API does not expose an atomic rename/replace guarantee across supported platforms. The implementation does not claim stronger storage guarantees than that API offers. Platform-certified atomic storage can implement `ISovSaveStorage`; the capture, validation and last-good selection remain unchanged.

## Files

- `Source/ProjectVelkorran/Public/Save/SovCampaignSaveGame.h`, `Private/Save/SovCampaignSaveGame.cpp`: versioned envelope, metadata and integrity.
- `Public/Save/SovSaveSubsystem.h`, `Private/Save/SovSaveSubsystem.cpp`: native admission, platform namespace, slots, verified writes, failure decisions, asynchronous managed load and recovery.
- `Private/Save/SovSavePolicy.h`: production-used portable slot/schema/bank/admission policy.
- NarrativeSaveSystem `NarrativeSave`, savable actor/component interfaces and `NarrativeSaveSubsystem`: existing record metadata, candidate capture, validated initial-snapshot loading, required/optional handling and last-good world records.
- `Tests/Portable/SovSavePolicyTests.cpp`, `Private/Tests/SovSaveRuntimeTestFixtures.h`, `Private/Tests/SovSaveRuntimeTests.cpp`: policy and real Unreal serializer/storage regression tests.

## Validation performed and required

Executed here: portable C++ policy suite with warnings as errors and undefined-behavior sanitization. It covers 32,768 admission combinations, 100 rolling autosave replacements, bounds, bank selection, bad generations/overflow, version and account compatibility. The unified portable runner passed with this suite included. No Unreal/UHT/UBT execution is claimed.

Added Unreal automation under `ProjectVelkorran.Campaign.Save`:

1. Actual save-envelope serialization preserves the configured Narrative subclass and extra data. Denied storage and interrupted writes return failure, retain a good bank, and recover using a subsequent verified write while preserving corrupted bytes.
2. Initial schema 1.0 and same-schema future build compatibility; legacy/newer schema and wrong account rejection; payload checksum mutation detection.
3. Actual Narrative world capture creates an independent configured-subclass candidate; an actor archive error rejects the whole candidate and leaves live records intact.
4. Actual component restore places canon before presentation regardless of component creation order; duplicate component names and unknown restore phases fail before actor deserialization.
5. Actual Narrative disk serialization preserves explicit identity transforms; reload restores a moved actor's location, rotation, scale and state. Legacy identity omission and legacy nonidentity placement remain compatible.
6. A lookup-only stable actor does not poison the world snapshot with a zero record GUID; the real world load succeeds, lookup remains valid, and collisions with savable GUIDs still reject capture.

The last two regressions are in `Private/Tests/SovNarrativeSerializerRuntimeTests.cpp`. They are added source coverage, pending execution in UE5.7. The external campaign envelope remains schema 1.0; the optional tagged Narrative field does not require an envelope migration.

`Private/Tests/SovSaveWorldLoadRuntimeTests.cpp` adds an integration regression using two real transient campaign worlds. The requested world accepts a valid actor snapshot, the actor's load archive then fails inside Narrative's actual initial-restore path, and the core ticker must publish one recovery result without waiting for the timeout. Its expired travel URL stays rejected while a deliberate fresh campaign world without a slot request remains eligible. This test also awaits UE5.7 execution.

The required engine/platform validation remains: execute these tests in UE5.7, kill the process at every platform-write/readback boundary, fill/deny storage, verify console/account switching and optional cloud conflict UX against each target platform, load at every legal authored beat, 100 death/retry cycles, before/after respec, protagonist/convergence transitions, companion disabled/rescue state, destruction and streamed level states, and spawn-collision checks. Assets/maps, widget presentation, platform account callbacks and certification/service credentials are outside this source-only pass. Cloud synchronization itself is not implemented as a speculative network service.

No historical shipped schema migration is needed yet: the TDD establishes 1.0 at the first external v2 build. Any future schema increment must add deterministic migrations and immutable historical fixture files before shipping. Renaming or removing required Blueprint/item assets still needs an explicit content migration, not silent null references.
