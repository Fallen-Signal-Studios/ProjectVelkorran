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

## Priorities 5–8

Not yet started. Order per the revised brief: performance harness, progression, pause/menu,
fresh-clone audit. Each verified against the current tree before any change.

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
