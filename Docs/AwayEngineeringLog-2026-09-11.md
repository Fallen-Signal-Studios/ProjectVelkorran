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

## Priorities 3–8

Not yet started. Order of attack follows the brief; each will be verified against current
main before any change, per working rule 1.

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
