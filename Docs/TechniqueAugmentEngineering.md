# Ability augment selection and equipment refinements

TDD v2 sections 10.5 and 10.6 fit the existing Technique tree and owned-perk grant architecture. Previously every purchased native perk became active immediately; there was no distinct unlocked-versus-selected state for mutually alternative ability augments. This pass extends that implementation.

## Contracts

An ordinary `USovTechniquePerk` keeps its current deterministic persistent GameplayEffects and owned ability grants. An optional `AugmentedAbility` tag identifies one exact protagonist Echo ability. When populated, purchasing or upgrading that perk unlocks its rank without applying its effects until selected. Selectable augments cannot directly grant replacement active abilities. Their persistent GameplayEffects and semantic tags provide the behavior modifier; the core ability and its input remain granted by the existing weapon/protagonist definition.

`USovTechniqueComponent::SelectAugmentAtSafePoint` accepts an actual `ASovTechniqueSafePoint`, the exact core ability tag, and a purchased augment class. A null augment clears that slot. The same saved component ledger records one selected class per ability, alongside its existing protagonist identity, purchases and point history. Existing protagonist snapshot/restore consequently keeps the two protagonists' selections separate. Compatibility loading starts new optional selection data empty, preventing an older save from inheriting the live pawn's current selection.

Admission requires a ready living player, the same native controller and pawn, an idle campaign transition, ground movement, a real physical safe point and no combat, unresolved choice, dialogue, traversal or recovery. The uniquely granted core ability must exist and be inactive. The existing Echo weapon gate must accept its current weapon context; unavailable/returning/equipping equipment blocks selection. This means weapon-specific loadout changes require the matching wielded weapon at the safe point. The UI receives an actionable rejection reason.

Only the old and new purchased perk ranks are refreshed through Narrative's existing tracked grant helper. Each perk removes its own exact handles, and the parent grant map stays current for respec and load. Selecting another option spends no points and does not remove a core ability spec. Callback interruption rolls back the previous selection. If that rollback itself fails or avatar ownership changes, the affected grants are removed, the ledger rejects further progression and saves, and the failure explicitly requires checkpoint recovery. Unrelated mission, weapon or effect owners remain untouched.

`GetUnlockedAugments` and `GetSelectedAugment` expose the committed choices for Blueprint loadout/ability presentation. Mid-grant public queries and saves cannot observe the unpublished selection. `OnTechniquesChanged` publishes the complete existing component record before notifying UI listeners. Saved selections must reference a purchased option whose authored target matches that exact slot. Respec clears both purchases and selection, refunding only the existing earned point budget.

## Refinement authoring

Signature equipment refinements remain ordinary native Technique perks, with deterministic infinite effects, existing granted passive behavior and exact cleanup. No randomized affixes, crafting resources, new inventory or new shop is introduced. The authored perk effect/tag and existing ability Blueprint determine the specific handling, defense, interface or Echo conversion behavior. Examples in the TDD are design options rather than an additional mandatory active-ability roster; concrete branch content and tuning still require authoring against the approved campaign weapon roster.

Editor work consists of choosing exact ability tags and persistent modifiers on existing perk assets, authoring meaningful alternative behaviors in those assets/ability presentation, and connecting the safe-point selection UI to the new native API. Effect modifiers should be scoped to the intended ability using existing GAS tag requirements. Native catalog validation rejects invalid slot tags, cross-protagonist options, direct replacement grants and malformed grant policies.

## Validation

Added Unreal automation, not executed in this environment:

- `Techniques.AugmentSelectionOwnedGrantsAndRestore`: locked options, purchase without auto-equip, replacement without stacking, unchanged core spec/points, repeated save replay, unavailable equipment/readiness/combat rejection and independent ownership through respec.
- `Techniques.AugmentSavedSlotValidation`: a purchased option cannot be loaded under another ability slot.
- `Techniques.AugmentRollbackFailureRequiresRecovery`: real GameplayEffect callbacks interrupt selection and its rollback; the ledger fails closed, affected effects are removed, independent effects survive, feedback requests a checkpoint and invalid state cannot be saved.

The existing Technique tests now use the established ready player/controller fixture, so purchase and respec exercise the same stricter admission as selection. Independent source review covered saved-map validation, exact grant tracking, safe-point gates, rollback and protagonist initialization. The previously executed full portable runner passed 23 suites; these UObject/GAS changes require UE5.7/UHT compilation and `ProjectVelkorran.Campaign.Techniques` automation before engine acceptance can be claimed.
