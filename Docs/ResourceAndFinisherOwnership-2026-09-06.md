# Resource snapshots and accepted finisher outcomes

This source slice extends merged `main` at `93bf5c2c6e292e7ca0df05c3fe3ae677cadca57e`.
It addresses August TDD sections 6.11 (earned finishers and phase outcomes), 7.2
(authored checkpoint Echo), 11.8 (stable retries), and the save/load engineering
requirements. Unreal compilation and native automation have not run here.

## Resource contract

`FSovCombatResourceSnapshot` now captures schema 2: each of Health, Shield,
Stamina, Poise and Echo has both its resolved current and underlying GAS base,
plus resolved/base maxima. Existing fields remain available to UI and validation.
Older records default to schema 1 when the new property is absent.

The resource owner restores bases once. It does not write definition/progression
maxima from a save. With unchanged or higher maxima, the saved absolute base is
preserved, including a base hidden by aggregate clamping; a lower current maximum
caps the base. Current values resolve from that base and the presently reconstructed
modifiers. Expired or checkpoint-cleared effects are not recreated from a saved UI
number. Echo restarts its decay timing without writing the resource a second time.
An explicit zero-Health snapshot remains zero Health and does not invoke revive.

Continuous current-resource modifiers are admitted only when their magnitude is a
scalable constant and operation is additive. Modifier tag requirements/queries,
inhibited effects, attribute/custom/set-by-caller magnitudes, execution calculations,
and multiply/divide/override resource aggregators fail closed. Periodic resource
changes are base writes and are not counted as continuous offsets. Every saved
base and current is checked for finiteness; destination maxima are validated.
The adapter inspects actual active modifiers and verifies all restored bases,
resolved currents, maxima, and offsets again after component/status callbacks.
It does not serialize an arbitrary passive GE graph or promise that a grant owner
has reconstructed a missing effect. Existing definition, equipment and progression
owners retain that responsibility.

Schema 1 continues restoring absolute currents when the destination has no
continuous current-resource modifier. Ambiguous legacy modifier restores reject
before resource writes. Legacy files never stored the source aggregator, so this
cannot retrospectively recover data that was already lost in an older capture.

Callers intentionally authoring a resolved value after capture must use
`RebaseAuthoredResourceCurrents` before restoration. Companion rescue and native
handoff/status/recovery fixtures now use this explicit boundary. It recomputes
only changed currents' bases against the admitted active offsets, without mutating
the actor or discarding an unchanged aggregate's hidden base.

A restore pins its snapshot, ASC, avatar and attribute object. It fences ASC
actor-info/readiness epochs, attribute life epoch, and managed player initialization
generation. Nested restore and partial resource capture on that ASC reject. Own
Health revival is accounted for explicitly; additional callback-driven lives are
stale work. Shield/Poise restore holds use the component's binding generation, so
an old scope cannot release a replacement owner's hold. Pure preflight runs before
timer suspension. Failures after mutation are reported to the existing recovery
owner; this helper does not claim atomic rollback of arbitrary gameplay callbacks.

PR34 status serialization, pending queue, readiness completion and cleanup fences
remain intact. **Persistent continuous status modifiers to resources or maxima
remain unsupported.** Campaign bases alone do not migrate generic Narrative saves
or coordinate arbitrary deferred status reconstruction. `SovStatusComponent`
continues rejecting that authoring until those paths share a complete contract.
The historical PR34 document describes the old resolved-only campaign schema;
this document supersedes that specific limitation for the bounded campaign path.

## Finisher contract

The aligned elite/boss finisher reserves a target-owned pending outcome while
applying its damage GE. This token is distinct from the ability lease. Cancelling
an ability during the Health callback therefore neither erases accepted damage
nor allows a second finisher to reserve the target mid-transaction.

The strike listens for the exact effect-context, exact target native damage
receipt. Native proof and current target life are required, consumed once by that
receipt object. Accepted receipt flags are monotonic. The phase is added to the
target's saved `ResolvedPhases` only after positive accepted damage and unchanged
target component, ASC, attributes, actor-info/readiness/life generations. A GAS
application veto, zero-damage nonlethal strike, replaced target or loaded target
cannot consume the phase. Source ability cancellation may still complete the
captured accepted outcome for the same target generation.

While a phase damage transaction is pending, target component save preflight
rejects a half-committed record. Committing clears the pending token before the
phase event is published, so reentrant capture at that point sees both resource
damage and the committed phase. Load/end-play invalidate pending tokens. Outcome
payloads preserve captured instigator, target, phase and context. Existing normal
damage/kill rewards retain their owners; this slice adds no new resource reward.

## Validation evidence and engine gates

Executed locally: the new `SovResourceSnapshotPolicyTests.cpp` portable suite,
compiled under GCC C++17 with warnings as errors and undefined-behavior sanitizer;
`git diff --check` was clean at the slice review point. The portable suite runs
production-used base/max/current and finisher commit policy, not GAS or UHT.

New native cases cover:

- real additive GAS modifiers, reflected resource-save round trip, repeated
  restoration, modifier expiry, Echo's single write, and a reduced maximum;
- unsupported override and ambiguous legacy preflight with preserved caller state;
- nested restore/recapture and same-pointer ASC rebind during a resource write;
- zero-Health snapshot preservation;
- finisher GAS application veto after native preflight, pending save/reentry
  rejection, zero-damage phase rejection, and target-generation replacement.

The existing accepted-finisher-cancellation test still checks exactly one durable
phase event after a Health callback cancels the source action. Existing native
handoff, recovery and status queue tests now author schema 2 currents explicitly.
Run the following after UE 5.7 Editor and game compilation/UHT pass:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -BuildOnly
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -SkipBuild -TestFilter ProjectVelkorran.Campaign.Encounter.Resources
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -SkipBuild -TestFilter ProjectVelkorran.Campaign.Finisher
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -SkipBuild -TestFilter ProjectVelkorran.Campaign
```

Also exercise authored effects on both protagonists, companion rescue from the
fatal frame, protagonist handoff and reload, and rejection/fallback with an
unsupported GE. Passing the portable policy is not evidence of engine execution.
