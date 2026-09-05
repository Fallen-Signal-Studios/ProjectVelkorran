# Player combat adversarial repairs — 2026-09-05

Source implementation for audit findings PC-01 through PC-08 is complete. Follow-up review also closed finisher activation/reservation lifetime defects under ED-04 with the damage/outbox owner. This is **source completion, not UE 5.7 compilation or runtime certification**. No editor assets were edited, and this worker made no commits or pushes.

## Finding disposition

| Finding | Preserved architecture and repair | Dependency / validation gate |
|---|---|---|
| PC-01 — melee continues after poise break | Preserve Narrative combat ability, socket sweep task, finite attack definition and owned Busy tag. Subscribe to interruption tags for the active action; stop charge, sweep, input window and montage on break, death, equipping, freeze and conflicting Busy. Activation epochs fence Super, authored mesh resolution and commit callbacks; release/input/cancel continuation cannot operate on a replacement activation. End dispatch retires before a deferred GAS teardown. | Real Poise component and damage resolver must publish interruption tags. Native regression breaks Poise through the real execution in startup, active and recovery, then checks no later hit and no leaked Busy. Charged montage/socket timing still needs engine execution. |
| PC-02 — Echo cost callback can resume retired activation | Preserve single authority Echo debit and Narrative target-data dispatch. Capture epoch/spec/avatar/ASC before commit; check immediately after payment and every Blueprint boundary. A debit result cannot alter replacement payment flags. End fences continuation before scoped teardown; active duration timer captures its epoch. Tarrik/Selene release admission uses this same base ownership gate. Dispatch teardown detaches callbacks under a reentrant-end guard. | Actual `OnEchoChanged` cancellation fixture verifies a committed debit, no Started hook, no stale Narrative target-data binding and clean subsequent activation. A debit already committed is not refunded merely because a listener cancels its presentation. |
| PC-03 — negative/unpaid/reentrant ammunition | Preserve weapon clip + existing inventory stack. Reject nonpositive amount, insufficient loaded rounds and concurrent consume/reload. Reserve clip before quantity notifications, remove exactly the requested amount using inventory membership and quantity revisions, and grant payload permission only for the surviving weapon/owner transaction. Pin callback-live objects. Roll back a rejected debit only if the original resource/ownership revisions still match. Clamp clip views and use wider reserve arithmetic. Reload rejects ownership/write changes in authored getters. | Native fixture uses a real pawn, inventory and stack. Checks 0/-1/INT_MIN, removal denial, synchronous consume/reload from item notification, exact one-round debit, nonnegative raw clip and subsequent reload. Foreign resource replacement remains authoritative; a retired committed debit never authorizes a shot. |
| PC-04 — Judgement thin-cover bypass | Preserve configured weapon trace channel and radial LOS. Bridge-test eye to muzzle; a socket beyond cover starts its collision trace at the near-side eye. Reject reverse convergence relative to authority aim. | Actual physics wall fixture puts muzzle beyond a thin wall and enemy beyond its far face; no direct/radial damage may leak through. Authored skeletal muzzle placement remains an engine/content gate. |
| PC-05 — Judgement mutable release / inferred damage | Capture source ASC, avatar, source object, effect context, level and activation at release. Direct and radial damage use the existing synchronous native result receipt; healing/other damage in callbacks cannot masquerade as this shot. Every radial/physics continuation checks the original release. Recovery timer captures activation. A retired deferred presentation is destroyed. | Real direct-hit callback cancels and starts a second paid Judgement on the same instance. The first direct hit remains committed; its radial continuation and recovery cannot affect the new activation. |
| PC-06 — permanent hero GUID history | Remove lifetime transaction/attack GUID sets. Authoritative `FSovDamageResult` copies share a native receipt with weak consumer identity and channel bits (maximum 32 consumers / 8 channels per result, fail closed; no eviction). Heavy attack receipt retains at most two unrewarded unique targets and a permanent consumed latch for its own lifetime. Command-link sever copies share an issuer-owned latch, explicitly retired before reset, reactivation, restore or EndPlay. Existing protection/bypass proof owns its one-shot consumption. Reflection copies explicitly preserve native shared state. | Central damage publisher mints the result receipt before listeners (root-owned change). Tests cover C++ and reflected copies, manufactured/modified results, 1,024 subsequent real packets without old-copy replay, and an unconsumed sever result across same-instance checkpoint restore. No publication watermark was added: no normal damage replay queue was found. |
| PC-07 — ignored Selene GE overrides | Preserve canonical native damage/control execution. Previously ignored custom effect-class fields are explicitly deprecated, retain their serialized names for migration, and cease being editable runtime knobs. `ValidateNativeEffectOverrides` reports any noncanonical legacy class by exact property name; CheckCost rejects it before debit. Canonical or cleared values are accepted. Scalar/duration tuning and presentation hooks remain supported. | Native configuration regression verifies canonical, incompatible and cleared values. Build validation owner also invokes this validation from authored Blueprint inspection. Existing assets with compensated custom GE classes need migration; no asset was silently rewritten. |
| PC-08 — inverted recoil presets | Aiming uses aim translation ranges; hip uses hip ranges. Local controllers use the same local aiming tag as spread; other observers use authoritative aim state. | Deterministic actual weapon fixture verifies distinct hip/aim presets. Assets that compensated for the old inversion require a tuning review. |

## Finisher cross-review repair

The finisher now captures an action epoch and actor/ASC identity before Narrative activation. Target search, reservation, alignment, commit, protection effects and task activation cannot resume an old action after a callback restarts the same instance. Reservation uses a local lease until all checks pass. The watchdog finishes only its captured lease; it cannot cancel a replacement from inside a team-policy callback. Strike/finish/watchdog timers retain their action identity. End retires native dispatch before scope-lock deferral, guards recursive teardown and explicitly detaches the strike task.

The target-component owner separately hardened reservation after team-policy callbacks and retained committed phase delivery in a target-owned outbox. The ability deliberately publishes that already committed outcome after damage even if animation ownership is cancelled. This preserves ED-04's outcome semantics.

Three additional real-owner regressions reenter target search/reservation through a character team-policy implementation and exercise deferred virtual End teardown. The latter directly enters End under the ability scope and drains the actual queued delegates; it does not assume that UE's own `CancelAbility` dispatch reaches virtual End before an engine scope unlock.

## Files

Project source:

- `Public/Abilities/SovGameplayAbility_Echo.h`, `Private/Abilities/SovGameplayAbility_Echo.cpp` (also extended by authored-validation owner)
- `Public/Abilities/SovGameplayAbility_SeleneEcho.h`, `Private/Abilities/SovGameplayAbility_SeleneEcho.cpp`, `Private/Abilities/SovGameplayAbility_SeleneDispatch.cpp`
- `Public/Abilities/SovGameplayAbility_TarrikEcho.h`, `Private/Abilities/SovGameplayAbility_TarrikEcho.cpp`, `Private/Abilities/SovGameplayAbility_TarrikLifecycle.cpp`
- `Public/Melee/SovGameplayAbility_Melee.h`, `Private/Melee/SovGameplayAbility_Melee.cpp`
- `Public/Combat/SovEchoAttackReceipt.h`, `Private/Combat/SovEchoAttackReceipt.cpp`
- `Public/Components/SovCommandLinkComponent.h`, `Private/Components/SovCommandLinkComponent.cpp`
- `Public/Components/SovTarrikEchoGenerationComponent.h`, `Private/Components/SovTarrikEchoGenerationComponent.cpp`
- `Public/Components/SovSeleneEchoGenerationComponent.h`, `Private/Components/SovSeleneEchoGenerationComponent.cpp`
- `Public/Abilities/SovGameplayAbility_Finisher.h`, `Private/Abilities/SovGameplayAbility_Finisher.cpp` (shared ED-04 result retained)
- `Private/Combat/SovSelenePayload.cpp` — rename the anonymous liveness helper for unity compatibility (B-02 coordination)

NarrativeArsenal (`Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal`):

- `Public/GAS/SovCombatTypes.h`, new `Private/GAS/SovDamageConsumptionReceipt.cpp`
- `Public/Items/WeaponItem.h`, `Private/Items/WeaponItem.cpp`
- `Public/Items/InventoryComponent.h`, `Private/Items/InventoryComponent.cpp` — additive exact-consumption API; existing general inventory consumption contract preserved
- `Private/Items/RangedWeaponItem.cpp`

Regression files (subsequently relocated with the build owner's test-module migration): `SovPlayerCombatRepairRuntimeTests.cpp`, `SovPlayerCombatRepairTestFixtures.h/.cpp`, plus `SovMeleeRuntimeTests.cpp`.

## Validation performed and still required

Performed in this source-only environment: callback/ownership trace review of changed production paths, comparison of retired GE field defaults with every concrete Selene constructor, checks for orphaned lifetime ledger references, declaration/call-site review and `git diff --check` (passed). No UE executable/toolchain was available to this worker. New Unreal tests are written, **not executed**. Portable policy smoke results reported elsewhere do not validate these UE callback paths.

Eleven new native tests share the prefix `ProjectVelkorran.Campaign.Repairs.Player`:

1. `MeleePoiseInterruptionPhases`
2. `ExactAmmoAndRecoil`
3. `EchoDebitCancellation`
4. `JudgementThinCover`
5. `JudgementRetiredRelease`
6. `DamageReceiptReplayOwnership`
7. `LegacySeleneEffectValidation`
8. `FinisherSelectionReactivation`
9. `FinisherScopeLockedEnd`
10. `CommandLinkReceiptRestore`
11. `FinisherStrikeReactivation`

UE 5.7 still must run UHT, non-unity and normal unity builds, this repair prefix and existing melee/Echo/weapon/Selene/Tarrik/finisher suites. Specifically exercise real engine ability-task `CancelAbility` scope ordering, Blueprint K2 activation cancellation/reentry, montage notifies, server/predicting-owner target data, forced collection during removal callbacks, and source avatar replacement. Run tests against authored weapon/ability assets after migration. These remain validation gates, not claims that editor or console execution occurred.

Changing cancellation, resource commit and replay ownership has medium integration risk: source-owned invariants are stricter and expose assets that depended on ignored overrides, stale notifies or implicit clip state. No parallel combat or inventory system was introduced. The remaining audit items and shipping qualification stay in the consolidated repair report.
