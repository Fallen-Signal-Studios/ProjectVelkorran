# Campaign foundation and protagonist split

This pass creates project-owned campaign framework seams without discarding Narrative's working save, definition, input, HUD, interaction, and ASC lifecycles.

## Native ownership

- `ASovCampaignGameMode` derives from `ANarrativeGameMode` and defaults to the project PlayerController, PlayerState, and Tarrik pawn classes while retaining `ANarrativeGameState` behavior.
- `ASovPlayerController` is the future owner of campaign possession, authored protagonist handoffs, input profiles, and HUD coordination.
- `ASovPlayerState` preserves Narrative's replicated Ability System Component, owns the persistent corruption AttributeSet, and provides the future campaign-state seam.
- `ASovPlayerCharacterBase` owns Echo, Shield, player Health recharge, Poise, centralized combat status, and the player-only corruption lifecycle.
- `ASovNPCCharacterBase` owns centralized combat status but not player corruption. Mechanical drones explicitly accept Device Disabled.
- `ASovTarrikCharacter` additionally owns Guard and Cinderline Echo generation.
- `ASovSeleneCharacter` additionally owns the one-hit Deflection component and Selene's typed precision Echo generator. The implemented generator rewards perfect Deflection, the first break of an authored weak point, Axiom's first valid Sever of an active hostile command link, and a hostile defeat during Selene's Exposed window.
- `ASovDominionHandler` is the first concrete Commander profile: it owns one command link and a server-authoritative ability that orders an exact linked Hound Horn Charge. Horn Charge requires both the active relationship and a transient native Handler order.

Each concrete protagonist supplies a canonical native identity tag. When its Player Definition is applied, the character retains all definition-owned tags, removes the opposite protagonist identity, and adds its own. This uses `SetDefinitionOwnedTags`, rather than an unrelated loose tag, because Narrative's ASC lives on PlayerState and may survive pawn replacement.

## Required Unreal content migration

The repository snapshot does not include the project's binary Content assets. Complete these steps in the Unreal Editor after merging the source pass:

1. Before reparenting, record any Tarrik Blueprint overrides on the inherited Guard and Cinderline Echo generator, plus any Blueprint-authored Selene defense, parry, or weak-point reward logic. Native template ownership changes in this pass, so Unreal may not transfer every overridden value automatically.
2. Close the editor and perform a full Development Editor build. Do not use Hot Reload for the reparenting step.
3. Reparent the existing Tarrik player Blueprint from `ASovPlayerCharacterBase` to `ASovTarrikCharacter`.
4. Remove any Blueprint-added Echo, Shield, Health recharge, Poise, Guard, or Tarrik Echo generator that duplicates the inherited native component.
5. Reapply and verify the recorded Guard/generator tuning on Tarrik's new inherited component templates.
6. Create or reparent Selene's player Blueprint to `ASovSeleneCharacter`. Confirm it inherits Echo, Shield, Health recharge, Poise, Deflection, and Selene Echo generation exactly once, but no Guard or Tarrik Echo generator.
7. Remove any Blueprint-added Deflection or Selene Echo generator that duplicates the new inherited native components. Replace old Blueprint parry/reward logic with presentation bindings only after validating the native result.
8. Create a Gameplay Ability Blueprint derived from `USovGameplayAbility_SeleneDeflection` and grant it once through Selene's default Ability Configuration. See `Docs/SeleneCoreLoop.md` for input, target, and tuning setup.
9. Add one `USovWeakPointComponent` to each eligible enemy Blueprint and author stable zones only where the encounter truly exposes a breakable weak point. An empty component is a valid no-op.
10. Use `ASovDominionHandler` for the Dominion Handler Blueprint and keep its inherited `USovCommandLinkComponent`; add the component explicitly only to other authored command nodes. Keep **Include Owner As Participant** enabled for the Handler so its ASC receives the active-link state, assign linked actors, and configure their red weak-point reveal presentation. Wire `Try Sever Axiom Command Link` into Axiom's authority-owned pulse flow. See `Docs/SeleneCommandLinkAndWeakPointReveal.md`.
11. Create and grant the Handler command ability, reserve Hound Ability1 for native Handler dispatch, and complete the encounter/AI setup in `Docs/DominionHandlerProfile.md`.
12. Review inherited `USovStatusComponent` and `USovCorruptionComponent` templates, author any status-definition overrides, and place one uniquely identified corruption field plus its communicated remedy. Follow `Docs/StatusAndCorruptionPrototype.md`.
13. Keep `Sov.Character.Player.Tarrik` on Tarrik's Player Definition and `Sov.Character.Player.Selene` on Selene's. The tag picker permits the `Sov.Character` category.
14. Compile and save the player, ability, command-node, material, weak-point target, status-definition, and corruption-volume assets, then run `CompileAllBlueprints` before testing gameplay.

## Framework Blueprint migration

`DefaultEngine.ini` intentionally still references the existing Narrative framework Blueprints. They may own asset values unavailable in source control, including Player Definitions, pawn selection, input mapping, look input, HUD class, and menu behavior.

In the editor:

1. Duplicate the active Narrative GameMode, PlayerController, and PlayerState Blueprints into `/Game/Framework`.
2. Reparent those project copies to `ASovCampaignGameMode`, `ASovPlayerController`, and `ASovPlayerState`.
3. Preserve and verify the GameMode's Player Definitions and pawn configuration.
4. Preserve and verify the controller's mapping context, look action, ability input mapping, and gameplay HUD class.
5. Point the project or greybox map at the new project GameMode only after both protagonists spawn and reach Narrative's readiness gate.
6. Check `CombatGreybox` World Settings for a map-specific GameMode override before changing the global config.

Do not switch `GameInstanceClass` yet. Narrative's native GameInstance is empty, but the configured Blueprint may contain behavior that is not represented in this source snapshot.

## Current handoff boundary

This pass establishes class ownership and Selene's first real character-method loop, but does not implement Tarrik/Selene handoff on one PlayerState. Destroying the old pawn is not sufficient: Narrative keeps the ASC on PlayerState, and the prior protagonist's granted ability specs, loose tags, and persistent effects can survive the avatar change. Until an explicit ability/effect migration and component-detach policy is implemented, test each protagonist in a separate play session or with a newly created PlayerState. Do not ship an authored protagonist switch through ordinary re-possession.

## Verification

Run:

1. `ProjectVelkorranEditor Win64 Development`
2. `Automation RunTests ProjectVelkorran.Campaign.Foundation`
3. `Automation RunTests ProjectVelkorran.Campaign.Selene`
4. `Automation RunTests ProjectVelkorran.Campaign.Status`
5. `Automation RunTests ProjectVelkorran.Campaign.Corruption`
6. `CompileAllBlueprints`
7. Standalone PIE with Tarrik
8. Standalone PIE with Selene
9. Two-player listen-server PIE and, when available, dedicated-server PIE using the matrices in `Docs/SeleneCoreLoop.md` and `Docs/StatusAndCorruptionPrototype.md`

Verify that Tarrik reaches readiness with one Guard and one Cinderline generator and no Selene systems. Verify that Selene reaches readiness with one Deflection component and one Selene Echo generator, no Tarrik systems, and all shared resource components exactly once. Both must own exactly one status/corruption component, while NPC bases own status only. A valid perfect Deflection must award `+10` Echo once and expose its logical hostile attacker; a Selene-authored exposure kill must award `+6` once; the first valid break of an authored hostile weak point must award `+8` once; and Axiom's first valid Sever of an active hostile link instance must award `+12` once. Ordinary body hits, repeated hits on the same broken zone, Shield break or Device Disabled without a live link, friendly targets, Tarrik, and replayed damage/Sever transactions must not grant those rewards.
