# Campaign foundation and protagonist split

This pass creates project-owned campaign framework seams without discarding Narrative's working save, definition, input, HUD, interaction, and ASC lifecycles.

## Native ownership

- `ASovCampaignGameMode` derives from `ANarrativeGameMode` and defaults to the project PlayerController, PlayerState, and Tarrik pawn classes while retaining `ANarrativeGameState` behavior.
- `ASovPlayerController` is the future owner of campaign possession, authored protagonist handoffs, input profiles, and HUD coordination.
- `ASovPlayerState` preserves Narrative's replicated Ability System Component and provides the future campaign-state seam.
- `ASovPlayerCharacterBase` owns Echo, Shield, player Health recharge, and Poise.
- `ASovTarrikCharacter` additionally owns Guard and Cinderline Echo generation.
- `ASovSeleneCharacter` intentionally has no placeholder Selene-only component. Deflection and precision/disruption Echo generation should be added when their real gameplay events exist.

Each concrete protagonist supplies a canonical native identity tag. When its Player Definition is applied, the character retains all definition-owned tags, removes the opposite protagonist identity, and adds its own. This uses `SetDefinitionOwnedTags`, rather than an unrelated loose tag, because Narrative's ASC lives on PlayerState and may survive pawn replacement.

## Required Unreal content migration

The repository snapshot does not include the project's binary Content assets. Complete these steps in the Unreal Editor after merging the source pass:

1. Before reparenting, record any Tarrik Blueprint overrides on the inherited Guard and Cinderline Echo generator. Their native template owner changes in this pass, so Unreal may not transfer every overridden value automatically.
2. Close the editor and perform a full Development Editor build. Do not use Hot Reload for the reparenting step.
3. Reparent the existing Tarrik player Blueprint from `ASovPlayerCharacterBase` to `ASovTarrikCharacter`.
4. Remove any Blueprint-added Echo, Shield, Health recharge, Poise, Guard, or Tarrik Echo generator that duplicates the inherited native component.
5. Reapply and verify the recorded Guard/generator tuning on Tarrik's new inherited component templates.
6. Create or reparent Selene's player Blueprint to `ASovSeleneCharacter`. Confirm it has Echo, Shield, Health recharge, and Poise, but no Guard or Tarrik Echo generator.
7. Keep `Sov.Character.Player.Tarrik` on Tarrik's Player Definition and `Sov.Character.Player.Selene` on Selene's. The tag picker now permits the `Sov.Character` category.
8. Compile and save both player Blueprints, then run `CompileAllBlueprints` before testing gameplay.

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

This pass establishes class ownership but does not implement Tarrik/Selene handoff on one PlayerState. Destroying the old pawn is not sufficient: Narrative keeps the ASC on PlayerState, and the prior protagonist's granted ability specs and persistent effects can survive the avatar change. Until an explicit ability/effect migration and component-detach policy is implemented, test each protagonist in a separate play session or with a newly created PlayerState. Do not ship an authored protagonist switch through ordinary re-possession.

## Verification

Run:

1. `ProjectVelkorranEditor Win64 Development`
2. `Automation RunTests ProjectVelkorran.Campaign.Foundation`
3. `CompileAllBlueprints`
4. Standalone PIE with Tarrik
5. Standalone PIE with Selene

Verify that Tarrik reaches readiness with one Guard and one Cinderline generator, Selene reaches readiness with neither, and both retain the shared resource components exactly once.
