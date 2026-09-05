# Campaign, persistence, sustain, and presentation audit

Read-only audit of the current materialized source snapshot. Paths below are relative to `ProjectVelkorran/`; `NA` means `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/`, and `NS` means the sibling `NarrativeSaveSystem/` module. TDD authority is v2 dated 14 August 2026, not the December 2025 attachment. Existing explicit creator-approved changes for player Health recharge, sustain drops and magazine/reserve pacing supersede older baseline wording.

## Actual playable-state boundary

This is a substantial combat framework with content-dependent player and enemy implementations, not source evidence of a complete two-mission campaign. `Config/DefaultEngine.ini` still boots Narrative's MainMenuMap and BP_NarrativeGameMode, with `/Game/Maps/CombatGreybox` as editor startup map. `ASovCampaignGameMode`, `ASovPlayerController`, and `ASovPlayerState` are valid project extension seams, but have no mission state or handoff implementation. The project Content directory is excluded from source control. Therefore no claim can be made that M01/M02 maps, quests, configured weapon definitions, cinematics, or encounter setups are absent from Matthew's local project; their integration simply cannot be inspected or validated here.

## Issues and decisions

### C1. Authored protagonist handoff and separate persistent state: missing native implementation, high campaign risk

1. **Exists:** `ASovTarrikCharacter` and `ASovSeleneCharacter` own distinct native components and canonical identity tags; Narrative's player ASC lives on PlayerState, readiness has an epoch and idempotent default grants, and GameMode can select a pawn class and PlayerDefinition. `Docs/CampaignFoundation.md` explicitly calls handoff a future boundary and forbids treating ordinary re-possession as a completed authored switch.
2. **Requires:** TDD §§3.3–3.4, 15.4: M01 is Tarrik, M02 is Selene; authored changes retain individual equipment/progression, correct protagonist/faction tags and ability sets, coherent save data, and input/HUD readiness. No at-will campaign switching.
3. **Gap:** `ASovPlayerController` and `ASovPlayerState` constructors only call Super. `ANarrativeGameMode::SpawnDefaultPawnAtTransform_Implementation` independently chooses `GetDefaultPawnClassForController()` and `GetPlayerDefinitionForController()`. The latter indexes PlayerDefinitions by joining player number, not mission or protagonist. `ANarrativePlayerCharacter::SetPlayerDefinition` rejects a live different definition after initialized ability publication. `UNarrativeSave` holds only one `FNarrativeSavePlayer`; `LoadPlayerData()` deserializes it into the currently spawned pawn/PlayerState/controller without a protagonist key. A pawn class change alone can reuse prior abilities, effects, resources, inventory, or quest context; the native canonical tag correction does not migrate loadout or player progression.
4. **Decision:** Extend project controller/PlayerState and a versioned Narrative save subclass; preserve Narrative ASC, quest, inventory and actor record infrastructure. Add explicit snapshot/restore and grant-detach policy, not a second ASC or independent save engine.
5. **Dependencies:** Approved M01→M02 boundary, both player definition assets, validated identity-specific default grants and inventory, checkpoint rollback, destination readiness and cinematic completion/skip contract.
6. **Risk:** High. Touches all weapons, active abilities and save compatibility; needs rollback on failed spawn/load.
7. **Order:** After combat payload/enemy execution fixes, before calling M01→M02 playable end to end.

### C2. Campaign resource save defaults are not established in native code

1. **Exists:** Narrative saves actor/component `SaveGame` data; `UNarrativeAbilitySystemComponent::PrepareForSave_Implementation` stores selected scalar attributes and `Load_Implementation` restores them. Health/Shield/Stamina/Poise/Echo and maxima carry `NarrativeSaveAttribute` metadata. `USovEchoComponent::RestoreEchoFromCheckpoint` is available.
2. **Requires:** TDD §§11.8, 15.9: deterministic restore of Health, Shield, Stamina, Echo reserve, equipment and progression at declared checkpoint policy.
3. **Gap:** Both ASC save methods only process `AttributesToSave`; the list has no native population anywhere in the tracked sources. Metadata does not automatically populate it. `ASovPlayerState` does not configure it. A Blueprint may configure its ASC, but cannot be inspected. `RestoreEchoFromCheckpoint` has no native caller outside its definition. Thus merely adding reflected resource attributes is not evidence they survive a checkpoint.
4. **Decision:** Extend project PlayerState resource-save defaults using `AddUnique` to preserve authored additions; use Narrative save hooks and apply a documented maxima/current restore order. Audit Blueprint overrides and preserve any required existing fields. Add actual capture/mutate/restore tests, not only CDO assertions.
5. **Dependencies:** C1 protagonist identity/save partition; resource policy (exact snapshot vs authored reserve), ASC initialization readiness, inventory restore.
6. **Risk:** Medium to high. Reapplying current before max can clamp values; wrong resource selection can carry one protagonist's state into the other.
7. **Order:** First checkpoint slice, alongside or immediately before authored handoff.

### C3. Narrative save mechanism is reusable, but campaign checkpoint contract is incomplete

1. **Exists:** `NS/Public/NarrativeSave.h`: stable GUID actor records, class soft references, component byte data, version enum; subsystem supports actor lookup, save, load and player-only travel saves. `UTalesComponent` serializes quest classes, state IDs, branch progress and quest byte data. `NarrativeInventoryComponent` persists items; player save restore intentionally loads pawn, then PlayerState, then controller quests.
2. **Requires:** TDD §§9.11, 11.7–11.9, 15.9: validated mission transitions, canon/consequence records, checkpoint retry, rolling autosaves, safe save gates, destination validation, rollback, version migrations and last-known-good fallback.
3. **Gap:** No project mission/consequence/checkpoint types or integration in tracked runtime beyond empty framework seams. `UNarrativeSave::SavedLevels` is commented out; current RecordMap is not an explicit authored per-mission checkpoint policy. `Save()` writes directly using `SaveGameToSlot`, without temporary replacement/rolling backup in this layer. `ANarrativeGameMode::ProcessServerTravel` triggers `CreatePlayerOnlySave` for a LevelTransition option but ignores its success, then travels. `CreatePlayerOnlySave` reads `PS->GetPlayerName()` before testing PS for null and uses player display name for slot identity. No native campaign schema/migrations beyond initial Narrative enum. These are source deficiencies, while an authored Blueprint may contain higher-level save gates that were not supplied.
4. **Decision:** Preserve Narrative's savable interfaces, GUID registry, quest serializer, inventory serialization and world lookup. Extend with project mission transaction/checkpoint policy and a campaign save subclass. Repair failure/null handling in existing travel path where appropriate; do not replace the whole subsystem.
5. **Dependencies:** C1/C2, mission packages, checkpointable enemy reset policy, immutable canon gate identifiers, stream destination checks.
6. **Risk:** High for format/restore changes; lower for defensive null and failure propagation. Golden-save and interrupted-write tests needed.
7. **Order:** Before mission integration can claim retry or cross-level continuity.

### C4. M01/M02 dependencies and cinematic canon gates remain unproven

1. **Exists:** Narrative's quest graphs/state/tasks, interaction component, spawners, player-ready input/HUD gate and `ANarrativeLevelSequenceActor` with participant binding, owned controller notification, sequence playback and blend-out. `ANarrativePlayerController::LevelSequencePlayed` exits dialogue and tracks active sequences. These are suitable building blocks.
2. **Requires:** M01 The Mantle: tutorial/exploration, heir declaration, Selene's shot wounds Caelus, response/pursuit, fixed refusal of unsafe market shot, Crownmark lead. M02 One Degree: Selene infiltration/precision-shot tutorial, close-quarters escape, Lyric extraction, Voss's Caerion II/Crownmark orders. TDD §§9.11–9.14, 14.13–14.14 require validated state and identical essential writes on normal playback, skip and recovery.
3. **Gap:** No native M01/M02 mission identifiers, canon gate state or authored mission manifest were found; map/quest/sequence assets are unavailable. Narrative sequence play/stop/finish callbacks are not themselves an idempotent canon-write transaction. No source evidence of objective restart, safe-shot constraint, shot outcome framing, civilian/escort escape checks, destination prefetch, or encounter completion→checkpoint progression. Combat Echo has encounter boundary APIs, but no native encounter director caller. No ability or C++ map search can establish this content's end-to-end state.
4. **Decision:** Extend existing Narrative quest/events and sequence callbacks through one project-owned mission transaction and checkpoint adapter. Author missions in the existing content system. Do not introduce a competing generic quest graph solely to match class names in the TDD.
5. **Dependencies:** C1–C3, shared input/camera profiles, both starter weapon/ability sets, encounter spawners/AI, level navmesh, configured enemy assets, cinematic participant bindings, objective UI and subtitles.
6. **Risk:** High because canon, content and systems intersect; a missing event can create a softlock.
7. **Order:** After combat arena proof and save/handoff; start with short M01 ending→M02 opening route before full mission art.

### C5. Sustain drops largely exist; content integration and retry policy need verification

1. **Exists:** `USovCombatSustainDropComponent` binds typed `OnDamageResolvedAsTarget`, accepts authoritative player-caused hostile fatal transactions, claims reward once, resets eligibility on revive, and deferred-spawns configured ammo/Echo pickup classes. `ASovNPCCharacterBase` includes it. `ASovCombatSustainPickup` uses replicated physics sphere plus pawn overlap, 20-second lifetime and collection FX/audio; prevents dead/uncontrolled actors collecting. Ammo enters `UNarrativeInventoryComponent`, caps total carried quantity using ammo MaxStackSize and leaves partial remainder. Echo uses `USovEchoComponent::AddEcho` with source attribution, and full Echo does not reset decay.
2. **Requires:** Approved post-TDD exception: small transient automatic ammo/Echo combat-sustain rewards, no loot economy. TDD combat/checkpoint principles require readable reward and coherent encounter retry.
3. **Gap:** Class fields deliberately default empty, so there are no default drops until assets assigned. Authoritative native behavior is implemented, asset bindings are not verifiable. Transient pickups do not save, so checkpoint snapshots must not restore already-killed enemies with no authored resupply while dropping uncollected resources. Pickup is begin-overlap driven: after partial or full rejection it needs re-entry; docs explicitly say step back over it. Partial ammo grants intentionally return false and do not play final collection FX, so any desired partial feedback needs extending the existing contract. No runtime drop tests supplied.
4. **Decision:** Preserve; extend validation and encounter reset/resource policy. Do not route through Narrative loot tables or generic interactable pickups.
5. **Dependencies:** Pawn/NPC Blueprint parent migration, hostile factions, typed fatal damage route, proper ammo class and starting stack, collection art/audio, nav-safe floor collision, checkpoint actor policy.
6. **Risk:** Low for asset wiring, medium for retry/resource exploit policy and physics replication.
7. **Order:** Configure during first combat arena validation; checkpoint policy in C3 slice.

### C6. Mechanical draw/stow is a real state machine, not a missing feature

1. **Exists:** `ASovTransformingWeaponVisual` extends `AWeaponVisual`, preserves Narrative wield state, stages Holstered→Drawing→Deploying→Ready→Retracting→Stowing, timestamps replicated phase and serial, reconstructs late clients, reverses at current progress, watchdog-completes absent montage notifies, gates Equipping/BlockFiring and collision outside Ready, handles death/revive, and exposes native weapon/character sequences and phase cues. Optional Deflection weapon montage uses a custom weapon AnimBP path.
2. **Requires:** User-approved Verity collapse/eject presentation; TDD §§14.3–14.5 legible state/cancel cleanup. Current design deliberately uses one skeletal mesh, not duplicate weapon items.
3. **Gap:** Actual skeleton, montages, notifies, sockets, linked layers and Blueprint class assignment unverified. Docs state direct weapon swaps should pass through empty/holstered semantic wield state so outgoing stow owns its montage/overlay before incoming draw. This is an authoring dependency rather than a proven automatic cross-weapon transaction. Native single-node transition animations and Deflection montage AnimBP must be configured consistently; using both paths incorrectly breaks visual animation despite correct gates. No automated runtime phase/interrupt tests in supplied test suite.
4. **Decision:** Preserve the state machine. Extend the existing equipment coordinator only if direct-swap test exposes races; add phase/reversal/death/normal-weapon regression tests and content validation, not another holster system.
5. **Dependencies:** Verity skeletal asset and AnimBP, actual Narrative weapon definition class/attachment setup, readiness, montage slots and attack gate enforcement.
6. **Risk:** Medium. Shared animation layers and grants can be disturbed; rapid input and death during transition are primary regression cases.
7. **Order:** Validate with Axiom/Verity first playable Selene arena; coordinator change can follow independently if required.

## Recommended campaign vertical slices

1. **Combat arena proof first (parent-selected native Axiom Null Pulse):** bridges verified Echo spend and Selene command-link/shield control rather than adding more unused foundations. Validate on actual Handler/Hound and shielded drone targets; keep command-link and damage/defense components authoritative. This can be a contained code slice without pretending to implement mission content.
2. **Reliable retry for one arena:** `ASovPlayerState`, `USovEchoComponent`, existing Narrative ASC save hooks, `UNarrativeSave` subclass, savable encounter adapter and Narrative quest event hooks. Establish selected resource defaults, snapshot phase/actors, inventory totals, and exact retry policy; kill, spend, reload and compare full expected state. Tests include full/empty resources, partial ammo, live/dead NPC restore, stale transient tags and failed write. Done: one arena restores consistently with no resource duplication or lost objectives.
3. **Authored M01→M02 handoff:** project controller/GameMode/PlayerState and save adapter, actual Tarrik/Selene definitions, weapon grants and Narrative sequence/quest hooks. Validate pair class+definition, snapshot outgoing state, cancel/detach previous avatar abilities/components, install destination state and only unlock HUD/input after readiness; rollback failed handoff. Test both normal/skip route, reload before/after boundary, failed destination, repeat event and rapid death. Done: one controlled transition and retry route preserves each protagonist's correct kit and mission state.
4. **M01/M02 critical-path integration:** actual map and quest packages, spawners, cinematic assets and checkpointable world actors. Build validated canon gate journal atop Narrative; verify every required fixed outcome and optional-route return. Normal/skip/reload must produce matching canon state. Done requires local editor playthrough from clean boot through M02 exit, with death/reload at every declared checkpoint.

## Build/test environment findings

- `ProjectVelkorran.uproject` declares UE 5.7; project runtime depends on NarrativeArsenal, NarrativeSaveSystem, GAS, Niagara, PhysicsCore and AnimGraphRuntime. Enabled third-party requirements include NarrativePro and ZenDyn plus MetaHuman plugins.
- No UnrealEditor, UnrealBuildTool or dotnet executable found on PATH. No UnrealEditor/UnrealBuildTool.dll/Build.bat/Build.sh found in accessible searched roots `/opt`, `/workspace`, `/usr/local`; g++ exists at `/usr/bin/g++`. This is not enough for UHT/UBT or Unreal runtime tests.
- Available test sources: `SovCampaignFoundationTests.cpp`, `SovDominionHandlerTests.cpp`, `SovDominionHoundAbilityTests.cpp`, with five test registrations. Foundation includes native component/config tests and Selene precision-generation tests. No supplied sustain/holster/checkpoint/mission functional test and no repository workflow files found in the materialized tree at audit time. Remote CI availability not independently established by this audit.
- Full build additionally needs licensed UE5.7 installation/toolchain and required plugins; meaningful editor/game tests additionally need local Content/ and Narrative content (maps, Blueprint classes, Data Assets, montages, effects). The absence of engine binaries in Git is ordinary, not an instruction to commit the engine. A configured Windows build agent is the appropriate reusable route.

## Exact source evidence anchors

- `Source/ProjectVelkorran/Private/Framework/SovCampaignGameMode.cpp`: constructor only; project class defaults.
- `Source/ProjectVelkorran/Private/Framework/SovPlayerController.cpp`, `SovPlayerState.cpp`: empty extension constructors.
- `NA/Private/UnrealFramework/NarrativeGameMode.cpp:54` GetPlayerDefinitionForController, `:87` SpawnDefaultPawnAtTransform, `:155` ProcessServerTravel.
- `NA/Private/UnrealFramework/NarrativePlayerCharacter.cpp:493` OnCharacterVisualInitialized save/readiness path, `:720` SetPlayerDefinition live replacement rejection.
- `NA/Private/GAS/NarrativeAbilitySystemComponent.cpp:920` Load; `:945` PrepareForSave; `NA/Public/GAS/NarrativeAbilitySystemComponent.h:156` selectable AttributesToSave.
- `NS/Private/Subsystems/NarrativeSaveSubsystem.cpp:116` Save; `:255` LoadPlayerData; `:301` CreatePlayerOnlySave; `:344` LoadPlayerOnlySave; `:749` LoadActorFromRecord; `:814` InitializeSaveSystem.
- `NS/Public/NarrativeSave.h`: one PlayerData, GUID record map, initial-only schema, commented-out SavedLevels.
- `NA/Public/Tales/TalesComponent.h`: FNarrativeSavedQuest and savable quest progress; `NA/Private/Cinematics/NarrativeLevelSequenceActor.cpp`: native participant binding/play/stop/finish machinery.
- `Source/ProjectVelkorran/Private/Components/SovCombatSustainDropComponent.cpp`: fatal eligibility and once-per-death spawning.
- `Source/ProjectVelkorran/Private/Combat/Pickups/Sov{Combat,AmmoCombat,EchoCombat}SustainPickup.cpp`: collection/cap/remainder/decay behavior.
- `Source/ProjectVelkorran/Private/Weapons/SovTransformingWeaponVisual.cpp`: phased presentation and gates; `Docs/TransformingWeaponVisuals.md`: direct swap/AnimBP integration contract.
