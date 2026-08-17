# Narrative foundation pass

This pass turns the isolated Narrative Pro copy into the first Sovereign Call combat foundation while preserving existing Narrative asset references.

## Source contract

- `UNarrativeDamageExecCalc` remains the execution class so the existing damage Gameplay Effect does not need a C++ class migration.
- `UNarrativeAttributeSetBase` owns the ordered authoritative transaction: guard, partial/full Shield bypass, Shield, Health, Poise, break, death, and typed result publication.
- `FSovGameplayTags` in `NarrativeArsenal` is the only C++ registrar for `Sov.*` contracts. This location avoids a circular dependency from the plugin back to the game module.
- `ANarrativePlayerCharacter` owns ASC initialization and publishes readiness once per pawn generation. The controller creates HUD and activates gameplay mapping contexts only after readiness.
- `ASovPlayerCharacterBase` owns Echo, Shield, Poise, and Guard components and includes them in its readiness predicate.

## Required local content setup

1. Reparent the current `SovePlayerCharacterBase` Blueprint to `ASovPlayerCharacterBase`.
2. Remove the Blueprint-added Echo, Shield, and Poise components after reparenting. Their native counterparts, plus Guard, are inherited from C++.
3. Confirm the configured damage Gameplay Effect still uses `UNarrativeDamageExecCalc`. The fallback Narrative asset path remains in `DefaultEngine.ini`; a project-owned Gameplay Effect can replace it through `UArsenalSettings` without another source edit.
4. Create a Blueprint child of `USovGameplayAbility_TarrikGuard` for Tarrik's animation and presentation hooks.
5. Grant that ability through Velkorran's existing weapon/ability configuration. It uses the existing `Narrative.Input.AltAttack` slot.
6. Tag incoming attack specs with one guard class:
   - `Sov.Damage.GuardClass.Standard`
   - `Sov.Damage.GuardClass.Heavy`
   - `Sov.Damage.GuardClass.Unblockable`
7. Add one or more `Sov.Damage.Channel.*` tags and author SetByCaller values as needed. Existing `SetByCaller.Damage` remains the base damage input.
8. Tag the counterattack damage spec with `Sov.Damage.Source.GuardCounter`. Echo is awarded only when that tagged counter applies Shield or Health damage during an open counter window.
9. Bind guard component events to Tarrik's guard enter/loop/exit, impact, perfect-defense, break, and counter presentation.

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

## Minimum validation matrix

- PlayerState and PlayerDefinition arriving in either order
- repeated replication callbacks and respawn with the same PlayerState
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

The repository cannot validate binary Gameplay Effects, animation assets, input assets, or an Unreal build. Run UnrealHeaderTool, a Development Editor build, and multiplayer PIE locally after the content steps above.
