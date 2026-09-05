# Partitioned cinematic readiness and native world postconditions

Engineering follow-up against TDD v2 §§11.10, 14.13 and 15.11. The existing `USovCampaignCinematicComponent` and Narrative sequence actor remain the owners. Source implementation is complete for the contracts below. Unreal 5.7 compilation, runtime automation and authored partition-map acceptance have **not** run in this environment.

## Gap and disposition

The earlier cinematic component requested soft assets and conventional streaming levels. It never requested World Partition cells, and it resolved participant actors before streaming. A camera destination in an unloaded partition region could therefore fail early or play without the required world. Keep the existing asset, classic-level, receipt, participant and sequence lifecycle architecture and extend its preparation gate.

Required final world changes previously covered participant transforms and already-equipped weapon draw/holster state only. Add a deliberately finite native door/lift power-and-lock contract through `ASovWorldTransitActor`, whose save fields, navigation links, authority checks and notifications already exist. The subsequent [CinematicInventoryTransactions.md](CinematicInventoryTransactions.md) extends **Narrative's existing inventory/equipment authority**, with default-deny typed grants, removals and equipment replacement; no parallel cinematic inventory exists. Arbitrary Sequencer event replay remains unsupported.

## Implemented behavior

`RequiredPartitionRegions` declares up to 16 fixed world-space spheres, each 100–200,000 cm in radius and with 1–16 real map actor references. Each request creates its own transient anchor actor and `UWorldPartitionStreamingSourceComponent`. The source targets **Activated** cells in every runtime grid intersecting the sphere with an explicit radius. It follows the fixed region, independent of the camera or sequence actor. There is no optional grid-name filter that could accidentally produce an empty successful query.

Playback waits for all assets, conventional levels and partition sources. A partition source must be registered and enabled, its engine `IsStreamingCompleted()` query must succeed, and every declared actor must resolve inside that sphere in a visible streamed level of the same world, with initialization and BeginPlay complete. Persistent-level actors cannot serve as activation witnesses. Two consecutive readiness observations are required before resolving the actual participants and final-placement navigation. This lets streamed participants become available before binding; it does not spawn missing participants or activate disabled Data Layers.

The preparation timeout uses monotonic real time, remains bounded during pause/time dilation, and retires incomplete asset handles on cancellation. A request-specific Core ticker watchdog also retires ownership if component ticking is disabled; unregistered or non-ticking components cannot accept a request. Sources stay owned throughout playback, pause, restoration and the beat commit. Readiness is rechecked during playback and before commit. Abort, timeout, EndPlay and component unregistration disable and destroy only that request's sources and anchors. They never turn off player streaming or force-unload cells another source needs. Existing classic-level loading requests retain their prior behavior; there is no new global classic-level lease manager.

`TransitPostconditions` names up to 16 uniquely identified real `ASovWorldTransitActor` instances. Each may set power, lock reason, or both. Targets must remain alive, authoritative, unique and at the same stable endpoint throughout the scene. Natural completion and permitted skip execute the same native setters after Sequencer restoration and participant placement, then validate all world and participant postconditions before requesting the existing campaign receipt commit. The existing mechanism remains responsible for navigation links, feedback and save/restore.

A failed commit restores only fields that still contain this request's applied value **and exact native setter revision**, only for the same stable mechanism and matching request/playback generation. `ASovWorldTransitActor` now increments separate transient power/lock revisions before each setter broadcast and invalidates them on load; another native setter's same-value write is distinguishable and preserved. Identity, world, endpoint, health and epoch are rechecked between rollback callbacks, so a callback that breaks a mechanism prevents subsequent lock restoration. Direct external C++ writes that bypass the existing setters are outside this contract. This is bounded rollback of typed native fields, not rollback of arbitrary Blueprint callback effects. Door/lift motion and destruction/repair cannot be requested through this transit contract; inventory mutation uses the separate existing-inventory journal described above.

Two additional source defects were repaired: cinematic validation no longer reads Narrative's protected `CurrentSlot`/`WieldAttachmentConfigs` fields, and an already-completed placed cinematic actor can accept preparation again after restoring a checkpoint where its beat is incomplete. The normal completed-beat gate still rejects duplicate completion.

## Authoring and remaining acceptance

For each partitioned scene declare spheres covering the entire camera path, participants and final placements. Assign at least one spatially loaded map actor in each sphere as an activation witness; declare all gameplay-critical actors that must remain available. Data Layers must already be in the mission-authorized runtime state. The managed sequence actor and current player must exist at the physical entry before `RequestPlay`. Leave the partition manifest empty in conventional worlds; declaring it in a non-partition world fails explicitly.

The manifest asks Engine for the cells intersecting authored regions rather than persisting generated runtime-cell names. HLOD/Data Layer authoring, memory budgets, runtime spatial hash versus runtime hash set behavior, PIE soft-reference remapping and packaged-map activation must be verified in the real 5.7 project. No cell content exists in this checkout, so full streaming success is not inferred from portable tests.

Risk: regional prefetch can increase memory use and compete with player streaming. The bounded manifest is a validation limit, not a shipping memory budget. Typed transit callbacks can destroy actors or perform additional external side effects; such callbacks fail the scene and may require checkpoint recovery. Saving after a successful scene continues through the existing campaign canon checkpoint scheduling.

## Validation and definition of done

Executed the production cinematic policy suite with C++17, warnings as errors and UBSan: **41,878 assertions passed**. It covers region limits/nonfinite inputs, witness radius boundaries, two-observation readiness, timeout boundaries, playback progress/first-view proof and participant capsule clearance. `git diff --check` passed for this change.

Added five Unreal automation cases under `ProjectVelkorran.Campaign.Cinematic` for bounded/duplicate manifests, conventional-world rejection, no-source readiness rejection, exact owned source retirement, zero-game-delta timeout, unregistration/re-entry and tick-disable cleanup, typed transit apply/restore, conflicting callback preservation, same-value rewrite ownership and damage during rollback. These tests are authored and **not executed** here.

Engine acceptance before calling this playable:

1. Build ProjectVelkorranEditor with UHT/UBT in UE 5.7 and run the cinematic automation group plus existing campaign/save regressions.
2. In a cooked partition test map start with the remote scene region unloaded. Verify no playback/binding before real Activated cells and actors, then complete at 30 and 60 Hz.
3. Repeat first viewing, permitted skip and pre-scene checkpoint reload. Both completion paths must leave matching transforms, weapon state, transit power/locks, journal receipt and post-scene checkpoint.
4. Disable a required Data Layer, delete a witness, use an invalid actor reference, obstruct a final capsule, unload/destroy a participant, cancel during loading and unregister during pause. The beat remains incomplete; input, tags, camera and owned streaming sources recover without damaging another source.
5. Keep an unrelated player/destination source active during success and failure. Verify its regions stay available; profile transition memory against the platform budget.

## Engine API references

Implementation uses Engine's documented [streaming-source component](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UWorldPartitionStreamingSourceCo-?lang=en-US), [explicit source shape](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FStreamingSourceShape?lang=en-US) and [World Partition activation/readiness behavior](https://dev.epicgames.com/documentation/unreal-engine/world-partition-in-unreal-engine?lang=en-US). The publicly retrieved pages currently label the latest engine version; version-pinned 5.7 pages could not be retrieved. These references informed the API selection and do not substitute for the required 5.7 compile/runtime gate.
