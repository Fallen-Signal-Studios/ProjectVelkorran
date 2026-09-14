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
| C1 | Authored protagonist handoff and separate persistent state | **CLOSED** (12 Sep, see updates) |
| C2 | Campaign resource save defaults not established in native code | **CLOSED** |
| C3 | Campaign checkpoint contract incomplete | **CLOSED** (12 Sep, see updates) |
| E1 | Selene pulse not connected to combat | **CLOSED** |
| E2 | Enemy ability selection is an authoring responsibility | **CLOSED** |
| E3 | Weak-point break does not change enemy equipment | **CLOSED** |
| E4 | Link and weak-point state not checkpoint-persistent | **CLOSED** |
| E5 | Dismemberment completion is content-dependent | **CONTENT/EDITOR GATE** |
| E6 | Faction repertoire and perception/encounter fairness | **CLOSED** (12 Sep, see updates) |
| PC01 | Selene Echo expenditure does not deliver the control kit | **CLOSED** (12 Sep, see updates) |
| PC02 | Tarrik release depends on Blueprint for two paths | **CLOSED** (12 Sep, see updates) |
| PC03 | Tarrik Echo generation incomplete | **CLOSED** (12 Sep, see updates) |
| PC04 | Selene generation covers only three reward sources | **PARTIAL** |
| PC05 | Signature-readiness feedback disagrees with ability thresholds | **CLOSED** |
| PC06 | Echo encounter/checkpoint wiring not demonstrated | **SUPERSEDED (in part) / PARTIAL** |
| PC07 | Full-meter Deflections do not refresh Echo combat activity | **CLOSED** |
| PC08 | Zero-damage Poise/status packets outside the transaction | **CLOSED** |
| PC09 | Guard cancellation weaker than Deflection/Echo adapters | **CLOSED** (source, 12 Sep, see updates) |
| PC09-C | Guard/counter authoring: grant, input, counter classification, presentation | **CONTENT/EDITOR GATE** |
| PC10 | Shield/Health/Poise balance and content gates | **CONTENT/EDITOR GATE** |
| PC11 | Stamina, movement, combos, weapon transitions | **CONTENT/EDITOR GATE** |
| PC12 | Coverage proves construction more than combat behaviour | **SUPERSEDED** |
| T1 | `InitialMission` unset on campaign GameModes | **CONTENT/EDITOR GATE** |
| T2 | Cook exclusion incomplete | **CLOSED** (source/config/validation, 14 Sep, see updates) |
| T2-B | Template boot config still ships MP menu, loot UI, XP events and demo world | **CONTENT/EDITOR GATE** |
| T3 | Campaign UI carries template behaviour | **PARTIAL** |
| T4 | Intermittent hostile AI startup | **CLOSED** |
| T5 | Packaged Win64 Game target unverified | **OPEN (Windows gate)** |
| T6 | `ProjectVelkorranTests` sets `bUseUnity = false` | **CLOSED** |
| K1–K4 | Cargo miplevels, navmesh export, camera FOV, Enforcer demo pistol | **CONTENT/EDITOR GATE** |
| K5 | `BP_SovPlayerController::ReceiveBeginPlay` ordering | **CONTENT/EDITOR GATE** |
| X1 | `/Game/Cues` absent from version control | **EXTERNAL CONTENT GATE** (see below) |
| X2 | `SciFi_Drone_1` marketplace pack absent | **EXTERNAL CONTENT GATE** (see below) |

Closed: 18. Partial: 2. Open: 1. Content/editor gated: 11. Superseded: 2. External gates: 2.

_Recounted from the table above on 12 September after C1 closed, and again after C3, E6, PC09 and T2 closed. The earlier totals line did not
reconcile with its own rows. K1–K4 count as four gated items; PC09-C and T2-B count as one each; PC06 counts as superseded, its small
remaining part recorded under its own heading._

The single remaining OPEN item is T5, the packaged Win64 Game target, which is a Windows
environment gate rather than code. **There is currently no open source-only code defect in the
tree**, with PC04 the nearest thing to one and its gap being missing evidence rather than
missing implementation.

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

### C1 — Authored protagonist handoff (CLOSED 12 September)

1. **TDD.** §§3.3–3.4 (authored alternation, never free switching), §10.7 (inventory is signature
   equipment, augments, charges, mission items, evidence), §15.4 (initialization idempotent across
   handoff and restore), §15.9 (save holds campaign state and canon gates *separately from*
   protagonist progression/equipment).
2. **Boundary as implemented, established before testing.**
   - *Protagonist-owned*, keyed by protagonist tag in `ASovPlayerState::ProtagonistSnapshots`: pawn
     inventory, equipment and currency (each hero is a separate pawn; `FSovProtagonistSnapshot::PawnRecord`),
     the Technique ledger (one shared component, a per-hero `SkillTreeRecord`), factions, resources and
     wield slots.
   - *Shared by design*, on the controller: canon state values, mission records and completion, the
     journal, evidence records, and Narrative quest records.
   - *Partitioned inside shared storage*: `CharacterKnowledge` is keyed by protagonist. Evidence knowledge
     crosses only to a named witness or copy recipient.
   - *Transient*, cleared at handoff and never persisted: ability specs, active effects and
     definition-owned tags.
3. **Evidence — automation-verified on Mac.** `ProjectVelkorran.Campaign.ProtagonistPartition`, three
   tests driving the controller's own `StartPawnHandoff`, `StageCampaignLoad` and
   `PollCampaignInitialization` with two distinct pawn classes on one shared PlayerState:
   `RepeatedHandoffsKeepOwnedStateSeparate` (four round trips; owned state neither merges nor overwrites;
   transient state does not cross; canon facts shared, knowledge not), `SaveSwitchMutateReloadEitherProtagonist`
   (save, switch, mutate, save; each save reloaded in a fresh world, then handed off to the other hero),
   `MissingProtagonistRecordFailsClosed` (one hero's record is refused by the other's pawn; a save lacking
   the active hero's record fails explicitly rather than borrowing). With the Handoff, Companion and Save
   suites: **48 passed, 0 failed**.
   After the negative control was reverted, the full forced-unity suite gave **621 passed, 1 failed** of
   622; the one failure is `GameplayCuesStillResolve` (X1), identical to the prior stable baseline plus
   the three new tests.
   *Provenance of that run:* a concurrent session sharing this checkout rebuilt the test module at 16:28,
   before the run started. UBT recompiled only the unity blob holding `ProjectVelkorranTestsModule.cpp`,
   which carried an uncommitted diagnostic probe that stays inert unless `-SovActorEventProbe` or
   `-SovForceActorScript` is passed; the run passed neither. Every other blob, including the partition
   tests, was this commit's code. The 48-test run and the negative control were built entirely here.
4. **Negative control.** Skipping first-visit faction replacement and Echo reserve in the controller made
   all three tests fail, and the contaminated factions were visibly carried into later snapshots and both
   reloads. The controller was restored from git before the confirming run.
5. **Harness finding worth knowing.** Narrative's save dispatches the PlayerState's `PrepareForSave`
   through `AActor::ProcessEvent`, which silently does nothing in a world whose actors never began play
   unless `FEditorScriptExecutionGuard` is held. Without it the save appeared to lose the active hero's
   record. That was the harness, not the product: real game worlds initialize actors. Any runtime test
   relying on actor interface events without the guard is exercising less than it appears to.
6. **Not proven here, stated so it is not assumed.**
   - *Content-gated:* on a hero's **first** visit, Health/Shield/Stamina/Poise come from the authored
     `DefaultAttributes` effect. No production code resets them on the shared ASC; with no effect authored,
     the fixture observed the previous hero's values carrying over. Echo is reset natively and is proven.
   - Technique **perk grants** across heroes (the fixture has no authored branches; rewards and point
     ledgers are proven). Narrative quest records with real quest assets. Disk I/O (the real
     `UNarrativeSave` serialization is used in memory). `TravelToMission` map travel. M13 companion swaps
     are covered by the Companion suites instead.
   - Windows/MSVC remains a separate gate.

### C3 — Campaign checkpoint contract (CLOSED 12 September)

Work done in an isolated git worktree (`ProjectVelkorran-c3`, branch `engineering/c3-checkpoint-contract`),
not the shared checkout.

1. **TDD.** §11.9 (three rolling autosaves, one checkpoint slot, ten manual slots; never claim a save before
   serialisation and platform write both succeed), §15.9 (temporary-slot/atomic replacement where the
   platform permits, retain last known-good on failure, validate schema before applying, deterministic
   migrations with golden-file tests, migration history in the header), §15.16 (failed write keeps the old
   save; corrupted save offers last known-good and preserves the file), §18.5 (kill during write, denied
   write, load after migration, corrupted newest autosave, protagonist transition), §19.7 (schema 1.0 at the
   first external build; prototype saves unsupported, so no envelope migration exists to test).
2. **What the source implements** (read before any change). Two physical banks per logical slot with a
   generation counter and readback verification (the contract's replacement for an atomic rename, which
   Unreal's generic save API does not offer); rolling autosave selection by oldest generation in the
   subsystem tick; explicit recovery confirmation and byte-preserving recovery files; one payload
   migration, campaign state schema 1 to 2.
3. **Evidence — automation-verified on Mac.** `ProjectVelkorran.Campaign.CheckpointContract`, five tests
   through the production capture, autosave tick, writer, reader and decoder, staged through the controller
   as the game mode stages them, on C1's two-protagonist fixtures:
   `RollingAutosavesReplaceOldestAcrossRestart`, `InterruptedWriteRecoversLastGoodCampaign`,
   `SchemaOneCampaignMigratesThroughLoaderDeterministically`, `MissingActiveRecordStaysRejectedAfterMigration`,
   `CorruptOrIncompleteCampaignSaveIsRejected`. Outcomes are asserted on the stored bytes and on each
   protagonist's reloaded state, not on helper existence. Detail in [SaveSlotEngineering.md](SaveSlotEngineering.md).
4. **Defect found and fixed.** TDD §15.9 requires migration history in the save header.
   `FSovSaveSlotHeader::MigrationHistory` existed but nothing wrote it, so a campaign migrated from schema 1
   was re-saved with no record of the migration. The campaign state now records `CampaignState 1->2` when the
   migration runs and `CaptureAndWrite` copies it into the header. The test failed before the fix and passes
   after it.
5. **Negative controls.** Three temporary breaks, reverted from git before confirmation: autosave rotation
   always choosing slot 0 failed the rotation test; writes targeting the last good bank failed the
   interrupted-write and corruption tests; a migration that no longer advances the schema version failed the
   migration and missing-record tests. The C1 partition tests stayed green throughout.
6. **Suites.** With CheckpointContract, ProtagonistPartition, Objectives, Save, Handoff, Companion, Travel and
   the Aurelion pause UI: **68 passed, 0 failed.** Full forced-unity suite, run twice on the same build
   in the fresh worktree: **624 passed, 3 failed** and **625 passed, 2 failed**, of 627. In both runs one
   failure is `GameplayCuesStillResolve` (X1). Every other failure is the `SciFi_Drone_1` spillover (X2):
   `ABP_RefDrone`/`BS_Drone` errors from a deliberate drone-roster load landing on whichever `PlacedNPC` test
   runs next, a different one each run. `Campaign.PlacedNPC` run alone on that build passed 12 of 12. No
   failure involves the save, checkpoint or campaign-state code changed here.
7. **Not proven here, stated so it is not assumed.** Map travel and required-asset existence preflight (the
   fixture missions are transient objects); real platform write APIs and a process kill mid-write (the
   storage seam is in-memory, so tears are simulated at that seam); golden-file migration fixtures, which need
   stable asset paths and are therefore content-gated. Migration determinism is shown by repeated
   byte-identical output instead. Windows/MSVC remains a separate gate.

### E6 — Perception and encounter fairness (CLOSED 12 September)

Work done in the isolated worktree `ProjectVelkorran-c3`, branch `engineering/e6-perception-fairness`
(stacked on C3), not the shared checkout.

1. **Question.** Do hostile NPCs, especially Hounds and the Aurelion roster, acquire and pursue the player
   only through perception and awareness, or can any of them target an undetected player by raw distance?
2. **Authority.** TDD §8.3 (AI Perception for sensed stimuli; unaware → suspicious/investigating →
   acquiring → engaging), §8.5 (cloak breaks direct target confidence; enemies may suppress the last known
   area), §8.6 (sight, hearing, damage, ally alerts, network sensors for Reformation, Echo/corruption for
   selected units, command broadcasts, spoofing), §8.7 (the director owns composition and escalation, not
   individual attack timing).
3. **Target-acquisition architecture, as implemented.**
   - *Stimulus.* `ANarrativeNPCController::HandleThreatPerception` accepts Sight, Hearing and Damage from the
     controller's AI Perception component. Other producers (the Aurelion sweep scanner, Echo, commands,
     authored forced combat) call the Blueprint-callable `ReportThreatObservation` explicitly.
   - *Awareness.* Each observation carries a confidence capped by source (Sight 1.0, Damage 0.9, network
     sensor and Echo 0.8, Command 0.6, ally alert 0.55, Hearing 0.5), decays to zero at expiry, and keeps a
     last-known position. Cloaked targets and a perception component that is not sight-ready cannot create
     sight observations.
   - *Acquisition gate.* `CanDirectlyTargetThreat` requires an eligible hostile target and, for a
     threat-memory-managed controller, a direct observation (sight, damage, network sensor or Echo) at
     confidence 0.65 or more. Hearing, commands and ally alerts are investigation only.
   - *Aggro and combat request.* Narrative bot attack selection, `USovBTTask_UseCombatAbility`, the native
     Hound, Drone and Handler abilities, the Aurelion role activities and crossfire queries all consume that
     gate. Threat memory clears an attack target or focus that fails it and leaves the last-known position.
   - *Communication.* `ShareThreatWith` needs mutual friendly attitude, a shared faction, a finite radius
     (2,500 cm authored) and a managed recipient; alerts cannot be relayed, cannot outlive the original
     observation and never authorise direct fire. The Weaver shares only a threat it is directly tracking.
     The Aurelion sweep scanner reports only a player inside its cone and range with clear line of sight,
     only to its two registered relay drones that opt into network threats.
   - *Encounters.* The director and coordinator suspend threat memory for staging and restore; they never
     assign a target or report a threat.
4. **Distance-based fallback, classified.** The Hound attack abilities' `FindBestAttackTarget` scans the
   world for the nearest valid character when focus is invalid, and the Handler's Horn Charge uses it.
   Every candidate passes `SovThreatTargeting::CanTrack`, i.e. the controller gate. It is therefore an
   intentional combat-state fallback after legitimate detection (case 1), not a stealth bypass, for any
   managed controller. The gate has one documented exception: a controller with no perception component,
   no report and no `bRequireThreatMemoryForTargeting` keeps Narrative's legacy nonperception targeting.
5. **Content check (read-only editor probe).** Every tracked hostile resolves to a managed controller:
   Enforcer, Elite, Linkbound, WallRunner, Weaver and Security Drone use `BP_NarrativeNPCController`; the
   Contaminated Drone uses `BP_AurelionContaminatedDroneController`; `BP_DominionHound` and
   `BP_DominionHoundMaster` use `BP_NarrativeNPCController`. Each carries AI Perception with Sight (6,000 cm,
   lose at 7,000), Hearing (10,000 cm) and Damage. The legacy exception is unreachable for the authored
   roster, so no case-2 defect exists in current content.
6. **Evidence — automation-verified on Mac.** `ProjectVelkorran.Campaign.PerceptionFairness`:
   `RosterControllersRequireObservationBeforeAcquisition` spawns each roster enemy's *authored* controller
   class (resolved through its definition or pawn class) on a real combatant and asserts: managed; no direct
   target and no Hound bite against an undetected player 250 cm away; hearing records an investigation
   position only; no ally alert without any observation; a relayed noise and a relayed sighting are
   investigation only for the ally; sight authorises acquisition and the bite; losing sight revokes it and
   memory keeps the last-known position while the player moves.
   `EncounterActivationGrantsNoTargetWithoutAuthoredReport`: an active encounter grants nothing; a command
   broadcast is investigation only; an authored Damage report — the forced-combat override — authorises
   acquisition. Existing coverage retained: `Threat.PerceptionLossAndForgetting`,
   `Threat.SelectionCloakAndExpiry`, `Threat.FactionSharingAndLifecycle`,
   `Threat.NativeAbilityAcquisitionAndWindupLoss` and
   `Encounter.Coordination.ThreatSuspensionOwnersAndPawnReplacement`.
   Portable-tested: `Tests/Portable/NarrativeThreatPolicyTests.cpp`, 36,660 decay/sharing boundaries.
7. **Negative controls.** Two temporary breaks, reverted from git before confirmation: a 2,000 cm distance shortcut in `SovThreatTargeting::CanTrack`, and a 2,000 cm bypass inside the managed branch of `CanDirectlyTargetThreat`. Under both, 14 of 15 tests failed: every stealth assertion for all eight roster enemies (undetected target, Hound scan by distance, hearing, relayed alerts, loss of sight), the encounter-activation and command-broadcast assertions, and 12 of 13 existing `Campaign.Threat` suites.
8. **Suites.** With PerceptionFairness, Threat, AI, Encounter, Drone and Aurelion: **129 passed, 0 failed.** Full forced-unity suite in the worktree: **627 passed, 2 failed** of 629. The failures are `GameplayCuesStillResolve` (X1) and `PlacedNPC.OwnedEditorAssignmentPreservesMetadataAndRefusesForeignIdentity` failing only on the `BS_Drone` invalid-sample error from the external drone pack (X2 spillover); neither involves perception, threat or acquisition code.
9. **Not proven here, and not source defects.**
   - *Content tuning:* the authored sight's peripheral angle is 180°, so these enemies see all round,
     still only with line of sight. Hearing range is 10,000 cm. Stealth feel is a content/playtest gate.
   - *Hearing producers:* no project or Narrative source emits noise events. Hearing is handled correctly
     when a stimulus exists; whether weapons, movement and alarms produce it is Blueprint/content work.
   - *Latent fail-open:* a future enemy authored with a controller lacking perception would regain legacy
     distance targeting. The roster test pins the tracked roster so that regression fails automation.
   - Security Drone behaviour is not exercised in automation (X2 pack); its controller class matches the
     Enforcer's per the probe.
10. **PC04 relationship.** The undetected-bypass gate reads raw `UAIPerceptionComponent::GetKnownPerceivedActors`
    across all senses. With this roster's `max_age` of 0 a stimulus never ages out, so a player once heard
    stays "known" and the bypass is withheld even though hearing never authorises targeting. That errs
    conservative, not permissive, but it disagrees with the acquisition authority. The signal the gate
    should rely on is threat memory's *direct observation* — `ANarrativeNPCController::CanDirectlyTargetThreat`
    for each registered threat, or equivalently any `Sight`/`Damage`/`NetworkSensor` memory with
    `bDirectObservation` during the traversal window. Recorded only; PC04's overlap-harness blocker is not
    reopened here.

### PC01–PC04 — Ability and generation completeness (PARTIAL)

All named spenders now exist as native files: six Selene abilities including `Deflection`, `Dispatch`,
`StaccatoZero`, `StillpointGrenade`, `VeritysWake`, `AxiomNullPulse`; and `TarrikCinderSlam`,
`TarrikCinderlineRequiem`, `TarrikGuard`. Generation components exist for both protagonists, and
`GuardLifecycleValidation.md` records the guard lifecycle suites (PC09 closed separately, below). **These are marked PARTIAL rather
than CLOSED deliberately:** this pass confirmed the files and entry points exist, not that each
delivers its payload natively without a Blueprint release hook. Closing them requires per-ability
review, which is a slice of its own rather than a reconciliation result.

### PC09 — Guard cancellation (source CLOSED 12 September; authoring remainder is PC09-C)

Work done in the isolated worktree `ProjectVelkorran-c3`, branch `engineering/pc09-guard-lifecycle`
(stacked on E6), not the shared checkout. Evidence and reclassification pass; no production source changed.

1. **Question.** Does Tarrik's native Guard state admit, cancel and transition correctly, so that the
   only remaining gap is authored guard/counter content?
2. **Authority.** TDD v2 §5.2.3 (holding guard establishes a frontal plane; perfect guard creates a fast
   counter window and generates Echo), §6.5 (standard guard impact 8–20 Stamina; an exhausted guard breaks
   posture), the Echo table (+12 perfect guard, +10 guard counter), the Resonance table (Selene's Sever
   extends Tarrik's counter window), and `Config/DefaultEngine.ini` combat settings (start Stamina 8,
   perfect cost 5, guard multiplier 0.25).
3. **The audit claim is stale.** It said the ability observed only death, PoiseBroken and Sequencer while
   active, and that only the newer Echo/Deflection adapters closed the activation-to-binding race. In the
   current tree `USovGuardComponent` and `USovGameplayAbility_TarrikGuard` both refuse and live-cancel on
   dead, interacting, sequencer-controlled, ragdoll, fatal, poise-broken, guard-broken, Echo-active and
   deflecting, plus any Busy contribution beyond one activation-owned Busy (`AnyCountChange`). Both re-check
   immediately after binding, and activation epochs stop a cancelled start from continuing.
4. **Lifecycle, as implemented.**
   - *Admission and start.* Direct `BeginGuard` and GAS activation share one blocker set; the component also
     requires 8 Stamina and no existing guard or broken posture. Ownership flags are published before GAS tag
     dispatch so a reentrant cancel removes the contribution being added.
   - *Stamina and perfect defence.* Damage routing (`NarrativeAttributeSetBase`) resolves the frontal arc,
     heavy/unblockable class and Stamina transactionally. Perfect guard costs 5, blocks fully and still
     succeeds on the last Stamina, then breaks posture without a counter. Ordinary impact costs the clamped
     8–20 and mitigates to 0.25; an unaffordable cost or a heavy attack outside perfect timing breaks posture.
   - *Interruption and cancellation.* The component ends Guard on any blocker; ending broadcasts
     `OnGuardEnded`, which ends the ability; ability end ends the component. Removal and component rebinding
     end both.
   - *Counter.* A perfect guard opens one owned counter window (0.8 s). It survives input release and
     attack-state Busy changes, is cleared by incapacity or another defence, is consumed once by a landed
     `Sov.Damage.Source.GuardCounter` hit, and can be extended only while open.
   - *Cleanup.* Broken posture clears on its timer; rebinding removes every owned tag and cancels timers.
5. **Adapter differences, classified.** Echo, Melee, Deflection and Axiom also observe
   `Narrative.State.Weapon.Equipping`; Guard does not. No native code applies that tag (Narrative's authored
   equip flow does) and the TDD sets no rule for guarding through an equip, so it is a PIE/content check, not a
   source defect. `Sov.State.Status.Frozen` is not a gap: both native freeze paths (`SovSelenePayload`,
   `SovStatusComponent`) grant Busy with it, and Guard cancels on Busy. Native Melee refuses activation while
   `State.Guarding`, so a counter attack starts after release; the counter window surviving release is what
   makes that work, and it is now proven.
6. **Evidence — automation-verified on Mac.** `ProjectVelkorran.Campaign.Guard`, eight suites in a real
   world with the Narrative ASC and attribute set and no authored assets. The five existing suites
   (`AdmissionAndDirectInterrupts`, `GASCancellationAndEffectOwnership`, `ReentrantStartAndTagOwnership`,
   `PerfectDefenseLastStamina`, `CounterSurvivesAttackBusy`) are joined by:
   - `StaminaAdmissionImpactAndBreakRecovery`: 7.9 Stamina refuses both entries with no residue; 8 starts;
     Narrative's production tag-input release ends Guard; perfect timing expires on its timer without ending
     the hold; ordinary impact costs 10 and takes a quarter of the unguarded Shield loss; an unaffordable
     impact breaks, spends only what remains and refuses restart; the break expires, empty Stamina still
     refuses, 8 restarts; a heavy attack outside perfect timing breaks and also expires back to a startable
     Guard; no owned tag or Busy remains and every start has one end.
   - `CounterWindowReleaseExpiryAndSingleOwnership`: release preserves the counter; two concurrent attack
     Busy contributions and their removal do not kill it; re-guarding and perfect-guarding again leaves one
     counter contribution that pays once; the window expires, removing its tag, and then pays nothing; poise
     break and Deflecting clear it; Guard reactivates afterwards with no residue.
   - `RebindingReleasesOwnedStateAndReactivates`: rebinding with Guard and counter held, and again during
     broken posture, leaves no owned tag on either ASC, applies nothing when the cancelled timers would have
     fired, and Guard reactivates.
   The first run of the new suites failed on the fixture, not source: a timer armed outside a timer tick is
   pending until the next tick, so the test advanced 0.01 s short of each expiry. Margins were widened; no
   assertion was removed.
7. **Negative controls.** Seven seams were broken one at a time in production source, each rebuilt and run against the Guard suites, then restored from git (source diff empty; clean rebuild 8/8). All seven were caught: Busy clearing the counter (both counter suites); broken posture never expiring (StaminaAdmissionImpactAndBreakRecovery); Guarding tag leaking on end (7 of 8 suites); start-Stamina check removed (StaminaAdmissionImpactAndBreakRecovery); counter not consumed on landing (both counter suites); rebinding leaking counter and broken tags (RebindingReleasesOwnedStateAndReactivates); live interruption removed from component and ability (four suites including both cancellation suites). Three of the seven — stuck break, missing start-Stamina check, rebinding leak — were caught only by the new suites; the original five passed with those seams broken.
8. **Suites.** Guard 8/8. Affected suites (Guard, Defense, WeakPoint, Finisher, Projectile, Foundation, Selene, Exertion, Echo, Readiness, AxiomNullPulse, DominionHound, Transactions, Resonance, Melee, Companion, Threat) 129/0. Full suite 630 passed, 2 failed of 632: Validation.GameplayCuesStillResolve (X1) and PlacedNPC.DefinitionFallbackAndExistingOwnership, whose only error was the ABP_RefDrone missing-skeleton log from the SciFi_Drone_1 pack (X2 spillover); that test passes 1/1 run alone.
9. **Remainder, reclassified as PC09-C (content/editor gate).** A search of the on-disk `Content` tree,
   untracked assets included, found no reference to `SovGameplayAbility_TarrikGuard`, `Sov.State.Guard*`,
   `Sov.Event.Guard*`, `Damage.GuardClass` or `GuardCounter`. So no authored asset yet:
   - grants Guard to Tarrik or maps it to `Narrative.Input.AltAttack`;
   - implements the Blueprint presentation hooks (`Guard Ability Started/Ended`, `Guard Impact`,
     `Perfect Defense`, `Guard Broken`, `Counter Landed`) or guard hold/block/break/counter animation;
   - authors a counter attack node whose `AttackClassifications` carry `Sov.Damage.Source.GuardCounter`;
   - classifies enemy attacks as `Damage.GuardClass.Heavy`/`Unblockable` where intended.
   Also gated: the blocked-hit cue's `/Game/Cues` root (X1), guarding through an authored weapon equip, and
   owning-client prediction under latency (no Mac network automation).

### PC06 — remaining part (PARTIAL)

Echo persistence is carried by `AttributesToSave`. What is still unevidenced is a native caller that
begins/ends an encounter boundary for resource purposes. Source-only; low priority given C2.

### T2 — Cook exclusion (source, config and validation CLOSED 14 September; template boot remainder is T2-B)

Work done in the isolated worktree `ProjectVelkorran-c3`, branch `engineering/t2-cook-integrity`
(stacked on PC09), not the shared checkout. Shipping-build integrity audit: what actually enters a
packaged campaign cook, not what exists in the repository or is reachable in the editor.

1. **Question.** Can campaign packaging or cook discovery still pull in systems or assets that the campaign
   does not permit — XP, currency, loot, multiplayer or template UI, or other demo content — and does
   validation catch that without rejecting legitimate Narrative dependencies?
2. **Authority.** TDD v2 §1 (launch multiplayer: None, LOCKED), §15 content management (mission manifests
   enumerate dependencies; unused December-system assets excluded from cook), Appendix F (no obsolete class,
   loot, XP, rarity, vendor, crafting, morality or approval system in the campaign cook).
3. **Method.** The UE 5.7 cooker itself, `-run=cook -targetplatform=Mac -CookList -cookshowinstigators`, for the
   packaging stage's map set (`L_Aurelion_M12`, `L_Aurelion_M13`) and for no map. It lists every package a cook
   would include, with the instigator that added it, without saving. The first runs used
   `r.AreShaderErrorsFatal=0` because this host's Metal toolchain was broken; package discovery does not
   depend on compiled shaders. The host was later repaired (Xcode 26.6 system resources, Metal toolchain).
4. **Campaign cook-entry architecture, as measured.**
   - *Command-line maps:* the packaging stage passes `-map=`, so the cooker's all-maps fallback never runs; a
     no-map CookList confirmed the same roots minus the two maps.
   - *GameMapsSettings defaults:* `GameDefaultMap` (Narrative `MainMenuMap`), `GlobalDefaultGameMode`
     (`BP_NarrativeGameMode`), `GameInstanceClass` (`BP_NarrativeGameInstance`). `ServerDefaultMap` is excluded
     from a default cook.
   - *Asset Manager `ModifyCook`:* Primary Asset rules. Before the fix, NPCDefinition and PlayerDefinition were
     type-level AlwaysCook over `/NarrativePro` and `/Game`: 114 roots, 72 of them Narrative definitions no
     campaign content references.
   - *Startup soft object paths:* every non-editor config soft path loaded at startup, including
     `ArsenalSettings.GameEntryMap` (Narrative's demo open world) and `CharacterCreatorMap`.
   - *Startup packages:* classes loaded from config at startup (Narrative GameplayEffect classes, input actions,
     demo impact/footstep VFX, editor Tales node widgets).
   - *DirectoriesToAlwaysCook:* `/Game/Aurelion/VFX` and engine-plugin content directories.
   - *Input:* `DefaultTouchInterface=/Game/Input/TIS_MobileControls` names a missing asset.
   - *Dependencies:* game (non-editor-only) hard and soft package references from all of the above.
5. **Prohibited content found in the actual pre-fix cook (4,228 packages).** Six prohibited identities
   (`GE_GiveXP`, `NE_GiveXP`, `W_NarrativeMenu_Looting`, `WBP_Loot_TheirInventory`, `WBP_Loot_YourInventory`,
   `W_NarrativeMenu_MPMainMenu`), 494 Narrative demo packages and 173 demo quest/dialogue packages including
   the SecretMerchant vendor quest. Campaign content itself references none of it: the campaign GameModes,
   `BP_AurelionPlayerController` and both mission maps name no Narrative framework, template menu or demo package.
6. **Fixes.**
   - *Primary Asset rule (`c2b322d8`).* Definition types stay registered under `/NarrativePro` for runtime
     primary-asset-ID resolution, but the type rule is `Unknown` and a `/Game`-filtered `CustomPrimaryAssetRules`
     entry restores AlwaysCook for campaign-owned definitions. A custom override cannot lower a type rule.
   - *Validation cook-entry coverage (`011ad12a`).* `GatherConfiguredCookRoots` reads game defaults, packaging
     maps and directories, the touch interface and startup config references the way the cooker does;
     `-CookList=<log>` makes the cooker's own package list the root set; each finding names its cook-entry route.
   - *Validation dependency walk.* (`9f71fd09`) The walk now uses the cooker's game-only dependency query, so an editor-only reference never raises a finding; it changed no finding in this graph. Validation from configured roots and validation rooted at the cooker's post-fix package list flag the identical 64 prohibited identities, so no cook-entry route is missed. 23 of those 64 are absent from the CookList because `-CookList` only explores requests: it never loads or saves, so World Partition external actors are never folded into generated streaming cells, and the listing deliberately omits external-actor packages. Every one of the 23 is reached through the demo open world's external actors (e.g. `LS_Robbery` → `BPE_AddCurrency`, a `BP_LootableChest` actor, `DA_Crowd_Bandit` → `NPC_Mass_Ped_Bandit`) and ships in a real cook; validation reports them correctly. The campaign maps have no tracked external actors, so the CookList is faithful for campaign content.
   No asset was deleted and no prohibited-content rule was loosened.
7. **Post-fix cook (3,985 packages).** AlwaysCook roots 114 → 45; Narrative AlwaysCook roots 72 → 3, and those
   three (`AC_Pacifist`, `Appearance_Selene`, `Appearance_Tarrik_FullSuit`) are bundle references from campaign
   definitions, i.e. legitimate dependencies. Demo packages 494 → 434 and demo quest/dialogue 173 → 152: the
   content the demo definitions used to root is still reachable through the demo open world. The remaining six
   prohibited identities enter only through `GameDefaultMap`, `GlobalDefaultGameMode` and
   `ArsenalSettings.GameEntryMap`/`CharacterCreatorMap`.
8. **Negative controls.** Each control was applied to the fixed configuration, validated, and restored from a copy (`DefaultGame.ini` verified identical afterwards). NC1 — a config soft reference (`ArsenalSettings.DefaultMusicSet` → `BPE_AddCurrency`) and a game-default class (`GlobalDefaultServerGameMode` → `NE_GiveXP_C`), injected by command-line ini override: caught, with the XP and currency findings naming exactly those two routes. NC2 — `+DirectoriesToAlwaysCook` for Narrative's Luca dialogue folder: 18 new findings through that route, including the transitive `DBP_Luca` → `QBP_Demo_Narrative_SecretMerchant` and `NE_GiveXP` → `GE_GiveXP` chains. NC3 — the Primary Asset rule reverted to type-level AlwaysCook: effective AlwaysCook roots rose 45 → 114 and 56 findings named the Asset Manager rule, including the loot UI and demo dialogue it re-roots. NC4 — the pre-fix cooker list through `-CookList`: 68 findings against 64 after the fix, the four extra being content only the removed demo definitions carried. Non-overreach: the only Narrative AlwaysCook roots left are campaign bundle references (`AC_Pacifist`, `Appearance_Selene`, `Appearance_Tarrik_FullSuit`), which raise no finding, and the existing identity, demo-tale and loadout non-overreach suites pass.
9. **Suites.** Portable-tested: `Tests/Portable/SovCampaignCookRootPolicyTests.cpp` (32 checks) and `SovCampaignContentPolicyTests.cpp` (83 checks), clang `-Werror -pedantic` with UBSan. Automation-verified on Mac: `Campaign.Validation` 14/15, including the new `ConfiguredCookRoots`, `ProductionCookInputs` and `CookListRoots` and the existing identity, XP-modifier, AlwaysCook-root, demo-loadout and demo-tale non-overreach suites; the one failure is `GameplayCuesStillResolve` (X1). Affected suites (Validation, PlacedNPC, Encounter, AI, Aurelion) 124/125, same X1 failure. Full suite 633 passed, 2 failed of 635: `GameplayCuesStillResolve` (X1) and `PlacedNPC.OwnedEditorAssignmentPreservesMetadataAndRefusesForeignIdentity`, whose only error was the `BS_Drone` missing-animation log from the absent `SciFi_Drone_1` pack (X2 spillover); that test passes 1/1 alone.
10. **Reclassified as T2-B (content/editor and product decision).** The template boot configuration —
    `GameDefaultMap`, `GlobalDefaultGameMode`, `GameInstanceClass`, `ArsenalSettings.GameEntryMap` and
    `CharacterCreatorMap` — is the only remaining route for the multiplayer menu, loot UI, XP events and demo
    world. Replacing it means deciding what a packaged build boots into, which needs an authored campaign front
    end (T3). Shipping validation fails on it and names each route.
11. **Outside T2, recorded.** `NPC_AurelionEnforcer` still grants the Narrative demo pistol (existing gate K4).
    The M12/M13 manifest is rejected by the commandlet ("Curated companion abilities must be concrete and
    unique"), and the manifest check still expects `M01_Mantle`/`M02_OneDegree`; 387 referenced MetaHuman,
    UltraDynamicSky and overlay-material packages are absent from the checkout. `/Game/Cues` (X1),
    `SciFi_Drone_1` (X2), Windows/MSVC, and a full packaged cook remain outside.

### T5 — Packaged Win64 Game target (OPEN, Windows gate)

Mac compiles and links the Game target, which proves the runtime module carries no editor-only
dependency. It does not prove a shipping Win64 build. Windows-only; not closable here.

### T6 — Tests module disables unity (CLOSED 12 September; original detail kept below)

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


## Updates since the initial pass

Recorded as work lands, rather than deferred to another broad audit.

**12 September — PC01, PC02, PC03 closed; PC04 partial.** See
[AbilityPayloadAudit-2026-09-12.md](AbilityPayloadAudit-2026-09-12.md). Every ability traced
activation → targeting → payload → application → observable result, with each CLOSED verdict resting on
a test asserting the outcome. PC04's remaining gap is evidence, not implementation: nothing asserts the
undetected-bypass reward pays +15 once.

**12 September — T6 closed.** The cause was not anonymous namespaces but two file-scope
using-directives leaking across concatenated translation units. Compile-verified under forced unity
with blob membership confirmed, and automation-verified.

**12 September — validation trustworthiness restored.** Two consecutive full-suite runs now produce an
identical, attributable result: **618 passed, 1 failed**, the failure being X1. Before this, runs failed
two tests and the second varied between runs. Cause and fix are in X2 below. No suppression or
ignored-error mechanism was added, and no gameplay changed.

**12 September — PC04 attempted; remains PARTIAL on a test-harness blocker.** The AI-enabled
bypass fixture was built against the real gate entry/exit path, with `ConsumeUndetectedBypass` kept
private and the gate-owned receipt model untouched. It does not pass because, in a headless automation
world, the gate's entry volume never records the player's overlap: a world overlap query at the volume
finds the pawn, but `IsOverlappingActor` stays false, so no candidate is admitted. The two "no award"
cases passed vacuously until an entry-overlap assertion was added; that assertion stays. The fixture is
preserved at `Docs/Attic/SovBypassRewardRuntimeTests.cpp.wip` and is outside the suite, with no
suppression added. Untested hypotheses for a later harness pass: volume mobility, and overlap processing
tied to the fixture character's disabled movement ticking. Nothing observed indicates a source defect in
the gate. Detail in [AbilityPayloadAudit-2026-09-12.md](AbilityPayloadAudit-2026-09-12.md).

**12 September — C1 closed.** Protagonist isolation was treated as the invariant and tested through
the production handoff and load sequencing rather than by comparing keys or structs. Detail, the
negative control and the explicit limits are under C1 above. One content dependency surfaced and is
recorded there rather than closed: first-visit non-Echo resources depend on the authored
`DefaultAttributes` effect.

**12 September — C3 closed, one source defect fixed.** The checkpoint contract was qualified through the
production save path in an isolated worktree. The missing save-header migration history (TDD §15.9) was the
one defect; it is fixed and covered. Negative controls, suite results and explicit limits are under C3 above.
The fresh worktree also showed that X2's error spillover still reaches unrelated tests, contrary to the X2
note below; that is recorded for its own slice rather than folded into C3.

**12 September — E6 closed, no source defect.** Every tracked hostile — the Aurelion roster and both Dominion Hound Blueprints — acquires the player only through its authored perception and threat memory. The Hound world scan is a gated combat-state fallback, not a stealth bypass. The latent legacy nonperception path is unreachable for the authored roster and is now pinned by automation. Detail, negative controls and the PC04 signal recommendation are under E6 above.

**12 September — PC09 source closed, no source defect; authoring reclassified as PC09-C.** Native Guard admission, Stamina, perfect defence, interruption, re-entrant start, cancellation, counter survival and cleanup are automation-verified across eight suites with negative controls. The audit's cancellation complaint no longer describes the source. No authored asset grants, maps, presents or counter-classifies Guard yet; that remainder is PC09-C. Detail under PC09 above.

**14 September — T2 closed for source, config and validation; boot remainder reclassified as T2-B.** The UE 5.7 cooker's own CookList showed every Narrative demo definition cooked through an AlwaysCook Primary Asset rule, and shipping validation blind to game defaults, packaging settings and startup config references. Both are fixed, and validation now follows only the dependencies the cooker follows. What still ships prohibited content is the template boot configuration, which needs an authored campaign front end (T3). Detail under T2 above.

## X2 — `SciFi_Drone_1` marketplace pack, an external content gate

Also excluded from the source-engineering assessment.

The authored `NPC_AurelionSecurityDrone` genuinely needs this pack: its appearance targets
`SKM_SciFi_Drone_1`, and its configuration `AC_NPC_ReformationDrone` targets `GA_DroneGunfire` and
`GA_DroneRocketAbility`. `ABP_RefDrone` targets `SKEL_SciFi_Drone_1` and three of its animations. That
runtime dependency is **preserved, not engineered around**; the pack is marketplace content and
tracking it is not permitted.

Two consequences worth keeping distinct:

1. **Runtime.** The SecurityDrone cannot render or grant its gunfire and rocket abilities without the
   pack. This is a real gap for anyone validating that enemy, and it is why the two unresolved
   `DefaultAbilities` entries in `AC_NPC_ReformationDrone` are empty. An earlier note in this pass
   described them as authored-empty; that was wrong.
2. **Validation.** Those are soft references resolved asynchronously, so load failures and the
   `ABP_RefDrone` compile error surfaced after the causing test ended and were charged to whichever
   test was then active. That produced a failure that moved between runs. Four tests now load
   `NPC_AurelionEnforcer`, whose transitive closure is entirely tracked, which removes the async chain
   without touching the dependency. Editor identity tests still load the drone roster deliberately, and
   that is fine: they use synchronous `LoadObject`, so any error is charged to them.

## Source-engineering assessment

_Written at the start of 12 September and now historical; the
updates section and counts above are current._

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
