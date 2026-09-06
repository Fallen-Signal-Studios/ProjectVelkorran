# Combat error pass — 5 September 2026

Follow-up: [combat transaction reliability, 6 September](CombatTransactionReliability-2026-09-06.md)
addresses the Echo startup, pickup reservation, active-melee interruption and
weak-point receipt findings below. This document retains the historical audit;
the follow-up distinguishes source repairs from pending UE validation and lists
the combat liabilities not included in that batch.

Source review began at merged HEAD `6b754d8`. This note distinguishes fixes in the working tree from verified source liabilities that still require implementation. It is not an Unreal compilation or playtest result. Earlier `AdversarialAudit-2026-09-05` findings were checked against the current implementation rather than assumed to remain open.

## Fixed in this pass

### Deflection could continue after cancellation during startup

`Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_SeleneDeflection.cpp:121` now captures an activation epoch and the original avatar/ASC, claims component cleanup before opening the defense window, and checks ownership after synchronous startup, cost, montage, and presentation callbacks. Ending validates the GAS end request, honors a scope-locked end, retires the recovery task, and cleans the exact component belonging to that activation.

`Source/ProjectVelkorran/Private/Components/SovDeflectionComponent.cpp:144` gives each window an epoch; reentrant tag/start callbacks cannot let an old start continue over its replacement. The owned tag flag is set before GAS publishes the tag change (`:270`), so a tag listener can actually remove the contribution it is cancelling. Admission also rejects nonfinite timing/stamina and a stale ASC avatar.

New native tests in `Source/ProjectVelkorran/Private/Tests/SovDeflectionRuntimeTests.cpp` cover cancellation during the tag-add callback, cancellation from the component start event, preservation of externally owned tags, cancellation/restart during actual GAS activation, and subsequent recovery. These tests are authored, not executed here. Direct component calls outside the GAS ability still lack continuous death/status interruption listeners; this fix addresses callback ownership during opening/closing, not that separate integration path.

### A committed elite/boss finisher could lose its phase event

`Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_Finisher.cpp:211` captures phase-event identity before applying damage and delivers a committed phase event even if a damage listener cancels the player's action. Presentation remains conditional on the live action. Destroyed targets do not receive a late event.

`Source/ProjectVelkorran/Private/Tests/SovFinisherProjectileRuntimeTests.cpp:107` adds `CommittedOutcomeSurvivesCancellation`: a real GAS activation and target reservation, a forced aligned strike for isolating the phase transaction, cancellation from Health change, retained instigator/target/phase, nonlethal elite damage, and no duplicate event. It does not certify navigation, authored montages, save-time reentry, or target destruction/recreation across saves.

## Remaining source liabilities

| Priority | Location | Trigger and implication |
| --- | --- | --- |
| P1 | `Private/Abilities/SovGameplayAbility_Echo.cpp:204,306` | An Echo attribute/spend listener can cancel during `TrySpendEcho`; the outer assignment then sets spend success again and calls Narrative activation before checking `IsActive`. The shared base needs its own payment/activation epoch and valid-end discipline. Subclass epochs do not protect this base continuation. |
| P1 | `Private/Combat/Pickups/SovCombatSustainPickup.cpp:173` | `TryGrantTo` publishes Echo/inventory callbacks before `bClaimed` is set. Reentrant overlap can grant the same resource pack twice. Ammo also subtracts remaining pack quantity after the inventory callback. An in-progress collection reservation is needed before resource mutation. |
| P1 | `Private/Melee/SovGameplayAbility_Melee.cpp:56` | Active melee checks death, equipment and sequencing, but omits newly applied broken Poise/Frozen/ragdoll/interaction state. Activation blockers alone do not interrupt a running sweep. Damage or charged release can continue after the attacker is staggered. |
| P2 | `Private/Abilities/SovGameplayAbility_TarrikEcho.cpp:1121` | Cinder Judgement traces from a socket muzzle without checking the eye-to-muzzle segment or rejecting reverse convergence. A muzzle clipped through a thin wall can place an explosion on its far side. Requiem already demonstrates the bridge/forward-direction policy (`SovGameplayAbility_TarrikCinderlineRequiem.cpp:92`). |
| P2 | `Private/Abilities/SovGameplayAbility_ReformationDrone.cpp:348,1159` | The release hook can apply DeviceDisabled, but continuation checks only `IsActive`; rocket release can proceed while disabled. Generic drone weapon actions also lack the immediate interruption listeners used by Hound/Handler. |
| P2 | `Private/Components/SovShieldComponent.cpp:1137`; `SovPoiseComponent.cpp:866` | The write gate checks initialization and authority, not current avatar ownership or death/fatal state. A retained corpse or old pawn with a shared ASC can continue passive resource/state writes unless another system tears it down first. |
| P2 | `Private/Components/SovWeakPointComponent.cpp:520,542` | Every accepted unbroken-zone hit enters `ResolvedHitTransactions`; the set has no eviction/reset. Persistent targets and repeated retries accumulate IDs. Retention must preserve replay rejection through an explicit receipt generation/window, not simply clear the set arbitrarily. |

Paths in this table are relative to `Source/ProjectVelkorran`. These are source-supported execution paths, not claims that any particular authored map has already failed. Central damage serialization, merged corruption, inventory internals and campaign orchestration are covered by the coordinating audit.

## Validation

- `git diff --check`: clean after these edits.
- `Tests/Portable/SovRemainingCombatPolicyTests.cpp`: compiled with Clang, C++17, warnings as errors and undefined-behavior sanitizer; **29 assertions passed**. This validates existing portable finisher/projectile/channel policies, not the new callback lifecycle changes.
- Three new native automation tests require UE 5.7 compilation/UHT and execution before runtime acceptance. No matching engine build was available during this review.
- The event-listener API used by the new finisher regression was checked against [Epic's UAbilitySystemComponent API reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameplayAbilities/UAbilitySystemComponent).
