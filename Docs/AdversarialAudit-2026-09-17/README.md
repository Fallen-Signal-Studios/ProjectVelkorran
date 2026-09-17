# Project Velkorran: adversarial engineering audit, 17 September 2026

**Estimate: about 37% aligned with the August TDD (range 32–43%).** The engineering foundation is much further along than that. The whole-TDD figure is held down by authored content (missions, hubs, dialogue, art, audio), by systems that are built and tested but not connected to the shipped player kit, and by release gates that nothing checks yet.

This is a fresh audit. It replaces the stale [5 September audit](../AdversarialAudit-2026-09-05/README.md) and is independent of the delta estimate in [TDDAlignment-2026-09-11.md](../TDDAlignment-2026-09-11.md), which also landed at ~37%.

The two numbers match for different reasons:
- **Better than on 11 September:** most of the 5 September transaction and lifecycle defects are now fixed, with tests.
- **Worse:** an adversarial reading found wiring gaps a delta estimate could not see. The main one is that the protagonists do not use the native melee layer.

## Scope and method

| Item | Value |
|---|---|
| Revision | `f07538c9` on `codex/aurelion-tdd-content-20260913`, plus the working tree (uncommitted local content noted where it matters) |
| TDD | *Sovereign Call: Origins* TDD v2.0, 14 August 2026, `Docs/Design/Sovereign_Call_Origins_TDD_v2_2026-08-14.md`, SHA-256 `3d13b55a…7b2` (same document as the earlier reports) |
| Accepted deviations | [CampaignV2ChangeLog.md](../CampaignV2ChangeLog.md); ADS removal on 16 September (`36e1dfb6`) |
| Source | 698 project C++ files, ~135k lines, plus the customized Narrative plugin where the game depends on it |
| Reviews | Five independent domain reviews that read their TDD sections against the code. The lead then independently re-read the source behind every P1 before accepting it (see the P1 table) |

The reviews read source only. `.uasset`/`.umap` files are binary, so content conclusions come from asset listings and scans for referenced class and tag names, not from Blueprint graphs. Findings are labelled as source-proven defects, missing integration, or items that need runtime evidence. A plausible hazard is not reported as an observed bug.

## Alignment estimate

Domain weights are the same as in the 6 and 11 September reports. Companions are scored with enemies and AI here rather than with campaign; they carry little weight either way.

| Domain | Weight | Range | Midpoint | Report |
|---|---:|---:|---:|---|
| Combat, protagonists, movement and AI (player combat 60%, enemies/AI/companions 40%) | 30% | 40–54% | 47% | [PlayerCombatAudit.md](PlayerCombatAudit.md) (42–56, mid 49); [EnemyAICompanionAudit.md](EnemyAICompanionAudit.md) (38–50, mid 44) |
| Campaign, narrative, progression, world | 35% | 24–33% | 28% | [CampaignNarrativeWorldAudit.md](CampaignNarrativeWorldAudit.md): engineering ~60%, authored content ~8% |
| HUD, accessibility and localization | 10% | 38–50% | 44% | [UIAccessibilityPresentationAudit.md](UIAccessibilityPresentationAudit.md) |
| Art, animation, VFX, audio, cinematics | 10% | 17–30% | 23% | same report; the engineering side alone is 35–48% |
| Runtime architecture and save contracts | 8% | 50–65% | 57% | [ArchitectureSaveBuildQAAudit.md](ArchitectureSaveBuildQAAudit.md) |
| Asset pipeline, platforms, production and QA | 7% | 22–35% | 28% | same report |
| **Weighted** | 100% | **31.6–43.3%** | **37.1%** | |

These are planning judgements, not statistical intervals. For comparison, the separate M12–M13 slice rubric was at about 69/100 on 13–15 September; that rubric uses a different denominator.

## P1 findings

Every P1 below was re-read in source by the lead. "Confirmed" means the defective path is present as described; it does not mean the failure was reproduced at runtime.

| ID | Finding | Evidence | Lead check |
|---|---|---|---|
| PC2-01 | Tarrik and Selene attack through Narrative Blueprint combos (`WI_Velkorran` → `GA_Attack_Melee_Sword_*`, `WI_Verity` → `GA_Attack_Combo_Melee`). Only four Aurelion enemy abilities use `USovGameplayAbility_Melee`. The player therefore gets none of the socket sweeps, hit ledger, cover check, input buffer, charged release or defensive cancels. Tarrik's "heavy hits 3+ targets" Echo reward cannot fire, and the native melee tests exercise a path the player never takes. | `SovEchoAttackReceipt.cpp:7`; only caller `SovGameplayAbility_Melee.cpp:296`; check at `SovTarrikEchoGenerationComponent.cpp:911` | Confirmed by asset reference scan |
| EA2-01 | Summoned Elite adds are destroyed only when the retry is prepared, never on victory or failure. Adds summoned in the link phase survive the save-safety check and the Tarrik handoff; adds summoned in the thermal phase survive victory and the autosave on arena exit. | `SovEncounterDirector.cpp:962` (sole caller of `CleanupAttemptActors`), `:489-528`; grant in `Scripts/Editor/setup_aurelion_enemy_roles.py:641-648` | Confirmed |
| CN2-01 | With a persistent save failure, M12 is blocked for good. Cinematics, the priority terminal, M13 travel and checkpoint terminals all require a successful checkpoint write and ignore the player's acknowledged "continue without saving". Arena entry already honours it. | `SovCampaignCinematicComponent.cpp:555`; `SovAurelionPriorityTerminal.cpp:170`; `SovPlayerController.cpp:771`; compare `SovEncounterDirector.cpp:441` | Confirmed |
| UX2-01 | A bark that suspends dialogue gets no subtitle: the cue subtitle is dropped whenever any dialogue exists, so the paused NPC line stays on screen under the wrong speaker. (Old UI-01, still open.) | `SovFrontendComponent.cpp:333` | Confirmed |
| UX2-02 | The default holographic HUD draws above the subtitle and caption layer (depth 50 vs −1), uses the full screen and ignores the safe zone. By layout arithmetic, the Echo arc and radar cross the last subtitle line, and at UI scale 1.25 or more the name plate covers critical captions. | `SovFrontendComponent.cpp:60,79`; `SovHolographicHUDWidget.cpp:215-222,584-644`; `SovAccessibilityPresentation.cpp:422` | Draw order confirmed; overlap needs one capture with a subtitle on screen |
| AR2-01 | A corrupt local save reaches engine deserialization after only an 8-byte header check. Unbounded string lengths and file-chosen class names are decoded before the checksum. Every menu or recovery path that lists saves would hit the same file again. | `SovSaveSubsystem.cpp:58-70`, checksum at `:495`, no size cap at `:538` | Missing bound confirmed; crash mode needs a fuzz run |
| AR2-02 | Packaged builds boot Narrative's template: vendor main menu map, game mode and game instance. The 15 September M12 package stages Narrative demo worlds, character creator and demo quests (10,199 of 20,840 files). | `Config/DefaultEngine.ini:93-96` | Config confirmed |
| AR2-03 | No executable gate qualifies Test/Shipping, cook or data validation. `Validate-Unreal.ps1` builds only Development and counts passed-with-warnings as passes. The campaign commandlet requires M01/M02 assets that don't exist, so the slice cannot pass it. There is no CI. | `Scripts/Validate-Unreal.ps1:193-205,250`; `SovValidateCampaignCommandlet.cpp:359-360` | Confirmed |

**Counts:** 8 P1, 47 P2, 31 P3. P2/P3 details, failure sequences and recommended fixes (inside existing owners) are in the domain reports. Recurring themes among the P2s:
- **Built but unreachable:**
  - Technique progression, the viewmaker scan, hard lock-on, companion FocusTarget/DefendPerson, cinematic skip/pause and the Mass tier have no production caller.
  - Marked-target and command-target states are never applied.
- **Checkpoint semantics:**
  - Death retry replays arena wins and canon gates, and can send an M13 death back to M12.
  - Saves have no playthrough identity.
  - Any mission-definition revision invalidates every save.
- **Encounter integrity:**
  - A required enemy destroyed without dying still blocks victory (old C03).
  - Summons bypass coordination and the enemy budget.
  - Killing the Elite with ordinary damage fails the encounter.
- **Player comfort and access:**
  - The permanent "in encounter" flag drains sprint stamina out of combat.
  - The Story/accessibility defence window doesn't reach guard or deflection.
  - Colour-vision presets don't reach the HUD, radar or threat cards.
  - Remapping isn't reachable from the slice's menus.
  - There is no localization pipeline and no tutorials.

## Prior findings (5 September), re-verified

45 findings: **26 fixed, 6 partially fixed, 13 still open.**

| Status | Findings |
|---|---|
| Fixed | PC-01 (in the native melee, which the player doesn't use; see PC2-01), PC-02, PC-03, PC-04, PC-05; ED-01–07; C01, C02; SP-01, SP-03; UI-02, UI-03, UI-04, UI-06, UI-08; B1–B5 |
| Partially fixed | PC-06 (only one of seven reward histories capped), C05, SP-05 (see AR2-01), UI-07, V02, V03 |
| Still open | PC-07, PC-08, C03 (EA2-04), C04 (CN2-01), C06, C07, SP-02 (P2 on desktop, P1 on console), SP-04, UI-01 (UX2-01), UI-05, B6, V01, V04 |

## Executed evidence (17 September)

| Check | Result | Meaning |
|---|---|---|
| `Validate-Unreal.ps1 -DisableAura`, Development editor build | Exit 0 | Compiles, including the local NVIDIA plugins |
| Native automation, filter `ProjectVelkorran` | **682 passed, 1 failed** of 683 | The failure is `Campaign.Aurelion.SecurityStandOff.TreeOnlyTemplateDelta`. `Content/Aurelion/Enemies/BT_AurelionSecurityCrossfire.uasset` was re-saved in the Behavior Tree editor at 17:24 on 16 September (uncommitted) and now carries an editable graph (`BTGraph`), which the test deliberately rejects (`SovAurelionStandOffAuthoringLibrary.cpp:172`). The committed asset has no graph, and 683/683 passed on 16 September. This is a local-content failure, not a code regression. Revert the asset or re-author it through the script. |
| `python -m unittest` over `Scripts/Tests` | Exit 1: 119 run, 1 error, 11 skipped | `TestModuleIsolation` is stale: it rejects the 7 tests that correctly live in the editor module. Skips need symlink rights or a host compiler |
| `Check-ConsoleBuild.py` | Exit 1, 30 blockers | 4 Narrative runtime modules exclude consoles. With the engine plugin root it exits 2, because its strict JSON parser rejects 36 engine `.uplugin` files that contain trailing commas |
| `Test-NativePolicies.py` | Not run | No host g++/clang++ on this workstation |

Logs: `Saved/Validation/Audit-20260917/20260917-082801-cf1040d0/`. Not performed: Test/Shipping builds, cook or package, a fresh route run, performance capture, console builds, user research.

## Where the points are

Code alone could plausibly add 5–8 whole-TDD points by fixing the P1s and connecting what is already built:
- Move the protagonists onto native melee (or port its guarantees into their combos).
- Wire progression, lock-on, the viewmaker and companion commands.
- Fix checkpoint and retry semantics.
- Give the project its own boot map, game mode and game instance, and stop cooking the Narrative demo content.
- Add a Shipping/cook gate.

The remaining ~55 points need what code cannot supply:
- 15 more missions, hubs, prologue and epilogue
- authored dialogue and VO
- art, animation and music
- recruited accessibility and player testing
- console hardware and certification

## P1 repairs, 17 September

Every P1 was repaired the same day, each with a regression test and, where the defect was visible, a capture
or a play session. The estimate above is unchanged: it was not recomputed after these repairs.

| ID | Commit | What changed | Verified by |
|---|---|---|---|
| PC2-01 | `0d08cdf4` | Tarrik and Selene attack through native melee graphs authored over their existing montages, with a heavy attack each. A node can offset its sockets and sweep extra blade edges (Velkorran has no tip socket; Verity is a double blade) | Two M12 play sessions: both light chains advance through four nodes and land hits from distinct attacks, both heavy attacks land heavy-classified hits, all from the native abilities. Content test that the weapons grant them |
| EA2-01 | `cc30b7c8` | Resolving an encounter retires its attempt-scoped fighters: a victory destroys them before success is published, a failure suspends them until the retry removes them | Runtime test over both outcomes |
| CN2-01 | `a16bac9c` | One `EnsureCheckpointBoundary` for every pre-boundary checkpoint writer, accepting the acknowledged boundary once; travel recovers from the snapshot the player continued past | Runtime test with failing storage, including wrong boundary and wrong kind |
| UX2-01 | `16a1f8f1` | A suspended conversation no longer owns the speech surface; the bark is subtitled and the displaced line returns on resume | Frontend integration test through the real cue and Tales producers |
| UX2-02 | `94ade26b` | `SovHolographicHUDLayout` describes where the HUD draws; it paints inside the text safe area, beneath the text, and subtitles, captions, objectives and threat cards avoid its areas | Layout test across four safe sizes, UI scales 1–2 and text scales 1 and 2.5; M12 captures at UI scale 1 and 2 |
| AR2-01 | `e6729a02` | Stored envelopes carry a magic, length and CRC checked on raw bytes, with a size limit; decoding bounds string sizes and never loads a class named by the file | Corruption suite, including a frame-length and an inner-length attack |
| AR2-02 | `7e82c9cb` | Packaged builds boot into M12 on the campaign's own game mode; the Narrative entry map and character creator no longer point at demo content | A Shipping package of M12/M13 stages 368 Narrative demo files against the 15 September package's 2,161, and 6,934 plugin files against 10,199 |
| AR2-03 | `7e82c9cb` | `Validate-Campaign.py --release` builds Shipping, packages it and fails on staged Narrative demo or template content; `--fail-on-warnings` and `-FailOnWarnings` reject warning-only passes; `--slice` lets the commandlet validate M12–M13 | First Shipping build and first Shipping package this project has produced; the staged scan then failed on the 368 files above, as it should |

**What the repairs did not close.**

- **Prohibited content still reaches a cook through campaign content, not boot config.** With M12 as the boot
  map the commandlet names each chain: `BP_SovPlayerController` → Narrative loot menus;
  `BP_AurelionPlayerController` → the failure menu → Narrative's main menu map → its multiplayer menu; the
  Tarrik protagonist Blueprint → the demo player definition (demo rifle ammo, grenade). That is Blueprint
  content work, and several of those assets are not in version control. This is the old T3 gate. A Shipping
  package still stages 368 such files (368 of them `/Pro/Demo/`, plus the loot, main-menu, multiplayer-menu and
  character-creator entries), down from 2,161 demo files on 15 September.
- **The cook needs two known options.** `-NeverCookDir` for the two MetaHuman authoring directories whose
  plugins this project restricts to the editor, and the groom system both solvers load dynamically. Without
  them the cooker fails, as it did on 6 September. The gate passes what `Cook-CombatPlaytest.ps1` already did.
- **Test configuration.** An installed (launcher) engine cannot build `Test`. The gate records that rather
  than skipping it silently; qualifying `Test` needs a source engine.
- **The campaign commandlet reports 166 errors on the slice**, mostly missing `/Engine/USDImporter/Transient/…`
  packages recorded inside imported Kitbash materials, plus missing string-table entries for both mission
  names. Those are content issues the gate now surfaces.
- **PC2-01's melee graphs are provisional tuning.** Windows follow measured blade-tip speed, but damage,
  poise and cancel costs are first values, and Narrative's motion-warping lunge does not apply to the native
  path.
- **UX2-02 at 720p with UI scale 2 and text scale 2.5**: a worst-case three-line subtitle cannot fit between
  the identity plate and the Echo arc. It stays clear of the arc and radar.
- **AR2-01 legacy envelopes.** Saves written before the frame are still accepted unframed, and are framed on
  their next write.

## Suggested order

1. **Integrity P1s:**
   - CN2-01: one shared acknowledged-boundary helper for every checkpoint writer.
   - EA2-01: clean up attempt actors on victory and failure.
   - AR2-01: bounded pre-decode validation of save bytes, plus a size cap.
   - UX2-01/02: bark subtitle arbitration; HUD layout that respects the subtitle band and safe zone.
2. **Wiring:** PC2-01 melee path for both protagonists. Then connect the built-but-unreachable systems listed above, each with a test through the real input or data path.
3. **Product boundary:** AR2-02 boot map, mode and instance; strip demo content from the cook; AR2-03 Test/Shipping build, cook and slice-aware commandlet in `Validate-Unreal.ps1`; count warnings as failures.
4. Refresh the slice rubric with a fresh unattended M12→M13 route after 1–3.
