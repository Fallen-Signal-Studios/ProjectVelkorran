# Stamina, sprint, evade and attack exertion

Native completion of TDD §§4.6–4.7, 6.5 and the expenditure portions of §6.8. The TDD's prototype numbers remain editable. Existing Narrative Stamina, MaxStamina and StaminaRegenRate attributes are the single resource store; the approved perfect-defense exhaustion behavior is preserved.

## What was missing

The previous implementation had replicated stamina attributes, guard/deflection impact costs and Narrative movement's sprint request, but no native delayed regeneration controller, combat sprint consumption, exhausted evade rejection, character-specific evade payload, or charged-release/cancel cost transaction. `UNarrativeAnimSet` contains montage pairs; it is not an attack-state or resource controller. These foundations were extended rather than replaced.

## Runtime ownership

- `USovExertionComponent`, attached to `ASovPlayerCharacterBase`, initializes alongside the existing resource components and participates in readiness. No stamina regeneration Gameplay Effect is added.
- All native spending validates authority, live health, exact ASC/avatar ownership, readiness, finite nonnegative cost and full affordability. Callbacks cannot reenter a pending resource mutation. Attribute decrements from guard and deflection restart the same regeneration delay.
- The component runs before Character Movement. It consumes combat sprint stamina by elapsed game time, clamps continuous drain at zero, clears sprint intent on exhaustion and leaves discrete unpayable actions unstarted. It uses the existing Echo encounter boundary to distinguish combat. Outside that boundary sprint costs zero, including at zero stamina. Encounter authors must use the encounter director or existing Echo `BeginEncounter`/`EndEncounter` lifecycle.
- Regen splits a frame at the remaining delay, so a frame crossing the deadline receives only its eligible portion. Idle uses the existing StaminaRegenRate attribute; attacking, guarding, deflecting, evading or sprinting uses the configured lower rate. Current/max values remain bounded by Narrative's attribute set.
- The exhausted state is visible through `Sov.State.Exertion.Exhausted`, `IsExhausted`, attribute change events and `OnStaminaSpent`. Checkpoint restoration stops sprinting and restarts the delay without altering the restored resource.
- A legacy active periodic GE modifying Stamina suppresses native regeneration, avoiding two simultaneous regenerators. Remove the legacy authored stamina regeneration effect when integrating this component. Legacy regeneration is not rewritten or silently removed.

## Prototype profiles

| Setting | Tarrik | Selene | Authority |
|---|---:|---:|---|
| Maximum stamina | 120 | 100 | TDD prototype |
| Idle regeneration | 32/s | 38/s | TDD prototype |
| Regeneration delay | 0.65 s | 0.55 s | TDD prototype |
| Evade cost | 24 | 20 | TDD prototype |
| Evade invulnerability | 0.18 s | 0.27 s | TDD prototype |
| Walk/run/sprint | 210/540/730 cm/s | 225/600/820 cm/s | TDD prototype |
| Active regeneration multiplier | 0.5 | 0.5 | New editable tuning; TDD specifies faster idle only |
| Combat sprint drain | 16/s | 14/s | New editable tuning; TDD specifies consumption only |
| Evade distance | 240 cm | 420 cm | New editable tuning preserving the specified relative distances |
| Evade duration | 0.32 s | 0.42 s | New editable movement tuning |

`bApplyPrototypeDefaults` applies base max/rate and the existing movement speeds once at avatar initialization, before campaign snapshot restoration and Technique grants. Disable it when the default attribute/movement assets deliberately own these values; delays, costs and evade profiles still come from the component. Invalid nonfinite or contradictory profiles fail initialization.

Accessibility costs use `UNarrativeGameUserSettings::GetExertionCostScale`. Defensive assistance scales evade invulnerability but clamps it to the unchanged evade duration, so assistance does not lengthen input lockout.

## Evade and sprint

`USovGameplayAbility_Evade` uses semantic input `Narrative.Input.Evade`; `USovGameplayAbility_Sprint` uses existing `Narrative.Input.Sprint`. Both are standalone-authority campaign abilities. Grant these through existing player definitions and input mapping. They reject an additional Cost Gameplay Effect because expenditure already has a native owner.

Evade admission checks full payment, grounded movement, readiness, existing root-motion ownership and a capsule sweep using the capsule's collision responses. An obstructed move shorter than 12 cm or starting in penetration rejects without payment. A partial clear route is capped before the obstruction. Movement then uses a bounded constant-force root-motion source through Narrative's Character Movement component, whose ordinary capsule collision continues to handle moving obstacles. It never disables Pawn collision or teleports through enemies.

Tarrik retains his facing. Selene faces the requested movement direction. Ordinary movement rotation is temporarily disabled and restored on cleanup. Finite owned effects grant Busy/Evading and the shorter immunity window; cancellation removes only those effect handles and the ability's root-motion source. Death, cinematic control, interaction, ragdoll, weapon equipping, frozen state, poise break, invalid avatar ownership and timeout terminate the payload. Narrative combat abilities explicitly reject activation while Evading, including Blueprint combat children with no attack asset tags.

Optional `ReceiveEvadeStarted`/`ReceiveEvadeEnded` hooks supply animation, sound and camera presentation. Use an in-place evade montage with this native displacement so animation root motion does not apply a second movement trajectory. Mantles, vaults and contextual traversal continue to use Narrative's existing authored movement paths.

The native movement choice follows Epic's [root-motion source architecture](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FRootMotionSource) and [constant-force partial-frame integration](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FRootMotionSource_ConstantForce/PrepareRootMotion). The public documentation does not replace the pending UE5.7 compilation gate.

## Charged releases and defensive cancels

`ISovExertionProvider` is a small native interface in Narrative Arsenal. `USovExertionComponent` implements it. Existing `UNarrativeCombatAbility` owns the activation identity and exposes:

- `bRequiresChargedRelease`: opt-in admission before target-data dispatch.
- `MinimumStaminaChargeTier` and `ChargedReleaseStaminaCost`: editable tier/cost. Defaults 2 and 20 are prototype choices, not locked TDD decisions.
- `TryCommitChargedRelease(Tier)`: call after validating the actual charge tier, current node, geometry and attack state. Below the threshold it still records a zero-cost release. At or above the threshold the complete cost must be available. Replays reject.
- `TryCommitDefensiveCancel(Cost)`: call after the authored character/node/window and next-action admission checks. It pays once and closes all further target-data dispatch for that activation. End the old attack and enter the selected defensive ability.

A cost callback that cancels the attack or swaps its ASC/avatar prevents release continuation. An EndAbility deferred by a GAS scope lock immediately invalidates the attack receipt. Finalization copies the supplied target data and consumes the captured handle/key after callbacks, so a callback starting another activation cannot have its target data consumed accidentally.

These hooks extend the existing attack framework; they do not infer a charged tier from animation length, invent a free defensive cancel or automatically charge unrelated Echo techniques. Struggle/hazard actions can use the same `TrySpendExertion` contract after their native admission checks.

## Semantic input buffer

The existing `UNarrativeAbilitySystemComponent::AbilityInputTagPressed` pathway now admits a press to a registered native combat window. `RegisterCombatInputWindow(Ability, AllowedInputs, OpensAfter, ClosesAfter)` captures the exact active ability instance, spec, avatar, readiness epoch and attack GUID. Only one live node owns a scope; registration cannot silently replace it. The newest allowed semantic press is retained for 0.22 seconds plus the configured input-buffer assistance. Normal immediate input behavior remains outside the registered tag set.

`ConsumeCombatInputWindow` succeeds once, only between the authored opening and closing times. Assistance extends input freshness, not the cancel window or minimum commitment. A released tap remains a tap through the returned held-state flag. Existing input delegates still reach already-active abilities, preserving charge-release and hold behavior, while other inactive abilities are not activated by a press reserved for the owning node.

Native melee calls `BeginNextSovCombatAttack` only after a validated finite node transition. This renews the attack receipt and invalidates the old input scope without creating a second GAS activation. `CanDispatchNativeAttack` exposes charged/cancel/ending admission for direct native damage payloads as well as target-data dispatch. Defensive transition preflight uses `CanActivateAfterCombatCancel` on native Evade: it ignores exactly the cancelling attack's own GAS Busy contribution, checks all other required/blocked tags and cooldown, and validates capsule clearance plus the combined cancel and evade cost. It never removes a tag to simulate future state.

`ClearCombatInputBuffer` is the explicit modal/focus/load cleanup hook. Avatar/readiness changes and invalid/dead/cancelled owners invalidate the scope. There is no per-frame loop attempting to activate failed abilities; the owning melee node polls its authored window and consumes an existing fresh press.

Semantic input callbacks also capture the ASC activation serial, avatar and readiness epoch. If an input callback ends and reactivates the same spec, the old press/release event cannot be forwarded to its new activation, even when the server prediction key is unchanged.

## Validation and definition of done

Executed here: `Tests/Portable/ExertionPolicyTests.cpp` compiled and ran using C++17, `-Wall -Wextra -Werror -pedantic`, undefined-behavior sanitization and nonrecovering sanitizer failures. All 2,015 exact-payment, invalid-input, delay-boundary, frame-partition and exhaustion checks passed. `Tests/Portable/CombatInputPolicyTests.cpp` also passed 1,015 input freshness, window boundary, assistance and invalid-time checks under the same compiler/sanitizer flags.

Added, pending Unreal execution:

1. `ProjectVelkorran.Campaign.Exertion.RegenSprintAndCheckpoint`: real ASC attributes, external spend delay, idle/guarded rates, free traversal sprint, combat drain, final-fraction exhaustion, checkpoint and readable state.
2. `ProjectVelkorran.Campaign.Exertion.ChargedReleaseAndCancelTransactions`: missing payment, exact payment, threshold tiers, replay rejection, closed target dispatch after cancellation and synchronous cancel during payment.
3. `ProjectVelkorran.Campaign.Exertion.EvadeAdmissionAndOwnedCleanup`: no-resource/no-clearance rejection without payment, native CMC source, exact cost, offensive activation rejection, cinematic cancellation and preservation of unrelated immunity.
4. `ProjectVelkorran.Campaign.Exertion.SemanticInputBufferOwnership`: registered scopes, minimum commitment, tap versus hold, newest intent, exactly-once consumption, node identity renewal and modal/cancellation cleanup.
5. `ProjectVelkorran.Campaign.Exertion.StaleReleaseDoesNotReachNewActivation`: same-spec reactivation inside the real input-release callback preserves its new held state and receives no stale replicated release.

UE acceptance still requires the real engine build, both protagonists on representative moving/stationary obstacles at 30/60/120 fps, evades along walls and between enemies, held/toggled sprint input, external guard impacts during regen, death/respawn and checkpoint/cinematic interruption. Confirm that authored legacy regeneration and root-motion evade montages are not competing with the native owners. Passing the portable policy suite alone is not a claim that these engine/asset gates passed.
