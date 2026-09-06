# Passive Shield/Poise ownership reliability

Source follow-up to the retained-corpse/shared-ASC finding in
[the combat error pass](CodeErrorPass-2026-09-05-Combat.md).

## What changed

`USovShieldComponent` and `USovPoiseComponent` previously treated any non-null,
authoritative ASC as writable. An old pawn retained through possession or a corpse
retained through checkpoint recovery could continue recharge, break-recovery or
loose-state-tag writes against a shared ASC. Their continuation paths could also
overwrite replacement timer/state work performed by synchronous callbacks.

The existing components remain the only owners of these lifecycle rules. Their
bindings now require the owner's canonical ASC, the current ASC avatar, the exact
registered `UNarrativeAttributeSetBase`, and this actor's first matching component.
Narrative's existing actor-info epoch rejects away-and-back avatar ownership;
the registered attribute set's existing life epoch rejects zero-Health/resurrection
reuse. The compatibility API still accepts a base `UAbilitySystemComponent`; such
an ASC has identity/attribute/liveness checks but no Narrative actor-info epoch, so
the ABA guarantee applies to the project's Narrative ASC implementation.

Passive writes additionally require authority, finite positive Health, no Narrative
dead flag/tag and no Sovereign fatal tag. A direct Health decrease to zero stops
all lifecycle timers and releases owned state contributions before delayed death
notification. Incoming callbacks and timers retire stale bindings when observed.
An explicit readiness initialization or checkpoint reset is required to admit a
new life: restoring Health alone does not revive an old timer schedule. Ordinary
Narrative `Revive()` also supplies that explicit transition: a generation-fenced
next-tick readiness reset runs after its attribute-initializing death observers.
Life-stale resource callbacks retain this lifecycle observer while rejecting all
old passive writes, so delegate ordering cannot lose ordinary revive readiness.

Tag ownership is assigned before GAS publishes an add, and retired before GAS
publishes a removal. Cleanup snapshots its own contributions and exact previous
ASC before callbacks, preserving external tag counts and replacement bindings.
Callback continuations check binding generation and ownership before later state,
timer or presentation work. Already committed resource changes are not rolled back
against a replacement owner.

## Restore contract

1. `SetCheckpointRestoreInProgress(true)` stops passive lifecycle timers and
   suppresses resource-change reactions during canonical resource deserialization.
2. The existing resource pipeline restores resources and calls `ResetForCheckpoint`.
   Reset may explicitly rebind a new life, clears prior owned state, and derives
   current state without manufacturing combat break/recovery presentation.
3. `SetCheckpointRestoreInProgress(false)` resumes only the current living binding.
   A stale avatar, different registered attributes, corpse or retired life cannot
   restart an old schedule through this release call.

The canonical resource loader uses `GetCheckpointRestoreGeneration` and
`EndCheckpointRestore(generation, resume)` to retire precisely its own suppression
lease. Failure releases the original hold without resuming work, including after
an ASC handoff; a newer hold on the same component cannot be released by the old
invocation. A successful explicit lifecycle reset is required before resumption.

Dead/zero-resource authored setup is still valid. Cleanup is not subject to the
living-writer gate, and reset itself may clear/reconcile a dead owner's metadata.
Player-only Health recharge rules and storage were not changed. Shield and Poise
still use the existing delays, rates, block tags, replication and timer ownership;
invalid nonfinite passive tuning is rejected instead of entering a timer/write.

## Regression coverage and limits

`SovPassiveDefenseRuntimeTests.cpp` adds twelve native tests in
`ProjectVelkorran.Campaign.PassiveDefense`:

- Living delayed recharge, regeneration and broken-Poise fallback/recovery control.
- Retained pawn with a shared ASC; only the new canonical avatar can resume.
- Actor-info away-and-back reuse cannot revive an old timer.
- Health zero before death notification, and resurrection without readiness.
- Checkpoint suppression, explicit new-life rebind and post-commit resumption.
- Avatar handoff from the real Poise refill attribute notification.
- Replacement recharge created inside the public Shield change delegate.
- Death inside loose-tag addition with an external contributor retained.
- Removed registered attribute sets cannot receive passive writes.
- Positive-Health fatal/dead-tag actors cannot recharge or recover.
- Actual Narrative revive with Health/Shield/Poise initialization both before and
  after the components' death observers.
- Recursive same-owner damage during Poise refill: no false recovery presentation,
  then a fresh bounded fallback retries and successfully recovers.

## Coherent resource-restore ownership

`USovEncounterSnapshotLibrary::RestoreResources` now freezes the caller's snapshot
before callbacks, reserves one restore per ASC, pins its exact owner/avatar and
registered attributes, and validates canonical ownership after each callback.
Narrative actor-info epochs reject avatar ABA. Only the explicit initial revive
or Health stage may consume one admitted zero-to-positive life transition; death,
revival or raw Health zero in another resource callback aborts immediately.

After legitimate NPC revival has initialized authored maxima, the loader freezes
those maxima, restores explicit current values, resets the captured components,
completes the existing generic-status restore barrier, and verifies the entire
resource/maxima vector. Passive release itself remains inside the checked
transaction because zero-duration Poise recovery can invoke callbacks immediately.
Once release starts, ordinary gameplay may legitimately advance Poise from the
saved zero value; ownership/life checks still apply after each release.

`SovResourceRestoreOwnershipRuntimeTests.cpp` adds six native registrations:
frozen input/reentrant restore, actor-info ABA with unpoisoned retry, unauthorized
life transitions or raw Health zero during resource callbacks, exactly one
controlled revival, removed registered attributes, and callbacks rewriting earlier
resources/maxima. These tests supplement the existing saved-maxima retuning tests.

Failure is fail-fast, not transactional rollback: already published resource
mutations remain committed, and the caller must keep the encounter suspended and
perform its established failure/retry flow. This intentionally avoids attempting
to undo writes against a replacement avatar or replay arbitrary callback effects.
Snapshot values remain resolved current values, not raw GAS bases; PR34's rejection
of persistent continuous resource modifiers remains unchanged. No save format
migration or parallel resource store was introduced.

Fixtures use one `UWorld::CreateWorld` initialization and real GAS attributes,
delegates and world timers, with Editor script execution enabled for production
dynamic handlers. Source whitespace checks are available here; UE 5.7 UHT,
compilation and these native tests have **not** run in this workspace. Engine
execution, replication behavior and authored mesh/animation presentation remain
acceptance gates. The attribute-removal regression uses Epic's documented
[ASC attribute-removal API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameplayAbilities/UAbilitySystemComponent/RemoveSpawnedAttribute?application_version=5.3);
that reference is not a substitute for compiling the branch with UE 5.7.
