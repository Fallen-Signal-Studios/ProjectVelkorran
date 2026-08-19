# Narrative foundation pass

This pass turns the isolated Narrative Pro copy into the first Sovereign Call combat foundation while preserving existing Narrative asset references.

## Source contract

- `UNarrativeDamageExecCalc` remains the execution class so the existing damage Gameplay Effect does not need a C++ class migration.
- `UNarrativeAttributeSetBase` owns the ordered authoritative transaction: guard, partial/full Shield bypass, Shield, Health, Poise, break, death, and typed result publication.
- `FSovGameplayTags` in `NarrativeArsenal` is the only C++ registrar for `Sov.*` contracts. This location avoids a circular dependency from the plugin back to the game module.
- `ANarrativePlayerCharacter` owns ASC initialization and publishes readiness once per `(ASC, definition)` epoch. A replicated ASC epoch fences PlayerState data against pawn-channel readiness, and the controller creates HUD and activates gameplay mapping contexts only after the local ready transition.
- `ASovPlayerCharacterBase` owns Echo, Shield, Poise, and Guard components and includes them in its readiness predicate.
- Definition tags, default abilities, and persistent definition effects are tracked with stable ASC-owned identities so a PlayerState-backed ASC cannot accumulate duplicates across respawns.
- Direct writes to the `Damage` meta attribute are outside the project contract. Damage must use the configured execution Gameplay Effect; Narrative's self-damage helpers tag already-resolved/fatal policy explicitly.
- Zero Health converges both Narrative's death tag and `Sov.State.Fatal`, cancels ordinary active abilities, and removes gameplay input until revive.
- Damage numbers are disabled by default per the TDD; the existing developer setting remains the accessibility/gameplay toggle.

## Required local content setup

1. Reparent the current `SovePlayerCharacterBase` Blueprint to `ASovPlayerCharacterBase`.
2. Remove the Blueprint-added Echo, Shield, and Poise components after reparenting. Their native counterparts, plus Guard, are inherited from C++.
3. Confirm the configured damage Gameplay Effect still uses `UNarrativeDamageExecCalc`. The fallback Narrative asset path remains in `DefaultEngine.ini`; a project-owned Gameplay Effect can replace it through `UArsenalSettings` without another source edit.
4. Create a Blueprint child of `USovGameplayAbility_TarrikGuard` for Tarrik's animation and presentation hooks.
5. Grant that ability through Velkorran's existing weapon/ability configuration. It uses the existing `Narrative.Input.AltAttack` slot.
   Remove or disable any other granted AltAttack ability for Tarrik; Narrative activates every spec matching a pressed input tag.
6. Tag incoming attack Gameplay Effects through **Asset Tags** (not granted target tags) with one guard class:
   - `Sov.Damage.GuardClass.Standard`
   - `Sov.Damage.GuardClass.Heavy`
   - `Sov.Damage.GuardClass.Unblockable`
7. Add one or more `Sov.Damage.Channel.*` tags and author SetByCaller values as needed. Existing `SetByCaller.Damage` remains the base damage input.
8. Tag the counterattack damage spec with `Sov.Damage.Source.GuardCounter`. Echo is awarded only when that tagged counter applies Shield or Health damage during an open counter window.
9. Bind guard component events to Tarrik's guard enter/loop/exit, impact, perfect-defense, break, and counter presentation.
   Guard result presentation is multicast by the component. Drive simulated-proxy guard enter/exit animation from the replicated guarding/perfect-defense state tags or Gameplay Cues.
   Add `Narrative.Anim.AnimSets.Flinch.Block` to the active weapon overlay's `Tagged Anim Sets`. The Guard component executes Narrative's `GameplayCue.TakeDamage.Blocked` for `Result.bGuarded`; that cue remains responsible for resolving the active linked layer's paired 3P/1P block montage.
   In Narrative's existing `OnDamagedBy` flinch graph, pass its raw Spec and `Sov.Damage.Result.Guarded` into `Gameplay Effect Spec Has Asset Tag`; skip the normal directional flinch when that returns true. This tag exists only on the callback copy for that resolved hit, so rear, unblockable, failed-heavy, and later hits remain unaffected.
10. Apply requested `Sov.Status.*` tags with project Gameplay Effects from the typed damage-result/event hook. This source pass publishes validated status requests but cannot author the binary effect assets.

## Damage authoring defaults

Omitted values remain backward compatible:

- ability scalar: `1.0`
- source modifier: `1.0`
- hit-zone modifier: physical-material multiplier, otherwise `1.0`
- difficulty scalar: `1.0`
- authored mitigation multiplier: `1.0`
- Shield coefficient: `1.0`
- Health coefficient: `1.0`
- Poise coefficient: `0.0`
- Shield bypass ratio: `0.0`, or the configured `0.5` fallback when partial bypass is tagged

`Sov.Damage.Policy.AlreadyResolved` and `Sov.Damage.Policy.Fatal` are reserved for trusted engine/project helpers. Shield bypass checks are exact-tag checks; do not use a hierarchical Blueprint query when distinguishing full bypass from `Sov.Damage.BypassShield.Partial`.

Mixed channel tags are supported for routing and immunity, but the current damage spec has one shared scalar rather than per-channel weights. A mixed hit is rejected only when every declared channel is immune. Add weighted channel magnitudes before authoring attacks that require split mitigation.

`DealDamage(float)` remains a legacy shield-first helper. Falls, hazards, vacuum, and crush damage must use authored Environmental-channel specs so bypass, coefficients, and shield-recharge policy remain explicit. `Sov.Damage.Policy.Fatal` is reserved for out-of-band instakill/checkpoint logic, never ordinary attack assets.

## Minimum validation matrix

- PlayerState and PlayerDefinition arriving in either order
- repeated replication callbacks and respawn with the same PlayerState
- no duplicate default tags, abilities, or persistent startup effects after respawn
- cinematic entry/exit before and after readiness
- HUD and mapping context created/applied exactly once
- front and rear hits for standard, heavy, and unblockable attacks
- perfect-defense boundary around `0.16` seconds
- guard Stamina equality, exhaustion, and break cleanup
- full and partial Shield bypass with coefficients below, equal to, and above `1.0`
- hits against an already-broken Shield restart its three-second delay
- Poise recovery/super-armor floors and one break transition
- perfect guard grants `12` Echo once
- a landed, tagged guard counter grants `10` Echo once and consumes its window
- one melee target receives at most one hit per attack sweep in Development and Shipping/Test configurations

Narrative's current save calls are treated as synchronous. If the project replaces them with async loading, call `NotifyInitialPlayerDataApplied` only from the completion callback; readiness intentionally stays false until then. Live replacement of a ready pawn's `PlayerDefinition` is rejected until an explicit ability/effect migration policy exists.

Actual status Gameplay Effects and any timed vulnerability caused by Shield break remain content integrations. The source layer emits ordered, typed request/break events for those assets.

The repository cannot validate binary Gameplay Effects, animation assets, input assets, or an Unreal build. Run UnrealHeaderTool, a Development Editor build, and multiplayer PIE locally after the content steps above. Narrative's pre-existing weapon-spread helper also still needs a synchronized per-shot random stream before competitive/networked firearms depend on deterministic spread.
