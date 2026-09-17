# Architecture, save, build and QA audit (adversarial), 17 September 2026

Reviewer domain: runtime architecture, save architecture, platform, build/shipping configuration,
performance/scalability engineering, content pipeline tooling, QA/validation.

## Scope and method

- Tree: `F:\ProjectVelkorran`, branch `codex/aurelion-tdd-content-20260913`, HEAD `f07538c9`. The user's
  uncommitted `ProjectVelkorran.uproject` change (Aura entry) and modified `.uasset` files were read but are
  not judged as repository defects.
- Authority: `Docs/Design/Sovereign_Call_Origins_TDD_v2_2026-08-14.md` §15, §16 (esp. §16.11), §17.5, §18,
  §19, §20.4, §21.3, with §0.2/§0.3/§0.7; accepted deviations in `Docs/CampaignV2ChangeLog.md` (none of its
  six entries touch architecture, save, build or QA).
- Read-only. No Unreal, UBT, UAT or automation was run (a lead run was in progress). Engine behaviour was
  checked against the installed UE 5.7 source under `C:\Program Files\Epic Games\UE_5.7\Engine\Source`.
- Evidence classes used below: **source-proven** (control flow traced in project or engine source),
  **artifact-proven** (files under `Saved/`), **missing integration**, **needs runtime/hardware evidence**.
- Lead-verified facts accepted without re-derivation: editor target builds; 683/683 native automation
  tests passed headless on 2026-09-16; unity collisions in the editor module fixed around `ede889e1`.

## TDD coverage table

| TDD section | Status | Basis |
|---|---|---|
| §15.1 principles | Partial | C++ owns save/validation contracts; tags/messages/data-asset principles not followed (AR2-12) |
| §15.2 module layout | Divergent | One runtime module `ProjectVelkorran` + editor + tests; Sovereign tags/policies live in the vendor `NarrativeArsenal` module (AR2-12). No cycles possible with one module, so §15.18 bullet 1 is vacuously met |
| §15.3 core runtime classes | Partial | Present: `ASovCampaignGameMode`, `ASovPlayerController`, `ASovPlayerState`, `USovSaveSubsystem`, `ASovPlayerCharacterBase`, Tarrik/Selene, Echo/Targeting/Corruption/Companion components, `ASovEncounterDirector`. Absent (Narrative equivalents used): GameInstance, GameState, ASC, attribute set, movement, interaction, camera, equipment, world-state subsystem, `ISovCheckpointable`, door/volume/anchor classes |
| §15.4 character init | Not audited in depth | Other reviewer domain; save admission reads readiness/epochs |
| §15.5 GAS | Not in domain | — |
| §15.6 tag taxonomy | Divergent | 206 native tags, all under `Sov.*`/`Narrative.*` (`Plugins/.../NarrativeArsenal/Private/Sovereign/SovGameplayTags.cpp`), not the 18 TDD top-level namespaces; no naming-rule validator; 22 tags documented only as "Native campaign runtime contract" |
| §15.7 data assets | Partial | 7 native data-asset types (`SovCampaignDefinition`, `SovEvidenceDefinition`, `SovStatusDefinition`, `SovMeleeAttackDefinition`, `SovCorruptionProfile`, `SovNarrativeCue`, `SovDismembermentProfile`) vs 16 listed; no per-asset schema version field |
| §15.8 typed messages | Absent | Zero `GameplayMessage` usages in `Source` and the Narrative plugin; 79 dynamic multicast delegates instead |
| §15.9 save architecture | Substantial, with P1 gap | Two-bank generations, readback, recovery archives, owner token, header fields, portable settings, migration history. Gaps: pre-integrity decode (AR2-01), travel record bypass (AR2-05), no golden files (AR2-08) |
| §15.10 asset management | Weak | No mission/protagonist primary-asset labels (`Config/DefaultGame.ini:14-35` registers only Map, PrimaryAssetLabel, NPCDefinition, PlayerDefinition); 3 async-load sites in runtime vs 35 synchronous (AR2-13); template boot config roots demo content (AR2-02) |
| §15.11 streaming/world state | Replaced | Narrative record map with restore phases instead of `USovWorldStateSubsystem`/`ISovCheckpointable`; GUID validity checked at decode (`SovSaveSubsystem.cpp:793`) |
| §15.12 performance targets | Harness exists, gate weaker than TDD | AR2-06, AR2-07 |
| §15.13 scalability | Not evidenced | No project scalability profiles found in `Config/` |
| §15.14 build configurations | Largely absent | Only Development Editor/Game ever built; no Test/Shipping/Performance/Cinematic/Accessibility configurations; no CI (AR2-03) |
| §15.15 logging | Declared, mostly unused | 11 categories declared; `LogSovSave` 0 uses, 6 categories 0 uses (AR2-15) |
| §15.16 error handling | Save paths good; corrupt-input path not safe (AR2-01) |
| §15.17 online/privacy | Acceptable for current scope | Hash-only account namespace (`SovCampaignSaveGame.h` header comment), no analytics; diagnostics local-only and disabled in Shipping |
| §15.18 acceptance criteria | Not met | Golden saves absent; performance at floor unmeasured; data validation cannot report zero errors; removed systems present in cook (AR2-02, AR2-03, AR2-07, AR2-08) |
| §16.11 validation commandlet | Partial, cannot pass | AR2-03, AR2-04 |
| §16.2/16.3 naming, source control | Partial | `Content/Cues` required but untracked (AR2-11) |
| §17.5 slice gate (technical rows) | Not met | Reload success 30/30 editor soak only; one Development packaged non-combat capture |
| §18.3 automated tests | Strong unit/functional count, gaps | 683 `ProjectVelkorran.*` registrations (676+1 in Tests module, 7 in Editor module); gaps: 30/60/120 fps melee traces, cinematic skip at timestamps, hot-swap, golden save migration, every mission state transition |
| §18.5 save testing | Partial | Synthetic storage faults, torn/truncated bytes, owner revocation, suspend. Absent: length-field/class-name fuzz, process kill on device, low-storage on device, golden migrations |
| §18.8 performance testing | Partial | AR2-07 |
| §18.9 soak | Minimal | 30-cycle editor checkpoint soak (`Docs/AurelionTDD90Blockers-2026-09-15.md:97-104`) |
| §18.12 RC gates | Not met | Legacy/demo content in campaign cook (AR2-02) |
| §19.4 removal | Partial | Asset-level cook scan exists; vendor vendor/inventory/XP code compiled in runtime (AR2-19); demo content still cooks (AR2-02) |
| §19.5/§20.4 isolation | Met trivially | No Operations/online plugins exist; commandlet path-prefix check only |
| §21.3 locked decisions | At risk via boot config | Vendor multiplayer menu/loot UI/XP events reachable from `GameDefaultMap` (AR2-02) |

## Findings

Severity: P1 blocks engineering acceptance, risks data loss, or can crash Shipping; P2 significant;
P3 minor.

### AR2-01 (P1) Corrupt save bytes reach unbounded engine deserialization and arbitrary class loading before any integrity check

- **TDD:** §15.9 ("Validate required assets and schema before applying loaded state"), §15.16 corrupted
  save, §18.5 corrupted newest autosave / unrecoverable save is a release blocker.
- **Where:** `Source/ProjectVelkorran/Private/Save/SovSaveSubsystem.cpp:58-70` (`LoadCampaignEnvelope`)
  checks only the first 8 bytes (GVAS tag and version 3) and then calls
  `UGameplayStatics::LoadGameFromMemory`. Engine: `Engine/Source/Runtime/Engine/Private/GameplayStatics.cpp:166-211`
  (`FSaveGameHeader::Read` reads engine version, a custom-version array and the class-name `FString`),
  `:2473-2485` (`LoadObject<UClass>` on the file-supplied class name, then `NewObject<USaveGame>` and
  `Serialize`), and `Engine/Source/Runtime/Core/Private/Containers/String.cpp.inl:1817-1829` (length limit
  applies only when `GetMaxSerializeSize() > 0`; `FMemoryReader` leaves it 0, so `AddUninitialized(SaveNum)`
  runs with the corrupt length). Integrity is checked only afterwards (`SovSaveSubsystem.cpp:495`). Local
  banks have no size cap at all (`ReadBest`, `:538-539`); only cloud bytes are capped at 64 MiB (`:345`).
- **Failure sequence:** a torn or bit-flipped bank keeps its valid 8-byte preamble but has a corrupt
  custom-version count or class-name length (e.g. `0x7FFFFFF0`) → `ReadBest` → `LoadCampaignEnvelope` →
  multi-GB allocation request, or `LoadObject` of an arbitrary path, or `NewObject<USaveGame>` with a
  non-`USaveGame` class (asserts under `DO_CHECK`, unchecked in Shipping) → process terminates. Because
  `ListSlots` (`:699-717`, called from `UI/SovAurelionPauseMenu.cpp:113,187`), the autosave tick (`:1077`),
  `WriteEnvelope` (`:561,596`) and `FindRecoveryAutosave` (`:722-729`) all decode every bank, the crash
  recurs on each visit; the "preserve the file for support and offer last known-good" path is never reached.
- **Existing coverage:** `MalformedPreambleFallsBackAndPreservesRepairBytes` and truncation cases
  (`SovCheckpointContractRuntimeTests.cpp:376`) cover the preamble and truncation, not length fields or
  class names.
- **Evidence class:** path source-proven against installed 5.7 source; the exact terminal outcome (OOM
  fatal vs. assert vs. error flag) needs fuzzing on target hardware. **Confidence:** high on reachability,
  medium on crash mode.
- **Fix inside existing owners:** in `LoadCampaignEnvelope`, before `LoadGameFromMemory`: cap total bytes
  (reuse `SovPlatformServicesPolicy::MaximumEnvelopeBytes`), parse the GVAS header manually with a bounded
  reader (custom-version count and every string length ≤ remaining bytes), and require the class name to
  equal `USovCampaignSaveGame`'s path; or set `Reader.ArMaxSerializeSize` via a wrapping archive. Longer
  term, prepend a raw-bytes CRC frame verified before any UObject work (the SP-05 recommendation), with a
  documented schema-1 migration. Add fuzz tests over length fields and class names to
  `SovSaveRuntimeTests.cpp`.

### AR2-02 (P1) The packaged boot configuration roots Narrative's demo world, character creator, multiplayer menu and legacy UI into every campaign cook

- **TDD:** §15.18 ("Removed campaign systems are absent from cooked runtime references"), §19.4, §18.12,
  §15.10 (startup loads only global UI/front end), §21.3.
- **Where:** `Config/DefaultEngine.ini:93-96` set `GameInstanceClass=BP_NarrativeGameInstance`,
  `GameDefaultMap=/NarrativePro/Pro/Core/Maps/MainMenu/MainMenuMap`,
  `GlobalDefaultGameMode=BP_NarrativeGameMode`. The vendor widget
  `Plugins/Narrativeed3f9374a6eV6/Content/Pro/Core/UI/Menus/MainMenu/W_NarrativeMenu_MainMenu.uasset`
  (and `W_NarrativeMenu_MPMainMenu.uasset`) contain the `L_DemoMap_OpenWorld` reference.
- **Artifact proof:** the 15 September M12 capture package
  `Saved/AurelionPerfCapture-20260915-150424/Windows/Manifest_UFSFiles_Win64.txt` stages
  `Pro/Demo/Maps/OpenWorld/L_DemoMap_OpenWorld.umap`, `L_DemoMap_Old.umap` and their generated WP cells,
  `Pro/Core/CharCreator/CharacterCreator.umap`, `MainMenuMap.umap`, 612 `Pro/Demo/Maps`, 255 `Pro/Demo/Items`,
  26 `Pro/Demo/Quests`, 108 `Pro/Core/Inventory` entries; 10,199 of 20,840 staged files are Narrative plugin
  content, and the container is 6.1 GB for a two-map slice. This postdates the T2 AlwaysCook fix
  (`c2b322d8`, 13 Sep). `Docs/EngineeringBacklogReconciliation-2026-09-12.md:414,445-453` already names these
  routes and reclassifies them as a product/content decision (T2-B); nothing has changed since.
- **Failure sequence:** any Shipping package boots into the vendor main menu, whose "new game"/multiplayer
  paths reach the demo open world, XP events and loot UI; the cook fails §15.18 and §18.12 by construction,
  and memory/size budgets are measured against a build that is not the campaign.
- **Evidence class:** artifact-proven (staged manifest) plus source (config). **Confidence:** high.
- **Fix inside existing owners:** point `GameDefaultMap`/`GlobalDefaultGameMode`/`GameInstanceClass` and
  `ArsenalSettings.GameEntryMap`/`CharacterCreatorMap` at campaign-owned assets (a minimal campaign front
  end map with `ASovCampaignGameMode`), add `DirectoriesToNeverCook` for `/NarrativePro/Pro/Demo`, and make
  `Validate-Campaign.py` fail the package stage when the staged manifest contains `/Pro/Demo/`.

### AR2-03 (P1) No executable gate can qualify a Test/Shipping build, a cook, or data validation

- **TDD:** §15.14 (CI cooks representative missions, runs data validation, save migration, automation, map
  checks, forbidden-reference scans), §15.18, §16.11 ("Errors block candidate builds"), §18.12.
- **Where:**
  - `Scripts/Validate-Unreal.ps1:193-205` builds `ProjectVelkorranEditor` (and `ProjectVelkorran` only with
    `-BuildGame`), always `Development`; `:226` runs automation under `-NullRHI`; `:173` hard-codes
    `packagedBuild = 'not run'`; `:250` counts `succeededWithWarnings` as passing (91 of 667 tests passed with
    warnings on 15 Sep per `Docs/AurelionTDD90Blockers-2026-09-15.md:41`).
  - `Scripts/Validate-Campaign.py:188-207` plans Editor/Game `Development` builds and an optional
    `-clientconfig=Development` package. No script anywhere builds `Test` or `Shipping`;
    `Intermediate/Build/Win64/ProjectVelkorran` contains only `Development`, and `Binaries/Win64` has no
    Shipping receipt. `#if !UE_BUILD_SHIPPING`/`UE_BUILD_SHIPPING` code paths
    (`Diagnostics/SovDebugApproachCommands.cpp:16`, `Diagnostics/SovDiagnosticsSubsystem.cpp:33`) have never
    compiled in their Shipping form.
  - The campaign-preflight stage cannot pass on the only campaign content:
    `Validation/SovValidateCampaignCommandlet.cpp:359-360` hard-requires `M01_Mantle` and `M02_OneDegree`,
    which exist only as native test-shaped definitions (`Campaign/SovCampaignDefinition.cpp:453-500`, no map,
    `FText::FromString` objectives that fail `-ShippingValidation` at `:289-290`); the M12/M13 manifest is also
    rejected ("Curated companion abilities must be concrete and unique",
    `Docs/EngineeringBacklogReconciliation-2026-09-12.md:455-457`).
  - No CI definition exists (no `.github`, no build-farm config).
  - `Check-TestModuleIsolation.py --receipt/--uht-manifest/--stage-manifest` (Shipping metadata proof of B5)
    has never been supplied evidence and is wired into no runner.
- **Failure sequence:** a revision can pass every checked-in gate while a Shipping-only compile error, a
  cook failure, a data-validation error, or demo content in the package (AR2-02) is present.
- **Evidence class:** missing integration, source-proven. **Confidence:** high.
- **Fix inside existing owners:** extend `Validate-Campaign.py`'s stage plan with `Test` and `Shipping`
  Game builds and a Shipping package, make its manifest check accept the slice manifest (replace the
  `M01`/`M02` literal with a manifest-declared opening set), require zero accepted warnings without a
  waiver file, run `Check-TestModuleIsolation.py` with the produced receipt/UHT/stage manifests, and record
  the result in the existing run manifest.

### AR2-04 (P2) §16.11 validator coverage is still missing most structural categories

- **TDD:** §16.11, §18.3 content validation.
- **Where:** `SovValidateCampaignCommandlet.cpp:36-88` dispatches validators for cues, evidence, status,
  melee, corruption, dialogue and dialogue Blueprints only; `:391` itself states map actors, World Partition
  coverage, ability cleanup, Blueprint compilation and translation coverage "require separate gates".
- **Missing categories:** invalid/undeclared gameplay tags; abilities without cleanup/animation/cost
  declarations; encounters over role budgets; checkpointable actors without stable GUIDs (only checked at
  runtime decode, `SovSaveSubsystem.cpp:793`); cinematics without skip/recovery policy; localization beyond
  the selected `FText` fields; warnings with owner and waiver date (no waiver mechanism). Forbidden content is
  a path-prefix list (`:144-145`) plus class-ancestry checks.
- **Evidence class:** source-proven. **Confidence:** high.
- **Fix:** extend `ValidateNativeAsset` and add an editor world pass for placed actors (GUID uniqueness,
  encounter role budgets, sequence skip policy); add a waiver file read by the commandlet.

### AR2-05 (P2) Mission travel records bypass the save envelope: no preamble guard, checksum, size cap or readback

- **TDD:** §15.9 save rules, §18.5 (false save-success, canon-state corruption).
- **Where:** write `Framework/SovPlayerController.cpp:810` →
  `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/Subsystems/NarrativeSaveSubsystem.cpp:358`
  (reads the previous slot with raw `LoadGameFromSlot`) and `:370-372` (`SaveDataToSlot`, no readback);
  read `Framework/SovCampaignGameMode.cpp:61` → `NarrativeSaveSubsystem.cpp:380` (raw `LoadGameFromSlot`).
- **Failure sequence:** the travel record carries the controller's campaign-state component bytes (canon
  state). A torn or stale slot is decoded with no GVAS preamble check — exactly the legacy pre-GVAS path that
  `SovSaveSubsystem.cpp:60-63` documents as able to hit a fatal FName length check — and with no CRC, so a
  bit-flipped but structurally valid record restores silently. The write path also reads the old slot first,
  so a corrupt leftover record can crash the *outgoing* travel.
- **Evidence class:** source-proven; outcome needs fault injection. **Confidence:** medium-high.
- **Fix:** route travel records through `USovSaveSubsystem` (a `Travel` slot kind reusing
  `LoadCampaignEnvelope`, checksum and readback), or at least apply the same preamble/size guard and a CRC
  in `CreatePlayerOnlySaveInSlot`/`ReadPlayerOnlySave`.

### AR2-06 (P2) Checkpoint and autosave perform ~20 full bank reads and UObject deserializations on the game thread

- **TDD:** §15.10 (no uncontrolled stalls), §15.12 (1.2 ms UI/audio/other budget), §18.8 (no sustained spike
  above 50 ms).
- **Where:** `WriteCheckpoint` → `CaptureAndWrite` → `WriteEnvelope`: `ReadBest` of the target (2 reads +
  2 decodes, `SovSaveSubsystem.cpp:561`), existing-target read + decode (`:591-597`), write, readback + decode
  (`:619-624`). The queued autosave then runs on the next ticker frame: 3 `ReadBest` calls (`:1076-1079`,
  6 reads/decodes), and its own `WriteEnvelope` repeats the target `ReadBest` (2), the auto-slot loop
  (`:567-570`, 6), existing target (1) and readback (1). Current envelopes are 458-688 KB
  (`Saved/SaveGames/SovCampaign_v1_*`). While an autosave is queued but inadmissible, `CanCaptureInternal`
  iterates all encounter directors twice and all pawns every tick (`:429-486`). `ListSlots` decodes up to 28
  banks per pause-menu refresh.
- **Evidence class:** source-proven cost structure; milliseconds need a packaged capture across a
  checkpoint (the 15 Sep packaged capture did not cross one). **Confidence:** high on I/O count.
- **Fix:** cache verified headers per bank keyed by generation and invalidate on write; compute the next
  auto generation from the cache; decode only the header for listing; keep the single readback.

### AR2-07 (P2) The performance harness verdict does not implement the §18.8 gate

- **TDD:** §18.8 (≥99% of frames on target, no sustained spike > 50 ms, no growing memory across three
  reloads), §15.18, §17.5.
- **Where / defects:**
  - Defaults judge p95 with 5% allowed over budget (`Diagnostics/SovPerformanceCaptureSubsystem.cpp:28-31`),
    not 99%. In `Saved/Validation/Performance/PackagedCapture-20260915-150900-faab9793`, worlds 1 and 3 are
    reported **Pass** with p99 16.86 and 17.12 ms (TDD failure).
  - Samples are a 4,096 ring buffer (`Public/Diagnostics/SovPerformancePolicy.h:23,79`); world 4's report shows
    `dropped_oldest: 905`, so ~18% of steady frames, including the post-load period, were never judged for
    hard stalls or streaks.
  - Memory trend (`SovPerformancePolicy.h:210-231`) needs two *consecutive* rises above 64 MB: loads of
    1000/1060/1120/1180 MB, or 1000/1100/1090/1190 MB, are "Stable".
  - `Scripts/Capture-AurelionPerformance.ps1:50-51` exits 0 when every report is `Fail` or the game exited
    nonzero; only a missing report fails.
  - Captures are Development-only, non-combat, one run; no CSV/Insights export or PSO gate.
- **Evidence class:** source- and artifact-proven. **Confidence:** high.
- **Fix:** default `JudgedPercentile=0.99`, `AllowedOverFraction=0.01`; keep running counters (over-budget
  count, max, longest streak) over all post-warmup frames independent of the ring; judge memory by
  first-to-last growth across the reload set; fail the script on any `Fail`/`Insufficient` verdict or nonzero
  exit.

### AR2-08 (P2) No golden save files; migration tests synthesize "old" saves from current code

- **TDD:** §15.9 ("Migrations are deterministic and covered by golden-file tests"), §15.18, §18.3, §18.5.
- **Where:** no golden/fixture save files exist in the repository. `SovCheckpointContractRuntimeTests.cpp:234`
  builds a schema-1 campaign by mutating live state (`FAccess::MakeSchemaOne`) and saves it with today's
  serializer; `SovSaveRuntimeTests.cpp:340-356` reconstructs the legacy checksum in the test.
- **Failure sequence:** a change to `FSovSaveSlotHeader`, Narrative record layout or component `Serialize`
  moves both writer and "old" fixture together, so the test stays green while real schema-1 banks on disk
  (e.g. those in `Saved/SaveGames`) become unreadable.
- **Evidence class:** source-proven. **Confidence:** high.
- **Fix:** check in immutable byte fixtures for each supported envelope/campaign-state version (generated
  once from a tagged build) and load them through `USovSaveSubsystem` in automation.

### AR2-09 (P2) Loss of the original account still has no exit from the blocking interruption menu (SP-02)

- **TDD:** §15.16, §18.5 platform user change, §18.9.
- **Where:** `UI/SovApplicationInterruptionMenu.cpp:31-35` constructs only `ResumeButton`;
  `Public/UI/SovApplicationInterruptionMenu.h:22` consumes Back; `Framework/SovApplicationLifecycleComponent.cpp:183-189`
  requires `HasStorageOwner()` to resume and its message (`:203-207`) only asks to reconnect.
- **Severity note:** was P1. Downgraded because desktop never revokes on unknown identity
  (`SovSaveSubsystem.cpp:283-285`) and console targets are blocked elsewhere; it returns to P1 when a console
  target is pursued (a certification failure).
- **Evidence class:** source-proven. **Confidence:** high.

### AR2-10 (P2) Cloud revision discovery orders by client clock; worker-thread callbacks are dropped without retiring the request (SP-04)

- **Where:** `Platform/SovOnlinePlatformServicesAdapter.cpp:213` (lexicographic max filename), `:219-220`
  (filename from local `FDateTime::UtcNow()`), `:45,50,52,54,57,180` (callbacks return silently when not on
  the game thread), `:146` (provider rebind refused while a request is pending, so a dropped terminal
  callback wedges `Begin`, `:185`).
- **Evidence class:** source-proven; provider callback threading needs a real OSS. **Confidence:** high
  (ordering), medium (wedge).
- **Fix:** as SP-04: list all verified revisions with metadata for explicit choice; marshal callbacks to the
  game thread with the request id instead of discarding.

### AR2-11 (P2) The only configured GameplayCue root is untracked content

- **Where:** `Config/DefaultGame.ini:62` `+GameplayCueNotifyPaths="/Game/Cues"`; `.gitignore:58-59` ignores
  `/Content/*` except Aurelion; `git status` shows `?? Content/Cues/` (14 files), previously tracked in
  `696a4380` and dropped in `14490513`. `Docs/EngineeringBacklogReconciliation-2026-09-12.md:156,217` records the
  `GameplayCuesStillResolve` automation failure on a checkout without it.
- **Failure sequence:** any clean checkout, second workstation or future CI worker builds and cooks with no
  cues resolving (hit/impact presentation silently absent), and the gate cannot notice because cue presence
  is not validated.
- **Evidence class:** source/VC-proven. **Confidence:** high.
- **Fix:** track `Content/Cues` (force-add, as with `Content/Abilities`) or move the forked cues under
  `Content/Aurelion`, and add a commandlet check that each native cue tag resolves.

### AR2-12 (P2) Architecture contracts in §15.2/§15.6/§15.8/§15.10 are replaced without a change-log entry

- **Where:** single runtime module (`Source/ProjectVelkorran/ProjectVelkorran.Build.cs`); all 206 Sovereign
  native tags and several Sovereign policies live in the vendor module
  (`Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Sovereign/SovGameplayTags.cpp`,
  `Public/Sovereign/SovLookInputPolicy.h`, `SovMovementAssistPolicy.h`, `SovEnvironmentDamage.h`), inverting the
  §15.2 dependency direction and coupling vendor upgrades to campaign contracts; no typed gameplay messages;
  no campaign primary-asset types or labels in Asset Manager settings (`Config/DefaultGame.ini:14-35`); the
  validation commandlet (editor-only work) lives in the runtime module.
- **Why P2:** each is a deliberate-looking substitution (Narrative Pro ASC/save/tags), but §0.4 requires
  recording material changes; `Docs/CampaignV2ChangeLog.md` has none, so reviewers cannot distinguish
  intent from drift, and §15.10 mission bundles/preload (§15.12 memory baseline) have no mechanism.
- **Evidence class:** source-proven. **Confidence:** high.
- **Fix:** record the Narrative-substitution decisions in the change log; move Sovereign tag registration
  into `ProjectVelkorran` (tags can still be declared natively there); register `SovCampaignDefinition` as a
  primary asset type with mission bundles.

### AR2-13 (P2) Combat-time synchronous loads with no residency contract

- **Where:** Elite summon `Abilities/SovGameplayAbility_AurelionElite.cpp:262-263`; Mass promotion
  `Campaign/SovEncounterDirectorMass.cpp:268-269`; Mass proxy visuals `Campaign/SovCampaignMassProxy.cpp:39-54`;
  narrative barks `Narrative/SovNarrativeCueComponent.cpp:154-155` (prior V04, not re-assigned to this
  reviewer, still open). Only 3 async-load call sites exist in runtime source.
- **Evidence class:** source-proven; stall size needs packaged capture (assets may already be resident).
  **Confidence:** medium.
- **Fix:** preload encounter/mission bundles through the Asset Manager at encounter arm and hold handles.

### AR2-14 (P3) Diagnostics subsystem still ticks and binds delegates in Shipping (B6)

- `Diagnostics/SovDiagnosticsSubsystem.cpp:40-45` refreshes bindings every 0.25 s and `:96-126` binds
  damage/ability/health/Echo/campaign delegates regardless of `IsRecordingEnabled()` (`:31-39` returns false in
  Shipping). **Fix:** `ShouldCreateSubsystem` false in Shipping, or bind only while recording.

### AR2-15 (P3) Logging categories declared but not used on the paths §15.15 names

- `LogSovSave`, `LogSovAbility`, `LogSovCombat`, `LogSovEncounter`, `LogSovNarrative`, `LogSovWorld`,
  `LogSovUI`, `LogSovCinematic` have zero `UE_LOG` uses; `SovSaveSubsystem.cpp` logs no failure (errors only
  reach delegates) and uses `LogTemp` at `:780,790`. QA cannot reconstruct a save failure from a log.

### AR2-16 (P3) Host test suite is red at HEAD and the isolation checker is wired into no gate

- `python -B -m unittest discover -s Scripts/Tests -p "Test*.py"`: 119 run, 1 error, 11 skipped, exit 1.
  `Scripts/Check-TestModuleIsolation.py:54-56` rejects the 7 registrations in the Editor module
  (`Source/ProjectVelkorranEditor/Private/SovAurelionNPCIdentityLibrary.cpp:351,392,467`,
  `SovAurelionStandOffAuthoringLibrary.cpp`, `SovWeaponHUDAuthoringLibrary.cpp`), present since 11 Sep. The
  Editor module is itself Editor-only, so Shipping isolation is not breached; the rule is stale, and nothing
  runs the checker.
- `Scripts/Check-ConsoleBuild.py:47,77` uses strict `json.loads`; 36 installed UE 5.7 `.uplugin` files have
  trailing commas, so `--plugin-root <Engine/Plugins>` exits 2 and the tool cannot run against a real engine.

### AR2-17 (P3) A newer-schema bank is treated as damaged and its slot position reused

- `SovSavePolicy.h:29` returns `NewerSchema`; `ReadBest` marks it damaged (`SovSaveSubsystem.cpp:542-545`);
  `WriteEnvelope` archives it to `_Recovery_<guid>` and overwrites that bank with a low generation
  (`:577,599-613`). If the other bank also holds a newer-schema save with a higher generation, re-upgrading
  selects it and shadows progress made after the downgrade. Preserved bytes prevent loss; the selection is
  wrong. Relevant for PC version rollback only.

### AR2-18 (P3) Package contents depend on the build machine and include unneeded runtime payload

- `Config/DefaultEngine.ini:105` enables DLSS for a plugin that is deliberately untracked
  (`Docs/NvidiaDLSS-2026-09-16.md`), so packages differ by build machine; the run manifest does not record
  whether it was present. `ProjectVelkorran.uproject:66-67` enables `USDImporter` for all targets; the
  packaged Development build stages `boost_python311-mt-x64.dll` and other boost/TBB libraries
  (`Saved/AurelionPerfCapture-20260915-150424/Windows/Manifest_NonUFSFiles_Win64.txt`). Before shipping, set
  the NGX application id and disable on-screen DLSS debug/OTA as that document lists.

### AR2-19 (P3) December systems remain compiled into the campaign runtime

- `NarrativeArsenal/Public/Items/VendorInventoryComponent.h`, `Items/InventoryComponent.h`, the XP attribute on
  `UNarrativeAttributeSetBase` (checked by `Validation/SovCampaignContentValidation.cpp:175`), and cheat-manager
  XP paths remain in the runtime module. §19.4 asks for removal or isolation, not only cook scanning.

## Prior-finding dispositions

| ID | Prior priority | Disposition | Evidence |
|---|---|---|---|
| B1 unity test-helper collisions | P1 | **Fixed** | Tests moved to Editor-only `ProjectVelkorranTests`; helpers in named namespace `SovTarrikPayloadTests` (`SovTarrikPayloadRuntimeTests.cpp:22,83,95`); `ede889e1` fixed editor-module collisions |
| B2 unity overload changes Selene eligibility | P1 | **Fixed** | `IsLivingProtectionParticipant` (`Combat/SovProtectionInterceptReceipt.cpp:42`) and `IsLivingSelenePayloadParticipant` (`Combat/SovSelenePayload.cpp:21,88,119`); host compiler cross-check skipped here (no compiler) |
| B3 generated include not last | P1 | **Fixed** | `NPCActivityComponent.h:13-16` debugger include precedes `.generated.h` |
| B4 DXGI leaks / failed-query deref | P1 | **Fixed** (semantic residual) | `ArsenalStatics.cpp:145-171` uses `ComPtr`, `QueryInterface`, checks every HRESULT; `GetMonitorNames` guards Slate (`:84`). Adapter 0 and budget-as-`TotalVRAM` retained by stated contract |
| B5 reflected fixtures in Shipping modules | P2 | **Fixed** (verification residual) | 0 registrations/fixtures in `Source/ProjectVelkorran`; test module `Type: Editor`, `TargetAllowList: [Editor]`, `BuildException` for non-Editor. Packaged metadata proof never produced; checker red (AR2-16) |
| B6 diagnostics tick/bind in Shipping | P3 | **Still open** | AR2-14 |
| SP-01 authorization not held through I/O | P1 | **Fixed** | `FOperationOwner` with selection/authorization/suspension epochs checked before and after every read/write/exists (`SovSaveSubsystem.cpp:110-165`), after each serializer and asset load (`:494-519,737-790`), selection revalidated after hint persistence (`:214-223`); tests `OperationOwnerEveryStorageBoundary`, `OperationOwnerActualEnvelopeSerialization`, `ProfileHintReentryCannotPublishRevokedSelection`. Residual by design: a write already issued is reported unconfirmed (`:123`) |
| SP-02 no exit after account loss | P1 | **Still open** (re-rated P2) | AR2-09 |
| SP-04 cloud clock ordering | P2 | **Still open** | AR2-10 |
| SP-05 corrupt input before bounded preflight | P1 | **Partially fixed** | GVAS tag/version guard added (`:58-70`) closes the pre-GVAS FName path only; length fields, class name and local size remain unbounded (AR2-01) |
| V01 gate cannot qualify playable/Shipping | P1 | **Still open** (partial tooling added) | `-BuildGame`, `-NonUnity`, source manifests, `Validate-Campaign.py` optional Development package; no Test/Shipping, no CI, preflight cannot pass (AR2-03) |
| V02 commandlet misses native contracts | P1 | **Partially fixed** | Added GameplayEffect/CharacterDefinition/NarrativeEvent dispatch, AlwaysCook/config/CookList roots, prohibited-class ancestry; map actors, GUIDs, role budgets, ability cleanup, cinematic skip still absent (AR2-04) |
| V03 no performance qualification harness | P2 | **Partially fixed** | `USovPerformanceCaptureSubsystem`, portable policy tests, packaged PC capture with reports; verdict weaker than §18.8, ring-buffer blind spot, no combat capture (AR2-07) |

## Executed host checks

| Command | Exit | Result |
|---|---:|---|
| `python -B -m unittest discover -s Scripts/Tests -p "Test*.py"` | 1 | 119 tests, **1 error** (`TestModuleIsolation.test_actual_source_is_isolated`, AR2-16), 11 skipped (6 need symlink privilege, 5 need a host C++ compiler/preprocessor) |
| `python -B Scripts/Check-ConsoleBuild.py` | 1 | `descriptor_blocked: true`; 30 blockers: 26 `plugin-unresolved` (engine plugins, no root supplied; includes the user's local Aura entry) + 4 `required-module-platform-review` (NarrativePro, NarrativeCommonUI, NarrativeArsenal, NarrativeSaveSystem allow only Mac/Win64/Android/Linux); 1 review |
| `python -B Scripts/Check-ConsoleBuild.py --plugin-root "C:/Program Files/Epic Games/UE_5.7/Engine/Plugins"` | 2 | Argument error: engine descriptor JSON with trailing commas (36 files) is rejected (AR2-16) |
| `python -B Scripts/Test-NativePolicies.py` | 2 | Not run: no `g++`/`clang++` on this host |
| Registration count (static scan) | — | 686 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`: 677 in `ProjectVelkorranTests` (676 `ProjectVelkorran.*`, 1 `NarrativeArsenal.*`), 7 in `ProjectVelkorranEditor`, 2 NVIDIA DLSS; 683 match the default `ProjectVelkorran` filter, consistent with the lead's 683/683 |

No Unreal build, cook or automation was executed by this reviewer.

## Alignment estimates

### (a) Runtime architecture and save contracts: 50–65%, midpoint 57%

The save subsystem is the strongest engineering in this domain: explicit operation ownership at every
storage boundary, two-bank generations with readback, recovery archives, retry decisions, suspension
accounting, exact load-request tokens, and a large adversarial automation set. That carries §15.9 and
§15.16 a long way. It is held down by one P1 (decode before integrity, AR2-01), the unguarded travel record
(AR2-05), absent golden migration files (AR2-08) and unmeasured synchronous I/O (AR2-06). On the
architecture side, the §15.2 module layout, §15.6 taxonomy, §15.8 typed messages, §15.10 asset
management and §15.11 world-state interfaces are replaced by Narrative Pro mechanisms without a recorded
decision (AR2-12), and the boot configuration is still the vendor template (AR2-02). The 11 September range
(55–70%) was slightly generous given AR2-01/AR2-02 were present then; save hardening since has offset
some of that.

### (b) Asset pipeline, platforms, production and QA: 22–35%, midpoint 28%

Real progress since 11 September: a packaged, rendered PC capture with machine-readable reports, a
performance policy with portable tests, cook-root-aware validation, source manifests, and a 683-test
native suite with registration-coverage checking. But no Test or Shipping target has ever been compiled,
there is no CI, the §16.11 commandlet cannot pass on the slice and misses most structural categories, the
cook still carries vendor demo worlds, console descriptors remain blocked, the performance verdict is
weaker than §18.8 and excludes combat, the cue content required by config is untracked, and the host
suite is red. That places it modestly above the earlier 18–33% range rather than at a materially new level.
