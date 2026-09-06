# Shield and Poise ownership across interruption and restore

This source slice addresses the retired-avatar regeneration finding in the September 6 TDD alignment review. It changes the existing Shield and Poise lifecycle components, with no balance or resource-schema changes.

## Ownership contract

- An initialized component must still own the exact current ASC avatar. Narrative actor-info and character-readiness epochs must also match the captured binding. Switching away and back to the same avatar pointer requires a fresh binding.
- Resource writes require a living, authoritative owner: a valid actor, positive finite Health, and a Narrative ASC that is not dead. Zero Health and death notifications retire all regeneration, break-fallback, and recovery timers. A later positive Health write cannot resume a retired timer.
- Timers carry the lifecycle generation that scheduled them. A checkpoint reset, restore hold, death, teardown, or rebind prevents their continuation. A stale callback may clear its own generation's timers, but cannot clear timers installed by a replacement binding.
- Attribute notifications, material notifications, state transitions, and owned-tag changes can invoke synchronous gameplay callbacks. Continuations validate ownership again after those calls. A callback that replaces the ASC, changes readiness, kills the owner, or starts a restore stops the old operation.
- Loose-tag ownership is recorded before invoking GAS. Teardown detaches all local fields before releasing captured contributions from the old ASC. A reentrant binding survives that cleanup, and unrelated tag contributors retain their counts.
- Narrative publishes the initial readiness epoch after its legacy `OnASCInitialized` notification. Components listen for the later epoch and rebind even when the ASC pointer stays the same.
- Hits against an already-empty Shield restart recharge only from a current native target-life receipt, consumed once by this component. Replayed or retired-life damage notifications cannot postpone the replacement binding's recharge.

## Checkpoint integration

The original `SetCheckpointRestoreInProgress(bool)` API remains available. Native restore code can capture `GetBindingGeneration()` and use the two-argument setter for both begin and end. An old scope cannot release a new binding's restore hold.

Beginning a hold clears all old timers immediately. `ResetForCheckpoint()` can derive restored state while the hold remains active, but it defers arming regeneration or break recovery. Ending the matching hold after that reset starts the complete new resource delays. A restore that fails before lifecycle reset leaves those timers retired until an explicit retry/reset.

## Verification

Six content-free Unreal automation cases were added in `SovPassiveResourceOwnershipTests.cpp` under `ProjectVelkorran.Campaign.PassiveResources`:

1. Shared ASC avatar replacement, then returning to the original pointer, cannot resume an old Shield/Poise timer; explicit rebinding restores valid regeneration.
2. Zero Health retires Shield recharge and broken-Poise fallback. A later Health refill does not resurrect them; an explicit checkpoint reset starts new work.
3. A checkpoint hold prevents both recharge and break recovery while attribute writes and lifecycle reset run; completion begins full fresh delays.
4. Initial readiness-epoch publication rebinds both components, and cleanup from a retired restore scope cannot release the new hold.
5. Native loose-tag callbacks replace the ASC during Shield break and Poise recovery. Old work cannot mark/refill the replacement, and unrelated old tag contributions survive.
6. A real hit against a depleted Shield starts its delay; replaying the same receipt or replaying it after a readiness rebind cannot restart that delay. A copied in-flight delegate with an unconsumed receipt also cannot restart a replacement ASC's delay on the same avatar. The receipt API accepts an optional expected target ASC while preserving existing callers.

The tests use real Narrative ASCs, attributes, components, tag callbacks, and world timers. They require Unreal Engine. No Unreal build, UHT pass, or automation execution was available in this workspace; these cases are authored coverage, not a claimed engine pass. Local verification consists of source review and whitespace/diff checks. Existing encounter lifecycle-reset tests remain relevant for count preservation and fresh-delay behavior.
