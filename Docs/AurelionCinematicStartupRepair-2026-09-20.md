# Cinematic startup readiness and cancellation repair

Status: explicitly approved by the user and implemented. Full build and 726 automation tests pass. The live unpaced earned-checkpoint M12-to-M13 route also passes, including the observed pending-load window and all thirteen M13 receipts.

The FifthWitness failure captured in `Saved/Validation/Aurelion/SeleneLiveFaceAudit-20260920-174328-8536de71` transitions from Loading to Failed while Selene is alive, initialized and visually present, but still reports pending character load. A later route paced by five seconds succeeds; that is not a regression pass for immediate startup.

`USovCampaignCinematicComponent::StartPreparedPlayback` previously called the engine player's `Play()` directly. Narrative's `OnPlay` rejects pending participants immediately. Its existing `PlaySequence()` instead queues a bounded readiness wait. That method was protected; this repair moves its declaration into the public section for managed native callers. The component stays in Loading until its OnPlay callback and avoids repeated preparation once it reserves a playback generation.

## Implemented source changes

In `Source/ProjectVelkorran/Private/Cinematics/SovCampaignCinematicComponent.cpp`, at the end of `StartPreparedPlayback`:

```diff
-    Player->Play();
+    Actor->PlaySequence();
```

Cancellation also needs an explicit pending-state operation: engine Stop is a no-op on an already-stopped player, as Narrative's own `FinishBlendOut` documents. Without cancellation, the queued actor could start after the managed component has released input ownership.

Added a native method to `ANarrativeLevelSequenceActor` beside its playback-generation accessors:

```cpp
// Cancel only an unstarted request belonging to the caller's generation.
void CancelPendingPlayback(uint64 ExpectedGeneration);
```

Implemented in the corresponding Narrative actor cpp:

```cpp
void ANarrativeLevelSequenceActor::CancelPendingPlayback(uint64 ExpectedGeneration)
{
    if (PlaybackGeneration != ExpectedGeneration) { return; }
    bPendingPlayback = false;
    PendingPlaybackSeconds = 0.f;
}
```

The existing generation-owned Stop block in `USovCampaignCinematicComponent::Abort` now calls `Actor->CancelPendingPlayback(ExpectedPlaybackGeneration)` before `Actor->GetSequencePlayer()->Stop()`. It preserves the ownership condition, participant restoration and delegate/input cleanup. Cancellation emits no callbacks and cannot cancel a newer playback generation. Timeout values and participant/binding validation are unchanged.

## Acceptance checks

1. Exercise the actual managed preparation entry point with a required character whose real visual-load state is pending. Assert Loading, stopped player and no completion receipt; finish the load and assert one transition to Playing with correct bound identities.
2. Abort while pending, then finish loading and tick the actor. Assert no late playback, no completion receipt and released managed input/sequence ownership.
3. Let pending readiness time out. Assert one terminal failure and no later start. Also exercise the managed loading watchdog while the actor is pending.
4. Replace the sequence generation while the old component is waiting. Old cancellation must leave the replacement request intact.
5. Exercise already-ready startup and finish/skip paths; ensure Narrative's binding refresh preserves the managed contract. Include reentrant start/abort callbacks.
6. Run the full Unreal build and automation gate without SkipBuild before and after applying the native repair. Then repeat the live FifthWitness route without the artificial five-second delay and inspect animation, input release and the native completion receipt.

The unchanged pre-repair baseline `Saved/Validation/20260920-212406-d1d83c83` passed its full build and 722 tests. Post-repair gate `Saved/Validation/20260920-213451-005b43d0` passed its full build, all 726 matching tests, exact report coverage and source integrity. Neither gate used SkipBuild. An initial compile attempt caught the protected `PlaySequence` declaration and a test getter access issue; both were corrected before the passing gate.

Four new regressions in `SovCinematicLifecycleRuntimeTests.cpp` exercise the actual `StartPreparedPlayback` method and native tagged binding resolver:

- `ManagedWaitsForRealVisualProducer`: gameplay-ready pawn plus unfinished owned visual, no premature play/proof, actual `OnBaseMeshesReady` completion, one start and the correct pawn binding.
- `ManagedPendingCancellationAndWatchdog`: explicit abort, managed loading watchdog and actor participant timeout; late appearance completion cannot start any retired request, input/leases/proof are released, participant timeout fails once.
- `PendingCancellationPreservesReplacementGeneration`: stale explicit cancellation and the old managed owner leave a newer waiting request intact; only the replacement starts.
- `ReadyManagedStartupAndReentrantAbort`: immediate ready startup and an abort dispatched within OnPlay do not leave orphan playback or ownership.

The fixture preserves the existing player-readiness scaffolding, applies startup effects through the native producer with an empty test configuration, and completes appearance through the existing visual producer fixture. It does not override participant readiness or the managed startup implementation. Existing finish, receipt, skip and interruption regressions also pass in the full gate.

## Live unpaced verification

Run: `Saved/Validation/Aurelion/CinematicStartupUnpaced-20260920-213727-1184bfea`, using the unchanged `review_restored_m13_route.py`. This uses a verified earned E4B exit save, public loading and ordinary input; it does not import the five-second pacing wrapper or modify character state.

The observer caught the real pending-load window during SeleneIndependentAssent: Loading with Selene pending at 142.375 seconds; Loading with pending cleared at 142.500; Playing at 142.610. The scene completed with native cinematic session `19CF13154853CD4A11B7B782CDA704B2`. FifthWitness subsequently completed without skipping with native cinematic session `7FF2BF7F469EB399543550BE0A53A1FC`, followed by GrammarPropagation and ordinary movement through the paired lift to CP8. This live run exercises the previously failing condition, rather than merely starting after loading had already completed.

Inspected frames `route-10.png` (Selene assent) and `route-16.png` (FifthWitness) show the loaded characters and readable subtitles. The FifthWitness capture records Selene using `ABP_Biped_C` and Tarrik using `AnimSequencerInstance`. This is sampled visual evidence, not exhaustive animation-quality qualification or a fix for the separate intermittent skin issue.

The route finished successfully: all thirteen M13 receipts, full dialogue timelines, native co-action arrival, independent handoffs, physical paired lift, CP7/8/9 and separate departures. All 22 existing M12 receipts remained unchanged. The wrapper correctly retains `passed_requires_visual_review`; the underlying gameplay drivers both report `passed`. This does not qualify a CP9 reload, full artistic quality or the overall 90% TDD goal. PIE ended cleanly and the editor remains open.

No content asset was saved by this repair. Map hashes before and after the live run match:

- M12: `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`
- M13: `634FD9172C6B2A54CB2903BA3B1993914D2F6D0C6E218D87D0D61F563BB6E71E`

## Approval boundary

`Docs/EngineImplementationHandoff-2026-09-18.md` says: “if a task seems to, stop and say so rather than editing C++ to fit the content.” The user explicitly approved this additional cinematic startup repair and its regression tests: “Yes go ahead.”
