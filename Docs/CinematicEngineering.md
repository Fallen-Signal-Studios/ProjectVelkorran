# Native cinematic engineering

Follow-up: [PartitionedCinematicEngineering.md](PartitionedCinematicEngineering.md) adds native World Partition readiness and typed door/lift power/lock postconditions. Its scope and validation notes supersede the earlier limitations below.

This closes source-level lifecycle gaps identified against TDD §§9.13 and 14.13. Authored sequences, role bindings, animation tracks, preload manifests, scene blocking, and the actual Level 1/2 playthrough remain editor validation work. Unreal 5.7 is unavailable in this workspace: no engine build or runtime automation result is claimed.

## Existing owner repaired

`ANarrativeLevelSequenceActor` remains the playback owner. It now owns exact per-ASC loose-tag counts and the actual notified controllers, deduplicates repeated bindings, and retains leases through pause/resume. Acquisition and teardown are fenced across callback reentry. Stopping, failure, replacement and EndPlay remove only that session's leases and visual-ready subscriptions. Required tagged bindings wait for real initialized participants for a bounded timeout; missing/destroyed participants terminate instead of hanging.

`BlendOutAndStop` captures poses, waits its bounded blend interval, then actually stops and emits `OnBlendOutFinished`; the actor remains reusable. The existing async play/blend nodes now clean subscriptions and expose interruption outcomes. Playback generations prevent an old request from consuming a replacement sequence's completion. Direct engine `Play` during teardown is stopped rather than leaving unowned playback running.

## Managed campaign scenes

Attach `USovCampaignCinematicComponent` to the existing Narrative sequence actor and configure `MissionId`, `BeatId`, `Sequence`, participant contracts, preload assets and streaming levels. Every native M01/M02 cinematic created by the native definition helper opts into `bRequiresCinematicProof`; generic legacy beats retain the legacy path. Managed scenes cannot use the generic dialogue beat event or `RecordCinematicViewed` to manufacture completion.

The component performs the following bounded transaction:

1. Check current ready, living protagonist, physical entry range, active mission and beat prerequisites. Reserve request ownership before callbacks. Require the existing save coordinator to complete a pre-scene canon checkpoint.
2. Preload declared assets, existing streaming levels and optional World Partition regions with a finite real-time timeout. Partition regions require active cell streaming sources and visible initialized actor witnesses before participant resolution.
3. Resolve exactly one controlled protagonist and uniquely tagged real characters; require matching ASC/avatar readiness, configured damage-state tags, and any required already-owned equipped weapon with a legal hand attachment. Validate sequence hierarchy, duration and role bindings. Reject generic Sequencer event tracks, including nested subsequences, because arbitrary event side effects cannot be replayed atomically on skip.
4. Play through the existing actor/player at normal rate with forced restoration of evaluated tracks. The component balances its own input/tag counts independently from the actor. Pause/resume, destruction, replacement and missing participants preserve exact ownership.
5. Natural completion must prove monotonically observed sequence progress, expected generation, normal rate and terminal position. A frozen player gains no viewing credit; seeking, reverse playback and premature completion fail. Skip requires an earlier fully viewed, non-interactive cinematic ID.
6. Both completion and skip stop/restore the same sequence, apply the same typed final transforms and draw/holster state, then commit the same campaign beat. Final placements require clear capsules, navigation, existing recovery-exclusion checks and pairwise participant clearance. Required weapons must still be owned/equipped. Every final postcondition is checked again after all participant callbacks.
7. The existing campaign journal records the unique consumed session receipt. Managed viewed IDs rebuild from non-skipped receipts in journal order during save validation; a saved viewed bit cannot authorize a first skipped entry. Beat state writes, knowledge, consequences, critical evidence and existing completion tasks retain their existing owner. Existing canon checkpoint scheduling follows the committed beat.

Only the matching playback generation may restore participant transform/wield snapshots or camera state. A stale session releases its own input/tag counts without stopping or repositioning a newer playback. Failure before the native beat commit leaves the beat incomplete and performs bounded snapshot restoration where the original actor and ASC still exist. Callback-triggered destruction cannot be rolled back.

## Exact supported postconditions and remaining scope

Typed postconditions cover character transforms, keeping/holstering/drawing an **already owned and equipped** weapon through `ANarrativeCharacter::SetWieldState`, and stable door/lift power and lock state through the existing `ASovWorldTransitActor` setters. They do not grant/remove inventory, replace equipment, change health, move a mechanism, or replay arbitrary Blueprint events. Required inventory/reward operations must use an existing authoritative native owner and a separately validated committed beat; generic Sequencer events are rejected for managed scenes. Native postcondition verification is a finite contract, not an atomic rollback system for arbitrary gameplay callbacks.

The preload manifest supports soft assets, existing classic streaming levels and explicit World Partition regions. Actual participants resolve after readiness so cell streaming can make existing placed actors available; the sequence owner and current player must exist at request time. The implementation does not spawn absent participants or activate disabled Data Layers. Custom track side effects, per-bone pose continuity, authored camera cuts, audio/subtitle timing and replay UI require the actual assets and engine validation. Managed scenes intentionally exclude interactive choices and protagonist handoffs; those have their own native gates.

`ValidateConfiguration` and sequence/participant preflight run from the component. The campaign asset commandlet validates native beat schemas, but does not instantiate map actors or certify a cinematic component's authored bindings/streaming manifest. Run each scene's preflight and runtime automation in UE before shipping.

## Validation

`Tests/Portable/SovCinematicPolicyTests.cpp` exercises the production progress/terminal-position and capsule policies at 30/60 Hz and long frames, rejects seeking/rate/nonfinite inputs, and enumerates participant placement boundaries. Run `python Scripts/Test-NativePolicies.py` for all portable suites with warnings as errors and undefined-behavior sanitization.

Authored Unreal tests in `SovCinematicLifecycleRuntimeTests.cpp` cover exact ownership, pause/resume, callback reentry, missing-participant timeout, async generation isolation, finite blend completion, bounded manifest/event-track validation, native proof bypass rejection, stale completion preserving a newer player's playback/transform/camera, abort/EndPlay and pause-callback abort. They have not been executed here. Required engine acceptance additionally includes complete and skipped versions of every scene, pre/post save restore, missing asset/participant tests, blocked final placements, real weapon attachments, and 30/60 Hz camera/input recovery.
