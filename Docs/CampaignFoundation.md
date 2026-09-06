# Campaign foundation and protagonist split

This pass creates project-owned campaign framework seams without discarding Narrative's working save, definition, input, HUD, interaction, and ASC lifecycles.

## Native ownership

- `ASovCampaignGameMode` derives from `ANarrativeGameMode` and defaults to the project PlayerController, PlayerState, and Tarrik pawn classes while retaining `ANarrativeGameState` behavior.
- `ASovPlayerController` owns managed campaign initialization, authored protagonist handoff, guarded map travel and the campaign state component. Input/HUD assets remain authored; see [CampaignHandoff.md](CampaignHandoff.md).
- `ASovPlayerState` preserves Narrative's replicated Ability System Component, stores independent protagonist snapshots, and replaces the existing skill-tree subobject with the campaign Technique policy.
- `ASovPlayerCharacterBase` owns Echo, Exertion, Field Recovery, Resonance, fatal recovery, targeting, Shield, Health recharge, Poise, generic combat status, and mission-scoped corruption. Each component is constructed once.
- `ASovNPCCharacterBase` owns generic combat status and encounter save/restore identity; drones explicitly accept Device Disabled. NPCs do not construct player corruption.
- `ASovTarrikCharacter` additionally owns Guard and Cinderline Echo generation.
- `ASovSeleneCharacter` additionally owns the one-hit Deflection component and Selene's typed precision Echo generator. The generator rewards perfect Deflection, accepted hits on unbroken authored weak points, precision chains, verified undetected bypass, Axiom's first valid command-link Sever, and marked/exposed kills. Selene-authored Exposed status preserves its source provenance; a target carrying both that status and an authored mark earns one kill payoff.
- `USovGameplayAbility_SeleneAxiomNullPulse` owns native charge, directed Shield collapse, timed recharge/device suppression, and validated command-link Sever. Its Blueprint supplies presentation and the exact Axiom weapon grant/allowlist. The other four approved Selene Echo abilities now also have native payloads. Remove superseded Blueprint gameplay execution when adopting them, and preserve their presentation bindings.
- `ASovDominionHandler` is the first concrete Commander profile: it owns one command link and a server-authoritative ability that orders an exact linked Hound Horn Charge. Horn Charge requires both the active relationship and a transient native Handler order.

Each concrete protagonist supplies a canonical native identity tag. When its Player Definition is applied, the character retains all definition-owned tags, removes the opposite protagonist identity, and adds its own. This uses `SetDefinitionOwnedTags`, rather than an unrelated loose tag, because Narrative's ASC lives on PlayerState and may survive pawn replacement.

## Required Unreal content migration

The original source-only snapshot did not include the project's binary Content assets. These migration steps describe the required asset work; see the dated work-PC qualification below for the assets now authored and the remaining validation limits.

1. Before reparenting, record any Tarrik Blueprint overrides on the inherited Guard and Cinderline Echo generator, plus any Blueprint-authored Selene defense, parry, or weak-point reward logic. Native template ownership changes in this pass, so Unreal may not transfer every overridden value automatically.
2. Close the editor and perform a full Development Editor build. Do not use Hot Reload for the reparenting step.
3. Reparent the existing Tarrik player Blueprint from `ASovPlayerCharacterBase` to `ASovTarrikCharacter`.
4. Remove any Blueprint-added Echo, Shield, Health recharge, Poise, Guard, or Tarrik Echo generator that duplicates the inherited native component.
5. Reapply and verify the recorded Guard/generator tuning on Tarrik's new inherited component templates.
6. Create or reparent Selene's player Blueprint to `ASovSeleneCharacter`. Confirm it inherits Echo, Shield, Health recharge, Poise, Deflection, and Selene Echo generation exactly once, but no Guard or Tarrik Echo generator.
7. Remove any Blueprint-added Deflection or Selene Echo generator that duplicates the new inherited native components. Replace old Blueprint parry/reward logic with presentation bindings only after validating the native result.
8. Create a Gameplay Ability Blueprint derived from `USovGameplayAbility_SeleneDeflection` and grant it once through Selene's default Ability Configuration. See `Docs/SeleneCoreLoop.md` for input, target, and tuning setup.
9. Add one `USovWeakPointComponent` to each eligible enemy Blueprint and author stable zones only where the encounter truly exposes a breakable weak point. An empty component is a valid no-op.
10. Use `ASovDominionHandler` for the Dominion Handler Blueprint and keep its inherited `USovCommandLinkComponent`; add the component explicitly only to other authored command nodes. Keep **Include Owner As Participant** enabled for the Handler so its ASC receives the active-link state, assign linked actors, and configure their red weak-point reveal presentation. Use the native Axiom pulse by granting `USovGameplayAbility_SeleneAxiomNullPulse` from Axiom's weapon item with its exact allowlist. Remove old Blueprint pulse/status/Sever gameplay; native charge/release owns it and direct helper calls are invalid. See [AxiomNullPulse.md](AxiomNullPulse.md) and [SeleneCommandLinkAndWeakPointReveal.md](SeleneCommandLinkAndWeakPointReveal.md).
11. Create and grant the Handler command ability, reserve Hound Ability1 for native Handler dispatch, and complete the encounter/AI setup in `Docs/DominionHandlerProfile.md`.
12. Review the inherited `USovStatusComponent` and mission-scoped `USovCorruptionComponent`. Author campaign exposure with validated `USovCorruptionProfile` assets and native producers/remedies from [CorruptionEngineering.md](CorruptionEngineering.md). Generic status definitions remain documented in [StatusAndCorruptionPrototype.md](StatusAndCorruptionPrototype.md). The optional `USovLegacyCorruptionComponent` and old field/remedy volumes are for standalone prototype maps; remove the legacy component before managed campaign initialization.
13. Keep `Sov.Character.Player.Tarrik` on Tarrik's Player Definition and `Sov.Character.Player.Selene` on Selene's. The tag picker permits the `Sov.Character` category.
14. Compile and save the player, ability, command-node, material, weak-point target, status-definition, and corruption-volume assets, then run `CompileAllBlueprints` before testing gameplay.

## Framework Blueprint migration

The framework Blueprints own Player Definitions, pawn selection, input mappings, HUD classes, and menu behavior. Keep these values on project-owned copies. Duplicating the controller and reparenting it to the native Sovereign class also requires remapping the UI's hard casts: the copy no longer derives from the original Narrative controller Blueprint.

Apply this order in the full UE 5.7 project with its existing content and licensed Narrative plugin baseline. Enable PythonScriptPlugin and EditorScriptingUtilities. Run files through Unreal's **Execute Python Script** command or `runpy.run_path` in the editor's Python console; the scripts use `__file__` to locate adjacent helpers. Except for the explicit live HUD probe, stop PIE before authoring. Use the same `VELKORRAN_SETUP_OUTPUT` value for each related probe/setup pair if overriding their default `Saved/Validation/WorkPCSetup` output directory.

1. Close the editor and build the Development Editor target, including `ProjectVelkorranEditor`. Restart the editor; do not use Hot Reload for class reparenting.
2. Run `Scripts/Editor/Setup-WorkPCFramework.py` to create `/Game/Framework` copies with `ASovCampaignGameMode`, `ASovPlayerController`, and `ASovPlayerState` parents, project player definitions, and development maps. Preserve the source GameMode's definitions/pawn values and controller's look input, mappings, camera manager, and HUD configuration.
3. Run `Scripts/Editor/verify_protagonist_setup_source_templates.py`, which applies the native-kit and projectile-presentation scripts between source-template preservation checks. The kit assigns the copied controller's `/Game/Input/IMC_Combat` and `DA_CombatInputs`; run it after framework setup so a framework rerun does not restore stock input defaults.
4. Run `Scripts/Editor/setup_project_controller_ui.py` in the interactive editor. It copies the UI dependency closure into `/Game/UI/Narrative`, remaps the old controller and UI Blueprint/class/CDO/function references only in explicit project packages, and recompiles parents and contained widget classes before their dependents. The standard engine widget-replacement operation may present confirmation dialogs; complete those dialogs and inspect the resulting report. The editor helper never saves assets itself. The script saves project copies only after compilation and source-property checks succeed, and records source package hashes, persistent UObject property fingerprints, and dirty flags under `Saved/Validation/EditorUIRemap`. Never save source plugin assets to repair these casts.
5. With PIE stopped, run `Scripts/Editor/setup_project_failure_menu_footer.py`, then `Scripts/Editor/verify_controller_ui_remap_readonly.py`. This creates the project failure-menu copy, hides only its developer footer, and connects `BP_SovPlayerController.DeathMenuClass`; the header/message, Respawn/Quit buttons, events and fade animation are preserved. Both scripts use `Saved/Validation/WorkPCSetup` or the same `VELKORRAN_SETUP_OUTPUT` override. Require successful source preservation, zero compile errors, matching controller defaults, copied Blueprint parents/graph references, and the supplemental failure-menu pair before bundling or cooking. Replay after recreating controller defaults. The verifier reads the latest local authoring reports, so run authoring first when reconstructing from source. See [WorkPCFailureMenuPolish-2026-09-06.md](WorkPCFailureMenuPolish-2026-09-06.md).
6. Run `Scripts/Editor/probe_weapon_display_metadata_readonly.py`, then `Scripts/Editor/setup_project_weapon_display.py`. These configure canonical weapon labels and fill confirmed missing icons using existing prototype textures. Run `Scripts/Editor/probe_weapon_presentation_health_readonly.py`, then `Scripts/Editor/setup_project_weapon_presentation_repairs.py` to reproduce the scoped Staccato holster-scale and copied Verity overlay/visual repairs.
7. Open either development combat map, enter PIE, wield a weapon, and run `Scripts/Editor/probe_combat_hud_layout_readonly.py`. End PIE, then run `Scripts/Editor/setup_combat_hud_layout.py` against that observed layout. It preserves the weapon/ammo bindings and crosshair while arranging the copied HUD around the native five-resource panel. Do not substitute a stale probe from a different baseline.
8. Run `Scripts/Editor/setup_combat_map_entry.py` with PIE stopped. It saves only the two development maps' PlayerStart facing changes toward their existing Dominion pack and verifies the source `CombatGreybox` remains unchanged. The kit already orders Verity before Staccato for first-free-slot assignment; `Scripts/Editor/setup_project_loadout_order.py` is an optional repair for older generated loadouts, documented in [WorkPCProtagonistKit-2026-09-06.md](WorkPCProtagonistKit-2026-09-06.md).
9. Save the project copies and restart the editor for a fresh-load check. Verify both protagonists reach readiness using the project GameMode. Check the copied controller's definitions, mapping context, ability-input mapping, camera manager, HUD, and remapped menu classes. In actual PIE, hold the weapon-wheel input, use relative mouse motion to change the selected sector, and release to equip; then open inventory and return to play without invalid controller casts or missing widget references. Check firing/reload, melee presentation, holster/wield transitions, and both protagonists' Echo inputs. A visible empty wheel is not acceptance. Source-compatible input rows alone do not prove Blueprint UI compatibility.
10. Point any additional intended project or campaign map defaults at the new GameMode only after these checks. Inspect each map's World Settings for a map-specific override. After a framework or UI rerun, repeat the downstream kit, UI, metadata, presentation, HUD and entry steps as applicable before qualifying gameplay again; the final authored overlay preserves the already-verified asset state.

The complete kit contracts, preservation guarantees, controls and targeted presentation changes are described in [WorkPCProtagonistKit-2026-09-06.md](WorkPCProtagonistKit-2026-09-06.md). These scripts do not reconstruct the licensed source assets or certify M12/M13 campaign progression.

Do not switch `GameInstanceClass` yet. Narrative's native GameInstance is empty, but the configured Blueprint may contain behavior that is not represented in this source snapshot.

## Current handoff boundary

The native managed path now handles per-protagonist snapshots, source ability/effect teardown, faction and Technique isolation, destination readiness, origin recovery, and explicit-slot non-seamless travel. Enable it by assigning `InitialMission` on each campaign map's project GameMode. See [CampaignHandoff.md](CampaignHandoff.md) and [EncounterRecovery.md](EncounterRecovery.md).

Ordinary re-possession still bypasses these campaign guarantees. The managed campaign transition is standalone only; network combat component tests do not certify multiplayer campaign travel. The earlier source-only audit had no Unreal build or map/playthrough result; the dated work-PC checks below supersede that environment limitation without claiming a campaign playthrough.

## Work-PC qualification — September 6, 2026

The full work-PC repository was advanced by 37 origin commits to `0bbd7c8`, with the prior work preserved on a branch and in a named stash. Unreal Engine 5.7.4 Development Editor and non-editor Win64 Development targets now build and link. Missing bundled AutomationTool script assemblies were rebuilt, and Win64 Turnkey SDK verification succeeds.

Project copies under `/Game/Framework`, `/Game/Input`, `/Game/Items/Weapons`, `/Game/Abilities`, and `/Game/UI/Narrative` provide both native protagonist kits and the repaired controller/menu dependency graph. `/Game/Maps/Development/L_TarrikCombat` and `/Game/Maps/Development/L_SeleneCombat` run their respective protagonists and face the existing Dominion pack at entry; the source `CombatGreybox` remains unchanged. The HUD presents the five canonical resources. Project-only weapon presentation changes repair Staccato hand/holster placement and Verity's animation Blueprint mismatch.

Rendered PIE observed expected input-triggered Echo costs for all ten native protagonist abilities, using an explicit test Echo reset immediately before each input. Fresh primary-input checks consumed Cinderline/Axiom/Staccato ammunition and observed Velkorran/Verity attack montages; aimed Cinderline fire killed a hostile hound through ordinary input. Both protagonists have been attacked and killed by the ordinary encounter, and the visible Respawn button restores play. These results do not certify every payload hit or animation transition. The copied weapon wheel opens, populates and closes; its sector selection uses relative mouse movement while held. Eleven controller defaults and 25 Blueprint parents/graphs passed remap checks, and checked source-template memory fingerprints and file hashes were preserved. The project failure screen retains its playable buttons and hides only the developer footer.

The final combined native run passes 414/414 tests: 389 clean passes and 25 passes with warnings, with zero failures or missing tests. This includes the corrected Dialogue LocalPlayer fixture and a regression for the event-driven perception component used by the actual enemies. The 27-test save family passes. Ordinary MSVC portable policy tests pass 42/42; Python source/tool tests report 67 passes and 11 skips. No official UBSan pass is claimed because the installed Windows sanitizer runtime could not link.

The selected-map Win64 Development cook/archive succeeds with zero errors and 944 retained warnings. The reproducible [combat playtest cook](Cook-CombatPlaytest.md) excludes two confirmed MetaHuman authoring-data directories and explicitly includes the dynamically loaded StableRods hair solver. Both archived combat maps load their intended GameModes, run for 20 seconds with isolated user data, and exit normally with no missing-object/package or Error/Fatal/Assert/Ensure diagnostics. Initial-tick HUD/LoadingMenu `Accessed None` warnings remain. The archive is `Saved/WorkPCPlaytest/Windows`, with `Play-Tarrik.cmd` and `Play-Selene.cmd` launchers. Rendered combat was checked in Unreal Editor; standalone visual inspection was blocked by a desktop-tool app-approval timeout and is not claimed.

In a 24-second rendered Selene check after ordinary Respawn, mapped Verity defensive input opened the native window, an actual hound hit produced perfect Deflection, and the typed `PERFECT_DEFLECTION` event awarded exactly +10 Echo (25 to 35). The check did not reset resources, force damage/targets, or invoke ability helpers. This qualifies that live defensive/reward loop alongside the isolated native tests.

The original August TDD was not located; available repository design documents and current source history informed this work. M12/M13 mission progression, full campaign travel/playthrough, multiplayer, consoles, and Shipping builds remain unqualified. These two development maps are not substitutes for an authored campaign route.

One Selene editor run left enemies idle with valid sight/hostility but empty stock attack goals. Subsequent fresh starts and respawns engaged normally, including the qualified perfect-Deflection check. Source inspection identified a possible gap when sight precedes asynchronous player faction initialization; the failed run did not capture the startup event ordering, so causation remains unproven. No speculative faction/cache change was made. Restart Play if encountered and capture the initial event timeline during further cold-start testing.

Binary `Content` remains ignored by Git. Reproducing the authored state requires the matching source commit plus the dated combat-asset ZIP/manifest over the full existing project and licensed plugin baseline. The overlay contains project asset changes, not a complete project. Re-run authoring only in the documented framework → kit → UI order, with PIE stopped and source-preservation checks enabled.

## Verification

Run:

1. `ProjectVelkorranEditor Win64 Development`
2. `Automation RunTests ProjectVelkorran.Campaign.Foundation`
3. `Automation RunTests ProjectVelkorran.Campaign.Selene`
4. `Automation RunTests ProjectVelkorran.Campaign.AxiomNullPulse`
5. `Automation RunTests ProjectVelkorran.Campaign.Status`
6. `Automation RunTests ProjectVelkorran.Campaign.Corruption`
7. `Automation RunTests ProjectVelkorran.Campaign.Echo`
8. `CompileAllBlueprints`
9. Standalone PIE with Tarrik
10. Standalone PIE with Selene
11. Two-player listen-server PIE and, when available, dedicated-server PIE using the matrix in `Docs/SeleneCoreLoop.md`

Verify that Tarrik reaches readiness with one Guard and one Cinderline generator and no Selene systems. Verify that Selene reaches readiness with one Deflection component and one Selene Echo generator, no Tarrik systems, and all shared resource components exactly once. A valid perfect Deflection must award `+10` Echo once; an accepted hit on an unbroken authored hostile weak point must award `+8` once per native hit transaction; and Axiom's first valid Sever of an active hostile link instance must award `+12` once. Ordinary body hits, repeated hits on the same broken zone, Shield break or Device Disabled without a live link, friendly targets, Tarrik, and replayed Sever transactions must not grant those rewards.

A Selene-authored exposure kill grants `+6` once, including when the target also has an authored mark/exposure window. Native Selene control and native Tarrik payload-owned Burn use `Sov.Status.Application.NativeOwned` so the generic status listener cannot duplicate their effects. See [MergeResolution-2026-09-05.md](MergeResolution-2026-09-05.md) for the integration decisions and validation limits.
