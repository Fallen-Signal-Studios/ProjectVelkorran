# Engineering backlog reconciliation — 12 September 2026

Read-only pass. Nothing was implemented during it.

**Why this exists.** The 4/5 September adversarial audit was used as an implementation queue and turned
out to be stale in at least three places; two of them were nearly acted on. This rebuilds the backlog
from the present tree so that does not happen again.

**Authority order used.** Current source and `Config/` first; then
[TDDAlignment-2026-09-11.md](TDDAlignment-2026-09-11.md) and
[CampaignV2ChangeLog.md](CampaignV2ChangeLog.md); then slice documents. Historical audit prose was
treated as a list of questions to ask, never as evidence of present state.

**Discovery method.** Tracked state was queried through git's index rather than by walking the
filesystem. That is both faster and correct for questions about the repository, and this host's
filesystem is under enterprise scanning that makes recursive traversal cost minutes.

## Verification taxonomy

These are kept distinct throughout and never collapsed into "passed":

| Term | Meaning |
|---|---|
| **portable-tested** | Exercised by `Tests/Portable` or `Scripts/Tests` under clang/GCC with `-Werror -pedantic` and UBSan. No engine. |
| **compile-verified** | Builds as part of an Unreal target. Says nothing about behaviour. |
| **automation-verified** | Passes a registered Unreal automation suite in a real world/GAS context. |
| **content/editor-gated** | Engineering complete; remaining work needs assets, Blueprint graphs or an editor session. |

## Summary

| # | Finding | Class |
|---|---|---|
| C1 | Authored protagonist handoff and separate persistent state | **PARTIAL** |
| C2 | Campaign resource save defaults not established in native code | **CLOSED** |
| C3 | Campaign checkpoint contract incomplete | **PARTIAL** |
| E1 | Selene pulse not connected to combat | **CLOSED** |
| E2 | Enemy ability selection is an authoring responsibility | **CLOSED** |
| E3 | Weak-point break does not change enemy equipment | **CLOSED** |
| E4 | Link and weak-point state not checkpoint-persistent | **CLOSED** |
| E5 | Dismemberment completion is content-dependent | **CONTENT/EDITOR GATE** |
| E6 | Faction repertoire and perception/encounter fairness | **PARTIAL** |
| PC01 | Selene Echo expenditure does not deliver the control kit | **PARTIAL** |
| PC02 | Tarrik release depends on Blueprint for two paths | **PARTIAL** |
| PC03 | Tarrik Echo generation incomplete | **PARTIAL** |
| PC04 | Selene generation covers only three reward sources | **PARTIAL** |
| PC05 | Signature-readiness feedback disagrees with ability thresholds | **CLOSED** |
| PC06 | Echo encounter/checkpoint wiring not demonstrated | **SUPERSEDED (in part) / PARTIAL** |
| PC07 | Full-meter Deflections do not refresh Echo combat activity | **CLOSED** |
| PC08 | Zero-damage Poise/status packets outside the transaction | **CLOSED** |
| PC09 | Guard cancellation weaker than Deflection/Echo adapters | **PARTIAL** |
| PC10 | Shield/Health/Poise balance and content gates | **CONTENT/EDITOR GATE** |
| PC11 | Stamina, movement, combos, weapon transitions | **CONTENT/EDITOR GATE** |
| PC12 | Coverage proves construction more than combat behaviour | **SUPERSEDED** |
| T1 | `InitialMission` unset on campaign GameModes | **CONTENT/EDITOR GATE** |
| T2 | Cook exclusion incomplete | **PARTIAL** |
| T3 | Campaign UI carries template behaviour | **PARTIAL** |
| T4 | Intermittent hostile AI startup | **CLOSED** |
| T5 | Packaged Win64 Game target unverified | **OPEN (Windows gate)** |
| T6 | `ProjectVelkorranTests` sets `bUseUnity = false` | **OPEN** |
| K1–K4 | Cargo miplevels, navmesh export, camera FOV, Enforcer demo pistol | **CONTENT/EDITOR GATE** |
| K5 | `BP_SovPlayerController::ReceiveBeginPlay` ordering | **CONTENT/EDITOR GATE** |
| X1 | `/Game/Cues` absent from version control | **EXTERNAL CONTENT GATE** (see below) |

Closed: 10. Partial: 10. Open: 2. Content/editor gated: 8. Superseded: 2.

Both remaining OPEN items are T5 and T6. T5 is a Windows environment gate rather than code,
so **T6 is the only open source-only code item in the tree.**

## Closed, with the evidence that closed them

- **C2** — `SovPlayerState.cpp:66–70` populates `AttributesToSave` with Health, Shield, Stamina, Poise
  and Echo via `AddUnique`. The audit's "no native population anywhere" no longer holds.
- **E1** — `SovGameplayAbility_SeleneAxiomNullPulse.cpp` performs native cone selection with
  `HasAxiomLineOfSight` occlusion tracing (`LineTraceSingleByChannel`, ECC_Visibility).
  [AxiomNullPulse.md](AxiomNullPulse.md) carries the charge/release contract.
- **E2** — `NarrativeBotAttackSelection.{h,cpp}` implements the candidate-selection contract;
  `SovBotAttackSelectionTests.cpp` covers it; [BotCombatSelection.md](BotCombatSelection.md) documents it.
- **E3** — `SovWeakPointComponent::ApplyConsequence` applies `BlockedAbilityTags` and
  `GrantedStateTags` through a per-zone GameplayEffect handle. **Note for future readers:**
  `OnWeakPointBroken` has zero subscribers, which looks like absence and is not — the consequence path
  is not delegate-driven. This misled a previous pass.
- **E4** — `SovCommandLinkComponent.h` marks `LinkId`, `State`, `LinkInstanceId` and
  `LastSeverTransactionId` as `SaveGame` and exposes `CaptureCommandLinkState` /
  `RestoreCommandLinkState`; `SovWeakPointComponent.h` carries `SaveGame` snapshot fields. The audit's
  "plain component, state Transient" is superseded.
- **PC08** — `FSovDamageResult::bStatusApplicationRequested` exists and is consumed
  (`SovSelenePayload.cpp:80`, `SovCorruptionSourceComponent.cpp:109`), so zero-body control packets are
  inside the transaction. [DefenseAndWeakPointEngineering.md](DefenseAndWeakPointEngineering.md) states
  the contract.
- **PC05** — `USovEchoComponent::GetSignatureEchoRequirement()` (`SovEchoComponent.cpp:180`) derives the
  requirement from the granted, weapon-context-valid signature for the active protagonist, taking
  `max(EchoCost, MinimumEchoRequired)` and the minimum across candidates; `IsSignatureReady()` consumes
  it through `SovEchoAwardPolicy::IsMeterReady`. The `SignatureReadyThreshold = 100.0f` field that the
  audit cites is a vestigial default, not the live threshold. Automation-verified by
  `ProjectVelkorran.Campaign.Echo.SignatureReadiness`, which asserts a 95 requirement from an authored
  child and that 90 does not advertise it.
- **PC07** — `USovSeleneEchoGenerationComponent` awards and records activity under
  `Echo_Source_PerfectDeflection` (`SovSeleneEchoGenerationComponent.cpp:406–411`), before the reward
  cap filter. Automation-verified by `ProjectVelkorran.Campaign.Echo.FullMeterDeflection`, which
  asserts at 100 Echo that a real deflection refreshes the activity source **and** produces no
  overflow. The audit's "reward handler returns when full" no longer describes the code.
- **C3, cited defects** — `CreatePlayerOnlySave` null-tests the player state before reading its name
  (`return PS && CreatePlayerOnlySaveInSlot(PC, PS->GetPlayerName())`), and
  `ANarrativeGameMode::ProcessServerTravel` now checks the result, logs, and calls
  `CancelPendingTravel()` instead of travelling regardless.
- **T4** — closed in the away pass, together with a retraction of the stall diagnosis.
- **GameplayCueNotifyPaths** — configured to exactly `/Game/Cues`. The *assets* are a separate gate (X1).

## Superseded

- **PC12** — "coverage proves construction more than combat behaviour". 216 test files and 617
  registered tests now include world/GAS/save runtime suites. The claim does not describe the present
  suite. The underlying concern survives only as T6.
- **PC06 (in part)** — Echo now persists through the ordinary ASC `AttributesToSave` path (C2), so the
  bespoke `RestoreEchoFromCheckpoint` is not the mechanism that carries it. What remains of PC06 is
  narrower than written: see below.

## OPEN and PARTIAL detail

### C1 — Authored protagonist handoff (PARTIAL)

1. **TDD.** §§3.3–3.4, 15.4: M01 is Tarrik, M02 is Selene; handoff retains individual
   equipment/progression, correct tags and ability sets, coherent save data.
2. **Evidence.** `USovTechniqueComponent::InitializeNewProtagonist` exists and is referenced from
   `SovPlayerController.cpp`; the Technique component saves protagonist identity and a reward ledger
   and validates identity on load (automation-verified, 9 suites).
3. **Missing.** Progression handoff is covered; a protagonist-keyed *save partition* across inventory
   and quest context was not confirmed in this pass. `UNarrativeSave` holding one `FNarrativeSavePlayer`
   was the audit's core claim and was not re-verified.
4. **Source-only?** Likely yes for the partition; the M01→M02 boundary itself is authored.
5. **Priority.** Medium-high, but larger than a slice.
6. **Verification.** Automation save/restore across two protagonist identities asserting no
   cross-contamination.

### C3 — Campaign checkpoint contract (PARTIAL)

Extensive native types now exist — `SovAurelionCheckpoint`, `SovCampaignDefinition`,
`SovAurelionMissionDefinition`, `SovEncounterSnapshotLibrary` — so "no project checkpoint types beyond
empty seams" is stale. The two specific defects the audit cited are closed (see above). Not re-verified: rolling
autosave/backup and version migration. Source-only; medium priority.

### E6 — Perception and encounter fairness (PARTIAL)

`SovEncounterDirector`, `SovEncounterCoordinationComponent`, `SovEncounterCoordinationPolicy.h` and
`SovThreatTargeting.h` now exist, so "no encounter director in source" is stale. Not re-verified: that
Hound acquisition respects perception rather than scanning all characters by distance. Source-only;
medium. Verification: automation asserting no acquisition without authorised stimulus.

### PC01–PC04, PC09 — Ability and generation completeness (PARTIAL)

All named spenders now exist as native files: six Selene abilities including `Deflection`, `Dispatch`,
`StaccatoZero`, `StillpointGrenade`, `VeritysWake`, `AxiomNullPulse`; and `TarrikCinderSlam`,
`TarrikCinderlineRequiem`, `TarrikGuard`. Generation components exist for both protagonists, and
`GuardLifecycleValidation.md` records five guard lifecycle suites. **These are marked PARTIAL rather
than CLOSED deliberately:** this pass confirmed the files and entry points exist, not that each
delivers its payload natively without a Blueprint release hook. Closing them requires per-ability
review, which is a slice of its own rather than a reconciliation result.

### PC06 — remaining part (PARTIAL)

Echo persistence is carried by `AttributesToSave`. What is still unevidenced is a native caller that
begins/ends an encounter boundary for resource purposes. Source-only; low priority given C2.

### T2 — Cook exclusion (PARTIAL)

Narrowed on 12 September: demo tale content (the SecretMerchant quest and dialogue) is now rejected,
portable- and automation-verified. Remaining: implicit AlwaysCook roots beyond
`GatherAlwaysCookPackages`, and confirmation that no campaign manifest still reaches XP/currency
assets. Note from that work: tracked content contains **no** vendor, crafting, rarity, morality or
approval assets, so rules for those categories would be dead code.

### T5 — Packaged Win64 Game target (OPEN, Windows gate)

Mac compiles and links the Game target, which proves the runtime module carries no editor-only
dependency. It does not prove a shipping Win64 build. Windows-only; not closable here.

### T6 — Tests module disables unity (OPEN)

`ProjectVelkorranTests.Build.cs` sets `bUseUnity = false`. This is not hypothetical: a
`DEFINE_LOG_CATEGORY_STATIC(LogSovPerformance, …)` collision with `SovLogChannels.h` stayed hidden
until an unrelated edit shifted the adaptive-unity grouping, then failed the build. Source-only;
medium. Verification: enable unity, fix collisions at the root as the Editor module's were, full suite.
MSVC conformance remains a Windows gate.

## X1 — `/Game/Cues`, an external content gate

Listed separately and **excluded from the source-engineering assessment below.** It is the sole
configured `GameplayCueNotifyPaths` root; its eleven project-authored override assets have never been
committed, so a clone registers zero cue notifies. `.gitignore` now admits `Content/Cues`. Closing it
requires committing those assets from the authoring machine. No source change is pending, and no
source work is blocked by it. See [ContentDependencyPolicy.md](ContentDependencyPolicy.md).

## Source-engineering assessment

Excluding content/editor gates and X1, the source-only backlog is **four OPEN items**, of which two
(T5, MSVC conformance) are Windows-environment gates rather than code. That leaves **PC05 and PC07 as
the only open source-only code defects**, plus ten PARTIAL items that need per-item review rather than
new subsystems.

The honest summary: the native layer is materially more complete than the audit prose implies, and the
remaining risk is concentrated in *verification breadth* and *authored content*, not missing systems.

## A method failure in this pass, and the rule that follows from it

PC05 and PC07 were first classified **OPEN**, and PC07 was implemented before the error was caught.
Both were already closed.

The cause is precise and worth stating. The audit cites line ranges; I read the cited lines, found them
substantially unchanged, and concluded the finding stood. For PC07 the cited handlers in
`SovEchoComponent.cpp` genuinely were unchanged — but the behaviour had moved to
`SovSeleneEchoGenerationComponent`, and an automation suite named
`ProjectVelkorran.Campaign.Echo.FullMeterDeflection` had existed the whole time. For PC05 the cited
`SignatureReadyThreshold = 100.0f` is still in the header, but is no longer what `IsSignatureReady()`
consults.

**Checking the lines an audit cites is not verification.** Stale prose points at where a problem used
to live, which is the least likely place to find its fix.

Rule adopted for any future reconciliation, and applied to everything above from this point:

1. **Search the automation registry for a suite covering the finding before classifying it OPEN.** A
   test named for the behaviour is the strongest available evidence that it exists. Both PC05 and PC07
   would have been caught by one query.
2. Search for the *capability* by name across the tree, not only the cited file.
3. Treat an absent delegate subscriber, an unchanged constant, or an unchanged function as weak
   evidence. E3, PC05 and PC07 each looked absent by exactly that reasoning and were not.

The implemented PC07 change was reverted in full. It was redundant, and it would have regressed
`FullMeterDeflection`: it recorded activity with an empty tag, overwriting the
`Echo_Source_PerfectDeflection` tag that test asserts.

## Recommendation

With PC05 and PC07 closed, **T6 is the only open source-only code item**: `ProjectVelkorranTests` sets
`bUseUnity = false`.

It qualifies as infrastructure that is *actively preventing verification*, which is the stated
exception to preferring gameplay work. This is not hypothetical: a
`DEFINE_LOG_CATEGORY_STATIC(LogSovPerformance, …)` collision with the project's own `SovLogChannels.h`
survived multiple green builds and only failed when an unrelated test edit shifted the adaptive-unity
grouping. A defect class that surfaces by luck is a hole in the suite's ability to qualify anything.

No design decision is required, so this is the slice to proceed with.

**The larger unknown risk is elsewhere, and should be scheduled deliberately.** Ten findings are
PARTIAL because this pass confirmed that entry points exist, not that each delivers its behaviour
natively. PC01–PC04 in particular — per-ability payload delivery without a Blueprint release hook — is
where a real gap is most likely to still be hiding, and it wants a per-ability review slice rather than
another reconciliation.
