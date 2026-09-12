# Away engineering log — from 11 September 2026

Running record of autonomous engineering while the creator is away from the development
machine. Baseline: `origin/main` at `9bfb44e1`. Work branch: `engineering/away-pass-20260911`.

Baseline invariants to protect, verified at branch creation:

| Invariant | Baseline state |
|---|---|
| Editor + Game targets build | ✅ both succeed |
| Packaged Aurelion cook | ✅ 6.28 GB archive |
| Packaged M12 boot | ✅ loads in ~2.5 s |
| Automation suite | ✅ 600/600 |
| Aurelion runtime behaviour | ✅ full cast, Tarrik kit, accessibility at boot |

---

## Priority 1 — Commander-death sever regression

**Status: fix implemented, regression coverage added, verification in progress.**

### Trace against current main (before changing anything)

The defect is real and still present at `9bfb44e1`. Path:

```
HandleParticipantDeathStateChanged   (source died)      -> DeactivateWithoutSever()
HandleParticipantDestroyed           (source destroyed) -> DeactivateWithoutSever()
  ... plus the two deferred variants, which set bDeferredDeactivateRequested and
      reach the same function through ProcessDeferredMutation()
```

`DeactivateWithoutSever()` unconditionally set `State = Inactive` and called
`ClearAllParticipantContributions()`. When the link was **already Severed**, that:

1. withdrew the `State.CommandLink.Severed` tag from every living participant,
2. broadcast a `Severed -> Inactive` state change that never actually happened, and
3. returned the link to a state from which `ActivateCommandLink` + `TrySeverCommandLink`
   could succeed again — a second Echo reward for one authored link.

### Fix

Smallest correct change, two edits in `SovCommandLinkComponent.cpp`:

1. **Guard inside `DeactivateWithoutSever()`** — return early when the link is already
   `Severed`. Placed in the function rather than at the four call sites so the two
   *deferred* source-loss paths are evaluated when they run, not when they were requested.
2. **Retire the dead source's own contributions** in the death branch before attempting
   deactivation, so a dead commander does not keep a live tag count while survivors
   correctly retain theirs. `RemoveParticipantContributions` is idempotent
   (`Set.Remove(...) > 0` guarded), so the unsevered path that clears every participant is
   unaffected.

Deliberately **not** changed: sever receipts, `LastSeverTransactionId`, Echo eligibility,
phase behaviour, encounter epoch isolation, restore/checkpoint behaviour, or any
Narrative Pro code.

### Regression coverage

`SovCommandLinkSourceLossTests.cpp`, four suites:

| Test | Asserts |
|---|---|
| `CommanderDeathPreservesSeveredState` | survivors keep Severed; dead commander retires its own; receipt survives |
| `CommanderDeathPreservesSeverIdempotence` | repeat sever returns `AlreadySevered` with the original transaction id |
| `CommanderDestructionPreservesSeveredState` | same through the `OnDestroyed` delegate |
| `CommanderDeathStillEndsActiveLink` | an **unsevered** link still deactivates — guards against over-fixing |

The fourth test matters most: it is the one that would fail if the guard were too broad.

---

### Verification

Full suite **604/604** with the guard in place. A **negative control** run with the guard
disabled produced `passed=1 failed=3`:

| Test | Without the guard |
|---|---|
| `CommanderDeathPreservesSeveredState` | FAIL — state 0 (Inactive) not 2 (Severed); survivor tag 0 not 1 |
| `CommanderDeathPreservesSeverIdempotence` | FAIL — resolution 1 (Inactive) not 4 (AlreadySevered); new transaction id |
| `CommanderDestructionPreservesSeveredState` | FAIL — same through `OnDestroyed` |
| `CommanderDeathStillEndsActiveLink` | **PASS** — unsevered path unaffected |

Three failures and one pass is the discriminating result: the tests detect the defect, and
the guard is scoped to severed links only. The idempotence failure is the clearest
statement of the real risk — without the guard a repeat sever returned a *new* transaction
id, so one authored link could pay two Echo rewards.

Committed as `d4e24c4a`.

---

## Priority 2 — Campaign forbidden-content validation

**Status: implemented and verified. Two of the listed concepts do not exist to detect.**

### Verified coverage before changing anything

| Appendix F concept | State at `9bfb44e1` |
|---|---|
| Crafting, Vendors, Morality, Rarity | Covered — commandlet `LegacySegments` path rules |
| XP | Covered — `GE_GiveXP`, `NE_GiveXP`, **plus** an XP-attribute modifier check on GameplayEffects |
| Currency | Covered — `BPE_AddCurrency` |
| Multiplayer menu | Covered — `W_NarrativeMenu_MPMainMenu` |
| **Loot** | **Missing, and present in project content** |
| **Class** | **No class system exists anywhere** |
| **Approval** | **No approval system exists anywhere** |

This corrects an earlier audit of mine that reported XP, currency and multiplayer-menu
content as gaps. They were already covered at asset level; I had only read the commandlet's
path-segment list.

**Class and approval are not implemented anywhere** — zero assets and zero source in both
the project and the fork. The `NC_*` assets that match a keyword search are Narrative
*Conditions* (`NC_IsQuestFailed`, `NC_IsDayTime`), and `DialogueBlueprintGeneratedClass` is
engine plumbing. Writing detection for either would be a speculative keyword ban with real
false-positive risk and nothing to catch. **Documented as not-applicable rather than
invented.** This is a design branch point: if a class or approval system is ever authored,
the rule table is where detection belongs.

### Loot economy — implemented

The project carries three looting widgets under
`Content/UI/Narrative/Menus/Inventory/Loot/`: `W_NarrativeMenu_Looting`,
`WBP_Loot_TheirInventory`, `WBP_Loot_YourInventory`. The fork also ships `BP_LootableChest`,
`Interactable_Loot` and `DT_LootChest`.

Matched by **authored name and class**, never by the word "loot". `FLootTableRoll` is the
framework's ordinary grant struct and is how `IC_Tarrik` and `IC_Selene` equip the
protagonists — a keyword rule would reject the campaign's own fixed-equipment path. The
suite asserts that accept-case explicitly.

### Demo/template dependency detection

Scoped to `/Pro/Demo/Items/` rather than all of `/Pro/Demo/`. Demo VFX
(`NS_SmokePuffLight`) appears in the cook, and whether it is reachable *from campaign
content* has not been measured. Broadening the rule needs a dependency-walk measurement
first — recorded as a follow-up rather than guessed.

### Testability change

`ProhibitedSystemReasonForName` now exports the existing name+class rule table. Three of the
prohibited loot assets are widgets and **`UUserWidget` is abstract**, so they cannot be
instantiated — the rules could not be tested by fabricating assets at all. The export is a
thin wrapper; no logic is duplicated and `ProhibitedAssetReason` still uses the same table.

### Two self-inflicted failures worth recording

1. **Fatal name collision.** The first version of the loot test created
   `W_NarrativeMenu_Looting` twice in `/Engine/Transient` (once per class, to prove class
   discrimination). Two objects sharing a name in one outer is an `appError` that kills the
   automation process — exit code 3, not a test failure.
2. **Abstract class instantiation.** After fixing the collision, `NewObject` on
   `UUserWidget` still ensured: *"Class which was marked abstract was trying to be loaded"*.
   The fabrication approach was unusable for this rule table, which is what prompted
   exporting the predicate.

Neither touched the baseline: both were confined to the test module, and Priority 1 was
already verified green beforehand.

---

## Priority 3 — GameplayCue discovery

**Status: implemented, after correcting a regression I introduced mid-task.**

### Trace

Enumerated cue notifies through the **asset registry by native parent class**, not by asset
name — a cue notify Blueprint can be called anything, and a name search could not rule out a
differently-named one inside 22 GB of marketplace content.

| Root | Cue notifies |
|---|---|
| `/Game/Cues` | 22 registry entries (11 assets) |
| `/NarrativePro/Pro/Core/Abilities/Cues` | 22 registry entries (11 assets) |
| **Anywhere else** | **0** |

No source in the project or the fork references `UGameplayCueManager`,
`AddGameplayCueNotifyPath`, `RemoveGameplayCueNotifyPath` or any cue notify base class, so
the set is entirely static and the brief's "do not force it if discovery is dynamic"
condition does not apply. Reproducible via `Scripts/Report-GameplayCueLocations.py`.

Config location verified in engine source rather than trusted from the warning text:
`UGameplayAbilitiesDeveloperSettings` overrides its section back to
`[/Script/GameplayAbilities.AbilitySystemGlobals]` in `DefaultGame.ini`.

### The regression, and the correction

Finding cues in two roots is **not** a reason to scan both. Configuring both produced nine
`AddGameplayCueData_Internal ... Skipping` collisions, resolving **inconsistently**:

```
Character.Invulnerable : fork skipped, /Game wins
Character.Poisoned     : /Game skipped, FORK wins
Character.Invisible    : fork skipped, /Game wins
TakeDamage             : /Game skipped, FORK wins
TakeDamage.Blocked     : /Game skipped, FORK wins
```

The project's `/Game/Cues` assets are forked copies carrying the same tags as the fork's
originals. The engine's previous fallback scanned `/Game/` alone, so the fork's copies were
never registered and the project's overrides always won. Adding the fork root registered
both and let load order decide — silently discarding some project overrides.

**Final configuration is `/Game/Cues` only.** It removes the fallback warning, bounds the
scan, and preserves the established resolution exactly.

### Measurement — no startup win claimed

Map load **3.071 s**, against 2.969 s and 2.472 s on identical content beforehand. That is
inside the run-to-run spread, so there is **no measurable improvement**. The benefit is that
the scan no longer grows with project content, and the warning is gone. Recorded here so no
one later cites this change as a performance gain.

This also sets a floor for Priority 4: with ~0.5 s of variance between identical runs,
single-sample comparisons cannot detect anything smaller. The harness must do repeated runs
and report spread.

### Guard

`SovGameplayCuePathTests.cpp` asserts the **resolved runtime value**, not the ini text, so a
config that parses but never reaches `AbilitySystemGlobals` still fails. It rejects a bare
`/Game` entry, asserts the fork root is *not* scanned, and counts tags into a `TSet` to
assert **zero duplicate tags** — so re-adding a colliding root fails the suite instead of
hiding in a startup warning. That last assertion exists only because the packaged
before/after comparison exposed the problem.

---

## Priority 4 — Intermittent AI startup stall

**Status: defect NOT reproduced under unattended automation. One deterministic precondition
measured for the first time. No fork modification made.**

### Instrumentation

`sov.AIStartupTrace` exists, in the **fork** at
`NarrativeArsenal/Private/AI/NarrativeAIStartupDiagnostics.cpp` — a `Source/`-scoped search
misses it. `ECVF_Default`, non-Shipping only, armed at `OnPreWorldInitialization`, capped at
120 s / 2000 events / 32 controllers.

**Arming in a packaged build requires `-dpcvars`.** `-ExecCmds="sov.AIStartupTrace 1"` sets
the variable — the log shows `sov.AIStartupTrace = "1"` — but runs *after* world
initialization, so the capture window has already closed and **zero events** are recorded.
`-dpcvars="sov.AIStartupTrace=1"` applies during engine PreInit and captures correctly.
Do not change the harness flag without re-verifying that events appear.

### Map selection — M12 is the wrong map

M12 captured 699 events, 34 controllers and **231 perception callbacks**, but:

```
callbacks targeting the PLAYER: 0
```

Every callback is an NPC seeing another NPC. Tarrik stands at the southern approach and no
hostile perceives him, so the race cannot be exercised. Twenty identical M12 runs would have
produced twenty identical null results.

The documented stall is a Selene/Hound encounter, and `/Game/Maps/Development/L_SeleneCombat`
exists and spawns **3 Dominion Hounds + 1 Handler**. That is the correct scenario.

### Result — 8 cold starts of L_SeleneCombat

| Measure | Result |
|---|---|
| Runs | 8, fresh `UserDir` each |
| Controllers per run | 4 (3 Hounds + Handler) |
| Perception callbacks | **0 in all 8 runs** |
| Player perceived | **never** |
| All generators initialize before faction publication | **8 of 8** |
| Window between last generator return and faction publication | **55.5 – 58.8 ms** |

Final snapshots explain the zero callbacks:

```
perception_active   = False
current_sight_count = 0
tree                = None
players.factions    = (GameplayTags=)      <- empty
players.attitude    = 1 (Neutral)
players.currently_seen = False
asc_ready_epoch     = 0
```

The Hounds' perception components are **inactive** and the controllers are not ticking — all
32 events occur inside the first 0.5 s and nothing follows for the remaining 45 s. These NPCs
stay dormant until something activates the encounter, which a human does by walking into it.

**So the stall is not reproducible without a driven player.** That is a statement about the
automation, not about the defect: the precondition (a hostile perceiving the player) never
occurs, so a null result carries no information about whether the race would fire.

### What was established, and it is new

**Every goal generator completes initialization 55–59 ms *before* the player's factions are
published, deterministically, in 8 of 8 runs.** At generator-initialization time the player
has empty factions and reads as attitude **Neutral** from the Hound's perspective.

That is the ordering precondition for hypothesis 2, and it is **deterministic rather than
intermittent**. It also explains *why* the observed stall is intermittent: the precondition
always holds, so the failure additionally requires a Sight event for the player to land
inside that ~56 ms window. Combined with the previously documented facts that
`GoalGenerator_Attack`'s only wake-up signals are a new Sight event and GameState's
`OnFactionAttitudeChanged`, that `ANarrativePlayerState::OnRep_Faction` broadcasts the
*player's* `OnFactionUpdated` instead, and that UE 5.7 `ProcessStimuli` suppresses same-state
Sight notifications, the model is a closed loop with no retry edge.

This upgrades the 6 September assessment from "candidate cause" to "precondition confirmed
deterministic; failure requires a Sight event inside a measured ~56 ms window".

### Not done, deliberately

No fork modification. The brief requires the failure reproduced or the causal ordering
demonstrated before touching plugin AI behaviour, and only the *precondition* is
demonstrated — the failing transition itself has not been observed. A fix would need to add
the missing retry/subscription edge so that generators re-evaluate when player factions
publish, and that belongs in fork Blueprint/source with the failure in hand.

**Blocker for further progress: requires a driven player.** Either a human playthrough, or
automated input driving Selene into Hound perception range. The latter is buildable but is
new automation rather than diagnosis, so it is recorded here as the decision point.

### Tooling added

`Scripts/Run-AIStartupColdStarts.ps1` — parameterised repeated cold starts with trace
arming and per-run extraction. Reusable for any future intermittent-startup investigation.

---

## Priority 4B — repair, and a retraction

**Retraction.** Priority 4's headline finding, "stall reproduced at 18/20", was **wrong**. It
classified each run by its last snapshot, and in 100% of cases a zero-goal terminal snapshot
occurred because the NPC had already killed Selene and correctly returned to spawn. Re-read
per controller over time, **80/80 baseline controllers acquired their attack goal and entered
`BT_Attack_DominionHound`**. Two supporting readings were also wrong: a `sight=False,
success=True` row is a non-Sight sense *succeeding*, not a sight loss, and it is the recovery
edge; and the claim that the loop had "no retry edge" was false.

**What the race actually costs.** Acquisition is delayed, not prevented:

| | Baseline | With repair |
|---|---:|---:|
| Acquired an attack goal | 80/80 | 80/80 |
| Median acquisition | 1.4064 s | 0.2285 s |
| Range | 1.4012–1.4242 s | 0.2241–0.3226 s |

The baseline recovers only when an unrelated sense fires (~1.41 s, strikingly consistent). The
repair closes the intended edge, so acquisition lands ~40 ms after faction publication.

**The repair** is documented in `AIStartupRaceReproduction-2026-09-11.md`: a new
`OnFactionMembershipChanged` signal on the game state, raised from `OnRep_Faction`, consumed by
`UNPCActivityComponent` under authority/perception/restore gates, dispatched to generators
through a new `ReevaluatePerceivedActors` hook that reuses the Blueprint's existing
`RefreshPerceivedActors` pass. Four fork files, purely additive, no fork content modified.
612/612 automation tests, with a negative control proving the two new-behaviour suites fail
against the defect.

**Still unexplained:** the 6 September observation of an NPC that never engages. Nothing in 40
packaged runs reproduces it. The delay fixed here is real but is not that.

**Method note.** The error was caught only by plotting goal count per controller over time
instead of reading a terminal snapshot. A single end-state sample cannot distinguish "never
started" from "finished and disengaged". Prefer a time series whenever the metric is the
*absence* of something.

---

## macOS as a second engineering environment

Work continued from a macOS checkout with UE 5.7. macOS is treated as a **second supported
engineering environment, not a Windows substitute**; full detail in
[MacEngineeringEnvironment-2026-09-11.md](MacEngineeringEnvironment-2026-09-11.md).

The away-pass branch was recovered intact from `origin/engineering/away-pass-20260911` at
`90d6c866` — three commits, `main` a clean ancestor, 2169 insertions and no deletions.

What was established here:

- The Mac editor target builds. Apple clang, Mac SDK 15.2. UBT resolves its own toolchain, so the
  host's broken `xcrun`/`xcodebuild` does not affect compilation.
- The Mac **Game** target compiles and links, which proves the runtime module carries no editor-only
  dependency. It does **not** prove a packaged Win64 build; that gate stands.
- 42 portable policy suites pass under Apple clang with UBSan.
- `Scripts/Validate-UnrealMac.py` is the Mac gate. It mirrors `Validate-Unreal.ps1`'s checks rather
  than a looser subset, and prints the outstanding Windows-only gates on every successful run so a
  green Mac run cannot later be misread as full validation.

### A Mac-only defect in the vendored fork's descriptor

The first full Mac suite run **crashed**, in `ReevaluationCannotDuplicateGoalsForSameTarget` — one of
this pass's own tests, which passes on Windows. The cause was not the test:

```
VerifyImport: Failed to find script package for import object 'Package /Script/NarrativeArsenalEditor'
Unable to load Weapon_Shield_Base ... because its class (WeaponItemBlueprint) does not exist
Fatal error: BlueprintGeneratedClass.cpp:691
UBlueprintGeneratedClass::GetAuthoritativeClass: ClassGeneratedBy is null
```

`NarrativePro.uplugin` sets `PlatformAllowList: ["Win64", "Android", "Linux"]` on seven of its eight
modules. Mac is absent, so the three `UncookedOnly` editor modules were never compiled — confirmed by
their absence from both `Binaries/Mac` and `Intermediate/Build/Mac`. Demo content importing
`/Script/NarrativeArsenalEditor` then loads a Blueprint class with a null `ClassGeneratedBy`, and the
Kismet compiler dereferences it while validating a cast node.

Adding `"Mac"` to those allow lists is **7 insertions and no deletions**, leaves Win64, Android and
Linux untouched, and cannot change Windows behaviour. All three modules then compiled on Mac first
try, and are registered in `UnrealEditor.modules`.

This is the one fork-descriptor change in the pass. It is platform enablement, not a behaviour change,
and no fork source was touched.

### Five suite failures that are a fresh-clone content gap, not a platform or code fault

The full Mac suite is **612 passed of 617, 5 failed**, reproducibly and identically across two runs.
All five new performance tests pass. The five failures are not Mac-specific and not regressions:

| Failing test | Requires |
|---|---|
| `Campaign.Cinematic.RequiredCharacterStillRefusesUnreadyOrRetiredState` | `/Game/SciFi_Drone_1/.../NPC_ReformationCombatDrone` |
| `Campaign.Cinematic.RequiredCharacterUsesActualNativeStartup` | same |
| `Campaign.PlacedNPC.AuthoredDefinitionPrecedesNativeASCStartup` | same |
| `Campaign.PlacedNPC.SpawnerAndRestoreDefinitionsOwnNativeStartup` | same |
| `Campaign.Validation.GameplayCuesStillResolve` | `/Game/Cues` |

Both paths are **absent from disk and untracked by git**, confirmed per asset. `.gitignore` tracks
only `Content/Aurelion/`, so no clone of this repository — Windows or macOS — has them. The Aurelion
assets those same files reference (`NPC_AurelionEnforcer`, `NPC_AurelionSecurityDrone`) are tracked,
present, and their tests pass, which isolates the cause to content availability rather than to the
suite or the host.

These tests passed on the work PC because that machine holds the untracked content locally. **The
612/612 Windows figure recorded earlier in this log was therefore obtained against content that is
not in the repository**, and it would not reproduce on a fresh Windows clone either. That is a
property of the repository, not of either platform.

One of the five, `GameplayCuesStillResolve`, was added by this away pass (Priority 3). It is
fresh-clone-fragile for the same reason, and that is this pass's own oversight rather than an
inherited one.

The Mac gate deliberately **still fails** on these. A suite that cannot load its assets has not
qualified the project, and reporting green would be false. It now prints each failing test with its
first error, and labels a failure as probable content absence where the report's own entries show a
missing package or object. The label explains; it never excuses, and there is no suppression list.

**Decision needed, not taken here** — this is a content policy question rather than an engineering
one, and it belongs to the project owner:

1. Track the required assets (they are small definition assets, not the marketplace art packs), or
2. give these tests an explicit content precondition that reports them as unrunnable rather than
   failed, or
3. accept that the suite is only fully green on a content-complete checkout, and record that as a
   standing gate.

Deliberately not done: silently skipping the tests, or adding them to an ignore list. Either would
turn a real gap into a permanently green suite.

#### Resolved

Classified and resolved in [ContentDependencyPolicy.md](ContentDependencyPolicy.md), with the
machine-readable form in `Scripts/Manifests/ContentPrerequisites.json`.

- **`/Game/Cues` — project-owned production asset.** The only configured `GameplayCueNotifyPaths`
  root, so a clone registers **zero** project cue notifies; an editor enumeration here finds 22 cue
  notifies, all in the fork root and none in `/Game/Cues`. Tracking is permitted — the repository
  already tracks 10,270 fork `.uasset` files including the eleven originals these override.
  `.gitignore` now admits the path; the test **stays Failed**; the cue configuration is unchanged.
  Closing it requires committing `Content/Cues` from the authoring machine.
- **`NPC_ReformationCombatDrone` — project-authored, not a production dependency.** It is an
  `NPCDefinition` instance, so no art pack shipped it; never tracked; referenced only by two test
  files. The authored campaign drones use `AC_NPC_ReformationDrone` directly, verified by loading
  them. The four tests now load the tracked `NPC_AurelionSecurityDrone`, so they exercise shipped
  production data and reproduce in any clone. No asset was committed; no third-party content added.
- **A content defect surfaced by that repoint.** `AC_NPC_ReformationDrone`, shared by both authored
  campaign drones, leaves the first two of its four `DefaultAbilities` entries `None`. The untracked
  fixture had been masking it. The tests skip unset entries, cannot pass vacuously (each requires at
  least one real grant), and warn naming the configuration. Whether those slots are meant to be empty
  is a content authoring question, left open.
- **Validation semantics.** Three states now exist: Passed, Failed, and Unrunnable /
  PrerequisiteMissing. A required production dependency's absence is **Failed**, never downgraded.
  `Check-ContentPrerequisites.py` refuses a manifest that softens a `project_owned` dependency, and
  the gate returns `NOT QUALIFIED` (exit 3) when a prerequisite is missing or any test is unrunnable
  **even when every executed test passed** — verified end to end.
- **Suite is now 616 passed, 1 failed, 0 unrunnable**, the single failure being the cue prerequisite.

One correction to the record: while checking this I read a run directory from *before* the ability-loop
fix and briefly took it for an order-dependent failure. It was not; the directories simply sort by
time and I picked a stale one.

---

## Priority 5 — Performance capture harness

Implemented. Full design and rationale in
[PerformanceCaptureEngineering.md](PerformanceCaptureEngineering.md).

Scope was chosen against what code can actually close. The TDD alignment review scores the
performance row as *not closable by code* — it needs console devkits. So this harness does not attempt
to produce a performance verdict for the project; it makes a capture repeatable, self-describing, and
hard to quote out of context.

| Piece | Verified by |
|---|---|
| `SovPerformancePolicy.h` — admission, percentiles, budget verdicts | `Tests/Portable/SovPerformancePolicyTests.cpp`, 60,856 checks, no Unreal build needed |
| `USovPerformanceCaptureSubsystem` — cvars, ticking, bounds, export | 5 automation tests under `ProjectVelkorran.Diagnostics.Performance` |

The decision worth carrying forward: **absence of data is never a pass.** A capture with too few
steady-state samples returns `Insufficient`, and an unusable budget returns `InvalidBudget`. Neither
is a pass, and `QualifiesCapture()` exists so a caller cannot accidentally treat one as an outcome.
That is the Priority 4 retraction encoded as a type rather than as a comment.

Every summary and every exported report records `platform`, `build_configuration`, `editor_build` and
`rendering_disabled`, plus a `scope` line stating the capture is local to that platform and is not a
console or certification capture. One automation test asserts `rendering_disabled` matches
`FApp::CanEverRender()`, so a capture taken under `-NullRHI` — as all automation is — declares that it
cannot qualify a frame budget.

### Negative controls

Each layer was shown to fail against an injected defect, and the controls were rerun after the fix:

| Defect | Result |
|---|---|
| Under-sampled capture treated as a pass | portable suite fails |
| Percentile floors instead of nearest-rank ceiling | portable suite fails — **only after coverage was added**; see below |
| Hard stall treated as exclusive rather than inclusive | portable suite fails |
| Warm-up frames admitted instead of discarded | 4 of 5 automation tests fail |
| `rendering_disabled` hardcoded false | exactly 1 automation test fails |

The percentile control initially **passed against the defect** — a real gap. Every explicit assertion
used a sample count of 100, where `Fraction × Count` is a whole number and ceiling equals floor, so
the rounding rule was never actually tested. The fix pins the rule against `std::ceil` for every size
1..200 and every percentile 1..100. Worth recording as the same failure mode as the Priority 4
retraction in a smaller form: a check that looked like evidence and was not.

### Two bugs in the Mac gate's own freshness check

`Validate-UnrealMac.py` adds a per-module binary freshness assertion the PowerShell gate does not
have, because a cook in this pass once consumed a binary that predated its source. The check itself
was wrong twice, both caught on real data:

1. It compared every binary against the *globally* newest source file, so an unrelated test edit
   marked plugin binaries stale. Now compared per module, against that module's own directory.
2. It then failed on any binary it could not map to a module — and a Mac Game build stages boost and
   tbb into `Binaries/Mac`. Now restricted to `UnrealEditor-*` module binaries, and it still fails on
   a module binary it cannot map.

Its negative control: touching one runtime source file names exactly
`Binaries/Mac/UnrealEditor-ProjectVelkorran.dylib` and nothing else.

---

## Priorities 6–8

Not yet started. Order per the revised brief: progression, pause/menu, fresh-clone audit.

---

## Method note

Two of the three completed priorities had a defect caught by verification rather than by
reasoning: the Priority 1 negative control, and the Priority 3 packaged comparison. Worth
retaining both habits — a regression suite should be shown to fail against the defect, and
any change touching startup or content resolution should be compared in a packaged build,
not only under the test suite.

---

## Known findings carried forward (not addressed in this pass)

- Three `/Game/Cargo/` textures report `contains no miplevels` at engine Error level.
- `GameplayCueNotifyPaths` unset — engine falls back to scanning all of `/Game/` (Priority 3).
- `BP_Planet_Volcanic` navmesh collision export at 261,120 triangles.
- Camera `FocalLength`/`FieldOfView` zero or negative, falling back to default.
- `BP_SovPlayerController::ReceiveBeginPlay` reads `GameplayHUD` and `LoadingMenu` before
  either exists. Diagnosed exactly: `EnsureGameplayHUDCreated()` runs on character
  readiness, strictly after controller `BeginPlay`. The remedy is to move that BeginPlay
  work onto the readiness hook — a Blueprint graph edit, so it needs the editor.
- `NPC_AurelionEnforcer` still grants `Weapon_DemoPistol`. Blocked on one authored rifle
  weapon item; clearing it would leave the Enforcer unable to shoot.
