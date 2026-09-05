# Campaign initialization, handoff and travel

The native campaign path is opt-in through `ASovCampaignGameMode.InitialMission`. It pairs the concrete pawn class with its Player Definition, retains one protagonist snapshot per identity on the existing Narrative PlayerState, and defers CharacterReady until the matching data has been applied. It supports the authored standalone campaign. Multiplayer and seamless travel are not supported by this transition path.

## Content setup

1. Reparent the framework Blueprints to `ASovCampaignGameMode`, `ASovPlayerController` and `ASovPlayerState`. Preserve the controller's existing input and HUD assets. The PlayerState replaces Narrative's **existing** `SkillTreeComponent` subobject with `USovTechniqueComponent`; remove duplicate Blueprint components.
2. Create mission assets from `USovMantleMissionDefinition` and `USovOneDegreeMissionDefinition`. Assign their matching Tarrik/Selene pawn Blueprint, Player Definition, map, and a unique entry PlayerStart tag. Each Player Definition needs its full default attribute effect, appearance and ability configuration. Configure both protagonists' Technique branches on the inherited component.
3. Set each map's GameMode `InitialMission` to its mission asset. Disable seamless travel. Keep unconverted greyboxes on the existing definition flow until they are configured.
4. Bind existing Narrative quest, dialogue, encounter and cinematic events to `GetCampaignState()`. Complete authored beat IDs only when the underlying action actually succeeds. The native graph enforces prerequisites, knowledge and immutable facts; it does not infer that a cinematic, infiltration, shot or escape happened merely because an actor was spawned.
5. Record a cinematic as viewed only after full successful playback. A previously viewed noninteractive cinematic can request skip; the same beat state writes still commit. Co-action proof requirements cannot be bypassed through ordinary `CompleteBeat` calls.
6. For a different protagonist in already loaded content, call `HandoffToMission(Destination, SpawnTransform)`. For an authored successor map, call `TravelToMission(Destination)`. Neither is a player input action. Mandatory beats must be complete; active/restoring encounters and busy/cinematic player states reject transitions. Same-protagonist successor map travel is legal.
7. Bind `OnCampaignTransitionChanged` for fades and a checkpoint recovery option. `Failed` deliberately keeps input blocked on a half-restored pawn. Do not call `NotifyInitialPlayerDataApplied` from Blueprint on a campaign-managed pawn; the native controller owns that stage.

## Transaction and save ownership

Mission definitions and appearance assets are loaded before outgoing combat state is removed. A new destination pawn is spawned before destruction of the source. Source snapshot and controller record are retained first. A transition epoch is latched before save callbacks or actor BeginPlay can run, and ownership is rechecked after callback-producing operations.

The outgoing controller releases held input. Narrative cancels abilities, removes perk-owned grants, clears old ability specs, tracked attribute/startup effects, active combat effects and definition-owned tags. The old pawn is destroyed because its cached controller could otherwise rebind the shared PlayerState ASC. Cleanup stops if a callback changes possession or ASC ownership.

The new pawn initializes owner/avatar, definition, project components, attributes, startup effects, grants and visual readiness. It then restores its own inventory/wield, Technique and resource snapshot. A first visit initializes its own Technique ledger and **replaces** factions with its own definition defaults. Current resources clamp to current definition maxima; old maximum attributes are not copied to another protagonist. Controller quest records load after the matching pawn exists. Mission state commits before CharacterReady publishes and gameplay input is released.

A failed same-world destination attempts to recreate the retained origin and restore its snapshot/controller state. A failed recovery remains explicit; it never reports a successful handoff. An externally possessed replacement pawn is not destroyed by this recovery path. All-world midfight rollback is not promised.

Map travel writes an explicit Narrative player record to `SovCampaignTravel`, independent of the displayed character name. The destination reads it only with the native `SovCampaignTransition` URL option and verifies that the saved destination matches that map's `InitialMission`. This temporary transfer slot is not the player's manual-save slot. A failed write prevents the native travel request. The generic Narrative LevelTransition path also checks write success and clears queued travel on failure.

Normal full-save loading reads PlayerState actor bytes first and excludes its source ASC/skill component bytes. Per-protagonist snapshots restore the matching kit. Generic source `PawnData` is never deserialized into another protagonist. Legacy saves without the new matching snapshot are rejected by this managed path; use a new campaign save or an explicit migration. Later edited mission definitions that invalidate saved journal facts are likewise rejected.

Narrative's stable-actor lookup now rekeys deferred actors after saved GUID assignment. Destroying an old staged actor cannot evict a replacement registered under the same GUID.

## Validation

Run `Scripts/Validate-Unreal.ps1` for the actual UE5.7 build and automation, then `CompileAllBlueprints` and the content playthrough. `Docs/HandoffRuntimeValidation.md` describes the authored native tests and their limits.

The explicit mission preflight is:

```powershell
UnrealEditor-Cmd.exe ProjectVelkorran.uproject -run=SovValidateCampaign -Missions=/Game/Missions/DA_M01.DA_M01,/Game/Missions/DA_M02.DA_M02 -unattended -nop4
```

Supply the complete authored mission set. It rejects missing/duplicate primary IDs, invalid mission graphs, wrong protagonist classes, absent definitions/attributes/appearances/maps, missing entry tags, and successors outside the supplied manifest. It does not load map actors to prove that the tagged PlayerStart exists, compile Blueprint graphs, validate localization/dialogue/cook exclusions, or replace an end-to-end playthrough. Those are separate gates.

Required map tests: new and previously visited destination; exact equipment/ammo/Technique restoration; failure during each readiness callback; wrong definition/class; missing appearance/map/PlayerStart; origin recovery; empty Narrative quest restore; save before/after each canon gate; successful and failed disk writes; source and destination facing; repeated M01→M02 clean-boot/reload paths. No UE build, UHT, commandlet, disk-travel or content playthrough result is claimed from the source-only environment.
