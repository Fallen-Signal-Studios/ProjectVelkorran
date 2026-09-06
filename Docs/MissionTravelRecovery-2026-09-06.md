# Verified-origin mission travel recovery

Slice 1 of the next three alignment slices, based on merged PR #34. Authority: August 2026 TDD v2, sections 11.7, 15.9–15.16 and 18.5, with `CampaignV2ChangeLog.md` exceptions. The December attachment is not the active kit/campaign specification.

## Implemented

`USovSaveSubsystem` remains the GameInstance lifetime owner of saves. `SovMissionTravelRecovery.cpp` extends that subsystem; there is no second serializer, slot format, or parallel transition subsystem.

- Before mission ServerTravel, retain the exact verified origin checkpoint envelope and decoded Narrative snapshot. Reject damaged-bank implicit fallback and checkpoints from another mission. A deliberately acknowledged failed checkpoint can use an earlier verified checkpoint of this same mission; recovery then returns to that older boundary, not unsaved progress.
- Capture account/selection/authorization ownership and a mission request GUID. Controller transfer serialization uses before/after fences; destination transfer reads acquire a fresh synchronous storage fence within the retained transaction.
- Validate the destination GUID, mission ID, definition and actual map before campaign staging. Missing mission configuration fails closed. Mission completion requires the managed pawn-readiness path, not ServerTravel acceptance.
- Bind engine travel/network failure delegates at GameInstance subsystem initialization. Failure callbacks only latch state; a core-ticker continuation starts one recovery through the existing GUID-correlated Narrative slot-loading path, without needing the source controller.
- Account/authorization replacement invalidates automatic work. An explicit original-account retry can acquire new authorization; no different selected account can consume retained bytes.
- Suspend holds the travel watchdog. Destination transfer I/O attempted while suspended fails closed and recovers after foreground rather than reading platform storage in the background. Already-decoded pending slot loads retain their existing same-owner suspend hold.
- Recovery errors release active ownership before the ordinary `OnLoadCompleted` failure notification. The exact origin remains available through `RetryMissionTravelRecovery`; each attempt receives a separate duplicate of the configured Narrative snapshot, so failed live-save mutations cannot contaminate retry. No automatic recursion. Late source-world failure callbacks cannot cancel the newer origin request. Uncorrelated engine failure is bounded by the load watchdog.
- An active travel transaction excludes competing saves, account selection and slot loads. Platform interruption now pauses the still-live source world in `Travelling`; destination readiness states remain unpaused.

See `SaveOperationOwnership-2026-09-06.md` for the coordinated storage-boundary, queued autosave, retry and acknowledgment changes.

## Validation

Five new `ProjectVelkorran.Campaign.Save.MissionTravel.*` native registrations exercise engine failure broadcast deferral, exact retained pending snapshots, bounded/explicit recovery, owner ABA, cancelled/stale requests, competing load rejection, invalid destination definitions/maps and suspend timeout behavior. They use actual subsystem methods and an overridden final OpenLevel boundary; they do **not** execute real map loading. Ten additional save-operation native registrations cover storage and serializer callback injection.

Host source-layout checks and portable C++ policies run independently of Unreal. They cannot establish UHT success or native runtime correctness.

## Required engine qualification / definition of done

1. UE 5.7 UHT and modular Editor compilation; run every source-discovered native test, not a stale test count.
2. Authored M01 completion → accepted M02 travel → ready Selene, with independent protagonist snapshots intact and exactly one mission-start boundary.
3. Inject missing destination dependency, missing/wrong InitialMission, transfer decode failure, source-PC destruction, destination readiness failure/timeout, and origin recovery failure. Observe one origin attempt, one explicit failure notification, preserved banks and successful explicit retry.
4. Sign out, reauthorize, suspend/resume, and unplug the owning controller at each boundary. Never issue unauthorized I/O or mutate a newer request. Verify the interrupted source simulation is paused while destination readiness can still proceed.
5. Hook the existing load-result notification and explicit retry API to authored recovery UI, including the front-end path when no campaign controller survives. Verify accessibility/controller navigation in the packaged game.

No UE installation or target SDK is available in this workspace; compilation, native tests, multiworld travel and packaged recovery UI remain unexecuted. This is source implementation, not a claim of an airtight engine-qualified build.

## Deliberately preserved limits

- Resource snapshots still store resolved current values. PR #34's rejection of persistent continuous resource/max modifiers remains; changing that requires a coordinated base/current schema migration, not removal of the safety gate.
- No new bounded outer archive format or cloud revision selection policy is introduced (SP-05 / SP-04 remain separate work).
- This transaction covers native campaign mission travel and its origin recovery, not arbitrary external OpenLevel calls or a full transactional rollback of arbitrary Blueprint callbacks.
- Engine failures without a correlated world are resolved by timeout rather than attributed to another transaction. Real engine failure-order behavior requires the injected packaged route above.
