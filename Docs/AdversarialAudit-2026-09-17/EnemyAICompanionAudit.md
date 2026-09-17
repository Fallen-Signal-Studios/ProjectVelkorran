# Adversarial audit: enemies, factions, AI, encounters, bosses, allies and companions

Reviewer: Caelis (independent reviewer 2 of 5), 17 September 2026. Read-only source audit at HEAD `f07538c9`
(branch `codex/aurelion-tdd-content-20260913`, with uncommitted binary content changes that could not be inspected).
No build, test run, editor launch or content edit was performed.

`P/` = `Source/ProjectVelkorran`, `T/` = `Source/ProjectVelkorranTests/Private/Tests`,
`N/` = `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal`. Line numbers refer to HEAD `f07538c9`.

## 1. Scope and method

- **TDD authority:** `Docs/Design/Sovereign_Call_Origins_TDD_v2_2026-08-14.md` §8 (1156–1383), §12 (1996–2099),
  Appendix A.4, B.2, D. §0.3/§0.7 were skimmed so that non-goals are not demanded.
- **Accepted deviations:** `Docs/CampaignV2ChangeLog.md`. None of its six decisions (revised ability rosters, Health
  recharge, Cinderline magazine, kill drops, deterministic primary-fire variation) covers AI architecture,
  encounter or companion design. So the StateTree→Narrative BT/activity substitution and the missing
  `ASovAIController`/`USovThreatComponent`/`USovCombatRoleComponent` class names are **not** recorded deviations. They
  are reported as undocumented architecture choices (EA2-13), not as missing behaviour.
- **Code read in full or in the relevant parts:** `SovEncounterDirector.cpp/.h`, `SovEncounterDirectorMass.cpp`,
  `SovEncounterCoordinationComponent.cpp/.h`, `SovEncounterCoordinationPolicy.h`, `SovAurelionCrucibleDirector.cpp/.h`,
  `SovGameplayAbility_AurelionElite.cpp/.h`, `SovAurelionElitePolicy.h`, `SovAurelionEnemyRoles.h` (plus durability
  effects), `SovAurelionRoleActivities.cpp` (BT task setup), `SovBTTask_UseCombatAbility.cpp`,
  `SovCompanionComponent.cpp/.h`, `SovCompanionCommands.cpp`, `SovCompanionCommandActivity.cpp/.h`,
  `SovResonancePolicy.h`, `SovFatalRecoveryComponent.cpp` (companion failure), `SovCampaignMassProxy.cpp`,
  `SovThreatTargeting.h`, `N/Private/AI/NarrativeThreatMemory.cpp` (1–300), `N/Private/GAS/NarrativeBotAttackSelection.cpp`
  (180–480), the prior-finding sites (pickups, finisher, weak point, Reformation drone), and the relevant tests.
- **Binary content** (`Content/Aurelion/Enemies/*.uasset`, BTs, EQS, AC_* ability configurations, `L_Aurelion_M12.umap`) cannot be
  read. Where a finding depends on authored content, this is stated. The only content evidence used is the editor
  authoring script `Scripts/Editor/setup_aurelion_enemy_roles.py`, which shows how the content was built.
- **The context documents** (`AurelionTDDAlignment-2026-09-13.md`, `AurelionTDD90Blockers-2026-09-15.md`,
  `AurelionCompanionCombat-2026-09-13.md`) were used to find where to look. Nothing they claim was taken as evidence.

## 2. TDD coverage table

Status: **Impl** = implemented in source with meaningful behaviour; **Partial**; **Missing**; **Changed** = deliberately changed
under a recorded decision. "Acceptance" rows need measurement, which no source audit can supply.

| TDD | Requirement | Status | Evidence / gap |
|---|---|---|---|
| §8.1 Dominion | formation, command hierarchy, loss-of-command effects | Partial | Hound/Handler command link and pack coordinator (`P/Private/AI/SovDominionPackCoordinator.cpp`, `BTTask_SovDominionHandlerCommandHound.cpp`). Packs are spawner-owned and cannot join an encounter director (EA2-09). No shield-lane or heavy-unit logic in source. |
| §8.1 Reformation | sensors, shared marks, crossfire, drones | Partial | Sweep scanner produces `NetworkSensor` observations (`P/Private/AI/SovAurelionSweepScanner.cpp:178`). Crossfire EQS query (`SovAurelionCrossfireQuery.cpp`) and drones exist. No redundant command or role rotation in source. |
| §8.1 Eclipse | links, coordinator, broken individuals, corruption zones | Partial | Weaver armour links and ally-alert sharing (`SovAurelionEnemyRoles.cpp:673–735`), Linkbound, WallRunner, Elite. Nothing in source for "broken individuals become distinct". |
| §8.1 Remnant | rare local threats | Missing | None. This is acceptable for the current slice but still uncovered. |
| §8.2 | 10-role taxonomy, behaviour differs by role | Partial | `ESovEncounterRole` (`SovEncounterCoordinationComponent.h:17`) is a label used only for simultaneous role quotas (`.cpp:568–569`). No role owns behaviour in source. Distinct behaviour lives in per-archetype BT content that cannot be inspected. |
| §8.3 | StateTree, perception, EQS, Mass; TDD class stack | Partial / undocumented change | Narrative BT/activities replace StateTree, and `ANarrativeNPCController` plus threat memory replace `ASovAIController`/`USovThreatComponent`. Perception and EQS are real. High-level states such as surrender and corrupted/linked have no explicit state model. EA2-13. |
| §8.4 | A/B/C/D tiers, budgets, promotion preserving identity | Partial | A≤16 and B≤24 are validated per wave (`Coordination.cpp:111–113,157–171`). B throttles brain/perception tick (`.cpp:413–427`). C/D promotion and demotion preserve identity, GAS, weak points and pose (`SovEncounterDirectorMass.cpp:197–294,428–591`), but the API has **no production caller** (EA2-08), loads synchronously (C06) and uses frozen poses (C07). Summons bypass the A budget (EA2-02). |
| §8.5 | threat memory, melee slots, ranged off-screen rules, commander slot changes, Tarrik challenge, cloak, protection priorities | Partial | Melee slots, role quotas, relief throttling and off-screen warning receipts are wired through Narrative bot selection (`N/Private/GAS/NarrativeBotAttackSelection.cpp:217–218,278–287`). Cloak breaks direct targeting (`N/.../NarrativeThreatMemory.cpp:273–286`). **Missing:** commander slot expansion, difficulty scaling of slots and Tarrik challenge intent (no source hits). The warning rule can be bypassed (EA2-03). |
| §8.6 | stimuli set; debug of source/strength/confidence/LKP/sharing/expiry | Impl (producers partial) | `FNarrativeThreatMemory` carries every debug field (`N/Public/AI/NarrativeThreatMemory.h:17–31`), with bounded memory, confidence decay and faction sharing. `EchoCorruption`, `Command` and `ViewmakerSpoof` have **no production producer**; only tests emit them (EA2-14). |
| §8.7 | activation, waves, quotas/slots, objective phase, music/intensity, checkpoint, reinforcements, civilians, completion, telemetry, low-pressure state | Partial | Entry checkpoint, save-verified begin, retry, waves with event gates, protected participants, relief state, completion and diagnostics are strong (`SovEncounterDirector.cpp:323–513,911–1223`). Music/intensity is a delegate only (`OnPressureChanged`, no C++ consumer). Open defects: C03, EA2-01, EA2-10. |
| §8.8 | rotating encounter patterns | Partial (content) | Crucible link-sever → thermal-fracture phases, crossfire and wall-route pressure exist. Most patterns are content-owned and cannot be judged from source. |
| §8.9 | boss spec, phase rules, retry, exploit tests, no health-only phases | Partial | Elite has three health-gated phases that shorten the attack interval (and so raise frequency), add a Summon and re-arm durability (`SovAurelionElitePolicy.h`, `SovGameplayAbility_AurelionElite.cpp`), with retry from phase-B entry. No Appendix D specification exists. The defined `DamageResistance` policy is unused (EA2-12). EA2-01, EA2-02, EA2-03, EA2-06. |
| §8.10 | allied tactics, civilians (panic, shelter, rescue), no punishment for ally path failure | Partial | Protected participants fail the encounter on death (`SovEncounterDirector.cpp:584,602–615`). No panic/shelter/follow-volume code (no source hits). |
| §8.11 | acceptance: role readability, 90% visible lethal, AI count budget, Selene/Tarrik effects, 99.5% companion co-action | Missing (evidence) | No automated traversal or co-action statistics harness and no footage analysis. Budget validation is static only. |
| §12.1–12.2 | companion contract | Partial | Mutual move-ignore removes body blocking (`SovCompanionCommands.cpp:66–71`). Curated abilities, protected/boss finisher avoidance and a contribution cap are present. The cap is structurally below the TDD band (EA2-05). |
| §12.3 | six contextual commands | Partial | API exists (`SovCompanionComponent.h:27`). FocusTarget/DefendPerson have **no production or test caller** (EA2-07). FocusTarget approach is range-limited (C05 partial). |
| §12.4 | state branches Follow/Lead/CombatRole/CommandTask/CoAction/NarrativeAnchor/Rescue/Separated/RecoverPath/Scripted | Partial | Follow (Regroup), CommandTask, CoAction with hidden fallback, RecoverPath (25 m, `SovResonancePolicy.h:25–29`), Rescue (`CanProvideRescue`) and Scripted (Sequencer tag) exist. Lead and NarrativeAnchor are not modelled. Separation recovery runs only while a command goal is current (`SovCompanionCommands.cpp:229`). |
| §12.5 | disabled state, canon protection, revive interaction, clear failure, difficulty scaling | Partial | Required and protagonist companions trigger fail/retry (`SovCompanionComponent.cpp:326–339`, `SovFatalRecoveryComponent.cpp:97–131`). Ordinary allies auto-revive after encounter success (`SovCompanionComponent.cpp:341–357`). No revive interaction and no difficulty scaling. EA2-11. |
| §12.6 | AI protagonist: bespoke tree, curated kit, 25 m anchor recovery, intent display | Partial | `USovProtagonistCompanionActivity` is an empty subclass (`SovCompanionCommandActivity.h:44–50`), so there is no bespoke decision logic. The curated kit and 25 m recovery exist. |
| §12.7 | bark priorities, cooldowns, variants, interruption | Impl (other reviewer's domain) | `ESovNarrativeCuePriority` matches the TDD order, with `FSovBarkVariant` and cooldown (`P/Public/Narrative/SovNarrativeCue.h:12–40`). |
| §12.8 | companion acceptance | Missing (evidence) | Workspace notes report contribution below band. No 0.4 s obstruction or 1.0 s co-action measurement. |
| A.4 | density and slot numbers | Partial | A/B caps match. Melee slots accept 1..A+B, not the TDD's 2–4 (EA2-15). The off-screen anticipation "+25–50%" is implemented as a flat ≥0.25 s acknowledged warning lead (`Coordination.cpp:124`). The C 40–100 count is unproven. |
| B.2 | encounter definition fields | Partial | Present: ID, bounds, composition roles/waves, slot policy, protected actors, checkpoint policy, A/B budget. Missing: spawn/promotion anchors, difficulty variants, music state, dialogue hooks, test route, a telemetry ID distinct from EncounterId. |
| D | enemy/boss spec template | Missing | No archetype or boss specification documents exist under `Docs/` (grep for template headings found only the TDD itself). |

## 3. Findings

Severity: **P1** blocks engineering acceptance or corrupts state/progress. **P2** significant. **P3** minor.
Each finding is classified as a source-proven defect, a missing integration, or a hazard that needs runtime or content evidence.

### EA2-01 (P1) — Elite summons outlive their encounter: they survive victory, phase transfer and phase-B retry

- **TDD:** §8.7 (completion and cleanup, reinforcements), §8.9 (checkpoint and retry behaviour), §8.4 (mission ownership).
- **Class:** source-proven lifecycle defect. Its trigger depends on content that the authoring script shows is granted.
- **Evidence:**
  - Summon is granted to the Elite at startup: `Scripts/Editor/setup_aurelion_enemy_roles.py:641–648` adds Slam, Lance
    and Summon to `AC_Abilities_AurelionElite` `default_abilities`. It is therefore available in link phase A as well as phase B.
  - Summon unlocks at ≤66% health (`P/Public/AI/SovAurelionElitePolicy.h:409,459–467`;
    `P/Private/Abilities/SovGameplayAbility_AurelionElite.cpp:421`) and resolves "its" director by whichever director currently lists
    the Elite (`.cpp:236–246`). Adds are registered only as **attempt actors** of that director (`.cpp:484`).
  - Attempt actors are destroyed only in `RetryEncounter` → `CleanupAttemptActors` (`P/Private/Campaign/SovEncounterDirector.cpp:962,846–869`).
    `CompleteEncounter` (489–513) and `FailEncounter` (515–528) neither destroy nor suspend them.
  - Phase A freezes and transfers only `Participants` (`SovAurelionCrucibleDirector.cpp:257–263,207–218`).
    `AreOwnedParticipantsQuiescent` ignores attempt actors (`SovEncounterDirector.cpp:616–632`), so
    `IsCompletedPhaseBoundaryQuiescentForSave` (`SovAurelionCrucibleDirector.cpp:143–152`) reports a quiescent save
    boundary while adds are still live, hostile and unsuspended.
- **Failure sequence:** Selene's link phase A. The player damages the Elite below 142 of 212.8 health, and the Summon activates and spawns up to three
  Enforcers owned by the phase-A director. Both links are severed, phase A freezes its participants and completes, and the CP save
  boundary is admitted while the adds keep attacking. The Tarrik handoff transfers the roster to phase B, but the adds stay in phase A's
  `AttemptActors`. Phase B fails (for example the premature kill in EA2-06) and `RetryEncounter` on phase B runs `CleanupAttemptActors` for
  phase B only, so the phase-A adds persist into the "restored" phase-B entry. The same leak applies to phase-B adds after
  victory: they remain in the world after `Succeeded` and through the arena-exit autosave (`SovEncounterDirector.cpp:169–173`).
- **Test gap:** `T/SovEncounterObjectiveRuntimeTests.cpp:1347–1393` checks the phase gate and that adds are not participants.
  Its comment says adds "are cleaned up with the attempt", but no assertion covers cleanup on success, transfer or retry.
- **Confidence:** high for the code path. Medium that the link phase actually reaches ≤66% in play (plausible at 212.8 health).
- **Fix inside existing owners:** make attempt actors part of the director's terminal transitions. On `CompleteEncounter`/`FailEncounter`,
  suspend them (reuse `SuspendActor`) and destroy them before `Succeeded` is published. Make the phase-boundary quiescence check
  require zero live attempt actors. Alternatively, prevent the Summon in `ASovAurelionLinkPhaseDirector` (a `RequiredPhase` gate by owning
  director type). Add regressions for adds across success, phase transfer and retry.

### EA2-02 (P2) — Summoned adds bypass encounter coordination and the Tier-A budget

- **TDD:** §8.4 (12–16 Tier A), §8.5 (melee slots, off-screen rules, relief), A.4.
- **Class:** source-proven missing integration.
- **Evidence:** only participants are bound to the coordinator, through `RefreshComposition` → `SetBotAttackCoordinator`
  (`SovEncounterCoordinationComponent.cpp:397–405`) and promotion (`SovEncounterDirectorMass.cpp:633`). Adds from
  `SovGameplayAbility_AurelionElite.cpp:473–486` are never bound. Narrative bot selection treats a null coordinator as admitted
  (`N/Private/GAS/NarrativeBotAttackSelection.cpp:217–218`: `bCompositionReady = !Coordinator || ...`). Composition budgets count
  `Director->Participants` only (`Coordination.cpp:157–171,681–700`). The Elite header's claim that these abilities "remain
  subject to the encounter coordinator" (`SovGameplayAbility_AurelionElite.h:18–22`) holds for the Elite's own attacks, not for
  its adds. Spawning uses `AdjustIfPossibleButAlwaysSpawn` at a fixed 420 cm ring with no navmesh projection or line-of-sight check
  (`.cpp:470–474`), next to survivor recesses that the layout reference requires to stay separate.
- **Failure sequence:** at ≤15% health (enrage), up to 4 living adds melee the player without taking melee slots, ignore relief when the player is
  nearly dead, and attack from off-screen without warning receipts. The arena exceeds the authored A-tier composition by up to 4.
- **Confidence:** high (source). Placement effects need runtime evidence.
- **Fix:** have the Summon ask the owning director to bind each add to `Coordination` (a supporting-tier, attempt-scoped
  composition entry that is not required for victory), and count living adds in the per-wave A/B check. Project spawn points to
  navigation, reject points without line of sight from the Elite, and exclude authored protected-recess volumes.

### EA2-03 (P2) — Elite Lance can hit the player off-screen without the required warning receipt

- **TDD:** §8.5 ("ranged units ... respect off-screen lethal-attack rules"), §8.11 (≥90% visible or warned).
- **Class:** source-proven defect.
- **Evidence:** the coordinator requires an acknowledged warning only when `Target == Director->GetEncounterPlayer()`
  (`SovEncounterCoordinationComponent.cpp:570–576`, again at `598–599`). Bot selection passes the BT/focus target
  (`N/.../NarrativeBotAttackSelection.cpp:217,278–287`). The Elite ignores that target and always acts on
  `FindBossTarget` = encounter player (`SovGameplayAbility_AurelionElite.cpp:274–281,291,311`). The Lance then sweeps and damages
  that player (`.cpp:391–413`).
- **Failure sequence:** the Elite's controller focuses the Tarrik or Selene companion proxy, which is a legal hostile target. Selection admits the
  ranged Lance against the companion, and no warning is needed because the target is not the player. `ActivateAbility` re-targets the player, who is
  behind the Elite's screen position, and the 22-damage lance lands without an off-screen cue. The mirror case also fails: when the selection
  target is the player, range and line of sight were evaluated for the player, but the payload is still correct only by coincidence.
- **Confidence:** high.
- **Fix:** capture the selection target instead of re-deriving it. Have `ActivateAbility` read the target that the reservation was admitted for
  (the lease target is available through `IsBotAttackExecutionValid`), or refuse activation when `FindBossTarget` differs from
  the admitted target.

### EA2-04 (P2) — C03 remains open: losing a required participant without a death receipt stalls victory while the next wave advances

- **TDD:** §8.7 (wave gates, completion), §8.4 (mission ownership).
- **Class:** source-proven defect (prior C03, re-verified).
- **Evidence:** victory needs `DefeatedParticipants`, which only `HandleDeath` fills (`SovEncounterDirector.cpp:570–587,674–690`). The director has no
  EndPlay or destruction observation of participants (no `OnDestroyed`/`OnEndPlay` binding in `P/Private/Campaign` or `P/Private/Characters`).
  The default wave gate skips invalid participants (`SovEncounterCoordinationComponent.cpp:691`), so `bRequiredAlive` becomes false and
  the next wave releases (`.cpp:698–707`). Event-gated waves correctly fail closed (`.cpp:794–802`). Narrative's multi-instance
  `FellOutOfWorld` → `CleanUp(0)`/`Destroy()` is unchanged (`N/Private/UnrealFramework/NarrativeNPCCharacter.cpp` `FellOutOfWorld`).
- **Failure sequence:** a two-wave encounter. A required wave-0 NPC falls through a floor seam and is destroyed without a GAS death. Within 0.1 s the next wave releases.
  The player kills all remaining enemies, and `EvaluateCompletionConditions` returns at `:687` forever with the encounter Active. The objective and travel are blocked, and
  only a manual or fatal retry recovers.
- **Tests:** none destroy a participant without death (no `Destroy()` in `SovCoordinationRuntimeTests.cpp`/`SovEncounterRuntimeTests.cpp`).
- **Confidence:** high. It is marked P2 rather than P1 only because it needs an unusual destruction path. Treat it as P1 if any authored
  content destroys encounter NPCs (corpse cleanup timers that fire before a death receipt, kill volumes).
- **Fix:** as proposed previously. Bind participant `OnEndPlay` with attempt and representation generation. On unexpected loss, hold waves (make
  `Coordination.cpp:691` treat an invalid non-defeated required actor as still alive) and call `FailEncounter`. Sanctioned demotion
  (`SovEncounterDirectorMass.cpp:253–260`) and retry destruction already clear ownership first and can be exempted by generation.

### EA2-05 (P2) — Companion contribution is a hard cap below the TDD band and is zero until the player deals damage

- **TDD:** §12.8 ("Companion contribution on Standard is 15–25% of ordinary encounter damage"), §12.2 ("contribute visibly").
- **Class:** source-proven design/implementation mismatch.
- **Evidence:** `SovResonancePolicy::WithinContributionBudget` requires `PlayerDamage > 0` and `Companion < Player·f/(1−f)`
  (`P/Public/Resonance/SovResonancePolicy.h:18–24`). Offence is gated on it (`SovCompanionCommands.cpp:294–296`) and damage is clamped
  to the remainder (`:385–402`). Contribution resets on every encounter scope change (`:74,189–190`).
- **Consequence:** by construction the companion share can never exceed f (default 0.20, `SovCompanionComponent.h:43`). Any in-band
  result therefore needs the companion to saturate the cap continuously, so the 15% floor is structurally unlikely. At encounter start, and whenever the player
  is defending or evading, the companion attacks nothing. Attacks that exceed the remainder still animate but deal clamped or zero
  damage (`:397–402`) with no feedback. This matches the measured shortfall reported in the workspace notes, but that report
  is context only.
- **Confidence:** high.
- **Fix:** keep the upper cap. Allow a bounded opening allowance (for example a fixed damage budget or time-based floor per encounter scope) so
  the companion can contribute before the player's first hit, and refuse (not clamp) an attack whose remainder is below its expected damage so
  no silent zero-damage swings occur.

### EA2-06 (P2) — The boss fails the encounter when the player "over-performs": killing the Elite conventionally in either phase forces a retry

- **TDD:** §8.9 (phase transition rules, "final interaction cannot fail because of ambiguous prompt timing", assisted alternates).
- **Class:** source-proven design defect.
- **Evidence:** phase A fails if the Elite dies (`SovAurelionCrucibleDirector.cpp:237–239`). Phase B fails a kill that happens before the thermal
  fracture receipt (`:424–427,433–436`). No health floor, nonlethal gate or damage refusal protects the Elite before the required
  mechanic: no hits for boss-protection predicates in the damage path, and the only boss floor is for companion damage (`SovCompanionCommands.cpp:399–400`).
  Link-phase health is 212.8 (`SovAurelionElitePolicy.h:400`) against a measured baseline death time of 16–36 s at 53.2 health.
- **Failure sequence:** the player uses Cinder Slam, a sticky grenade and primary fire on the Elite while working towards the links. The Elite dies before the second sever, and the
  encounter goes to `Failed` with a reload. The same happens in phase B if a heavy Echo lands before frost+heat.
- **Tests:** `T/SovEncounterObjectiveRuntimeTests.cpp:389` ("CrucibleEliteDeathBeforePhaseBoundaryRequiresRetry") codifies the
  failure rather than preventing it.
- **Confidence:** high.
- **Fix:** add a phase-owned lethal floor in the existing phase directors. While the required receipt is missing, clamp Elite health at a
  threshold through the existing protection or `LimitSovDamage`-style source policy, and expose a readable "invulnerable to finish" cue.
  Keep failure only for non-player causes.

### EA2-07 (P2) — Player-facing FocusTarget/DefendPerson commands are unreachable, and FocusTarget still does not approach beyond 10 m of the leader

- **TDD:** §12.3, §12.2.
- **Class:** missing integration (unreachable command); prior C05 partially fixed.
- **Evidence:** the only production `RequestCommand` callers use `HoldPosition`, `Regroup` or `Interact`
  (`SovAurelionDeparturePresentation.cpp:44`, `SovAurelionRequestActor.cpp:293`, `SovCompanionComponent.cpp:356`,
  `SovAurelionThermalFractureComponent.cpp:331`, `SovViewmakerLibrary.cpp:69`). No C++ caller or test uses
  `ESovCompanionCommand::FocusTarget` or `DefendPerson` (grep across `Source`). A Blueprint caller cannot be excluded.
  The approach for FocusTarget happens only when the focus is within 10 m of `Destination`, which is the leader's location for FocusTarget
  (`SovCompanionCommands.cpp:231–232,305–308`), while validation admits targets up to 25 m from the player (`:115–117`), and it
  requires companion line of sight (`:306`).
- **Failure sequence:** the player selects a hostile 15 m ahead behind a crate. The command is accepted and the companion sets focus but never approaches (and never
  attacks unless an already in-range candidate exists). C05's original symptom persists for 10–25 m targets and occluded targets.
- **Confidence:** high for the approach gate. Reachability needs content confirmation.
- **Fix:** use the selected target as `Destination` for FocusTarget (leash to the leader separately), drop the line-of-sight requirement for approach
  (path to a line-of-sight point), and add runtime tests for both commands.

### EA2-08 (P2) — Tier C/D representation and runtime tier changes have no production caller

- **TDD:** §8.4 ("The encounter director promotes and demotes eligible agents at controlled boundaries").
- **Class:** missing integration (Blueprint callers cannot be ruled out).
- **Evidence:** `SetParticipantRepresentation`, `SetParticipantMassRoute` and `SetDecisionTier` are called only from
  `T/SovCampaignMassRuntimeTests.cpp` (grep over `Source`). The editor authoring script does not set `bAllowMassRepresentation`
  (no hits in `Scripts/Editor/setup_aurelion_enemy_roles.py` for Mass). Each C/D record is a full encounter participant with
  1–32 poseable mesh components (`SovEncounterDirectorMass.cpp:157–158`), which is not a scalable 40–100 agent crowd.
- **Confidence:** medium-high.
- **Fix:** add a director-owned boundary trigger (a volume or objective phase) that calls the existing API, and a crowd-tier representation that is
  not a participant, for background ranks. Measure 40–100 agents on the approved PC target.

### EA2-09 (P2) — Dominion packs are structurally excluded from encounter directors

- **TDD:** §8.7 (director owns spawn waves, slots and checkpoint), §8.1 Dominion.
- **Class:** missing integration; needs content evidence of intended use.
- **Evidence:** the pack coordinator resolves Handler and Hounds from `ANPCSpawner` instances (`SovDominionPackCoordinator.cpp:27–44,202–206`).
  `CaptureNPC` rejects any participant whose `OwningSpawnerGUID` is valid (`SovEncounterDirector.cpp:220–224`), so a spawner-produced pack cannot be
  captured, checkpointed, wave-gated or coordinated (melee slots, off-screen warnings, relief).
- **Confidence:** medium. It depends on whether Narrative's spawner sets that GUID for these NPCs, which is standard Narrative behaviour.
- **Fix:** let the pack coordinator accept director-registered, encounter-owned actors as an alternative source (reuse
  `RegisterParticipant`/`SetEncounterOwned`), so Dominion encounters use the same checkpoint and coordination path.

### EA2-10 (P3) — Death receipts that arrive during a director mutation are dropped permanently

- **TDD:** §8.7 completion.
- **Class:** source-proven hazard that needs runtime evidence of a synchronous kill inside a guard.
- **Evidence:** `HandleDeath` returns without recording when `bMutationInProgress` (`SovEncounterDirector.cpp:572`).
  `OnDeathStateChanged` fires once. Guards are held across callback-rich work during Active: `TickMassPromotions` releases suspension, re-enables
  collision (overlap/hazard callbacks) and resumes the brain (`SovEncounterDirectorMass.cpp:596,636–647`). `BeginEncounter` broadcasts
  `Active` under its guard (`SovEncounterDirector.cpp:446,478`).
- **Failure sequence:** a promoted required NPC's collision is enabled inside a hazard volume and it dies synchronously inside the guard. Its receipt is lost, and
  because `BindDeaths` runs after (`Mass.cpp:647`) its ASC was not even bound. The encounter can never complete.
- **Confidence:** medium for the path, low for frequency.
- **Fix:** queue death receipts that arrive during a mutation (by attempt and generation) and drain them after the guard releases, then call `EvaluateCompletionConditions`.

### EA2-11 (P3) — Companion defeat outside a required or recovery encounter is permanent, and there is no revive interaction

- **TDD:** §12.5.
- **Class:** missing behaviour; one branch is source-proven.
- **Evidence:** `SovCompanionComponent.cpp:326–339`. An ordinary ally with neither `RequiredEncounter` nor `RecoveryEncounter` sets
  `bDisabled` and nothing else happens. A protagonist companion depends on `RequestCompanionFailure`, which refuses when the player is dead, the transition
  state is not idle, there is no active mission, or a failure is already pending (`SovFatalRecoveryComponent.cpp:106–110`). Nothing retries later, so in those
  states the protagonist proxy stays dead. There is no revive interaction or difficulty-based survivability.
- **Confidence:** medium (branch outcome needs runtime).
- **Fix:** when the failure request is refused for a protagonist or required companion, record a pending canonical failure that the recovery
  component consumes when it returns to Idle. Add an interaction-driven revive using the existing `Revive` path.

### EA2-12 (P3) — Boss phase policy is only partly consumed and not monotonic at runtime

- **Evidence:** `DamageResistance` (`SovAurelionElitePolicy.h:447–456`) has no consumer (grep). `ResolvePhase` calls
  `PhaseForHealthFraction` without the current phase (`SovGameplayAbility_AurelionElite.cpp:257`), so healing or a crucible durability override
  (`SovAurelionCrucibleDirector.cpp:384–395`) walks the phase back. The header comment admits that the monotonic owner "will land" later
  (`.cpp:250–251`).
- **Fix:** store the reached phase on the Elite (a savable component or the thermal director) and apply resistance through a phase-owned effect.

### EA2-13 (P3) — Undocumented AI architecture substitutions

- StateTree, `ASovAIController`, `USovFactionComponent`, `USovThreatComponent` and `USovCombatRoleComponent` (TDD §8.3, §12.4, §12.6) do not exist.
  Narrative BT/activities, `ANarrativeNPCController` threat memory and composition data fill these roles, and the protagonist
  activity is an empty subclass (`SovCompanionCommandActivity.h:44–50`). The implementation is reasonable, but `CampaignV2ChangeLog.md` does not record it.
- **Fix:** record the decision (player problem, affected acceptance gates) in the change log, or plan the bespoke protagonist tree.

### EA2-14 (P3) — Three TDD perception stimuli have no producer

- `ENarrativeThreatSource::EchoCorruption`, `Command` and `ViewmakerSpoof` are defined with confidence caps
  (`N/.../NarrativeThreatMemory.cpp:31–45`), but no production code reports them. Only `SovAurelionSweepScanner.cpp:178`
  (`NetworkSensor`) and native perception (Sight, Hearing, Damage) produce observations. The TDD's "Tarrik challenge alters eligible
  intent" (§8.5) has no implementation either; it may have been superseded by the revised Tarrik roster, but that is not recorded.

### EA2-15 (P3) — Slot and warning numbers drift from Appendix A.4, and warnings are Aurelion-only

- `MeleeAttackerSlots` is validated to 1..(A+B), default 2 (`SovEncounterCoordinationComponent.h:61`, `.cpp:111–113`), against the TDD's 2–4 range
  modified by difficulty or commander; no modifier exists. The only C++ presenter that acknowledges off-screen warnings is
  `SovAurelionWorldPresentation.cpp:90,213`. In any other encounter with the default `bRequireOffscreenRangedWarning=true`, off-screen ranged attacks on the
  player are never admitted, which silently removes flanking ranged pressure. `UpdateWarnings` re-creates and re-broadcasts a warning
  every time a ranged-capable source leaves the screen (`.cpp:638–660`), which may spam cues.

## 4. Prior-finding dispositions

| ID | Prior claim | Disposition | Evidence |
|---|---|---|---|
| ED-02 | Pickup grants before reserving itself | **Fixed** | `bGrantInProgress` reservation rejects re-entrant overlap before the grant (`P/Private/Combat/Pickups/SovCombatSustainPickup.cpp:155,180–187`, `Public/Combat/Pickups/SovCombatSustainPickup.h:52`). Ammo is capped to reserve space and the remainder conserved (`SovAmmoCombatSustainPickup.cpp:49–80`). Regressions for nested ammo callback and Echo reservation: `T/SovResourceTransactionRepairTests.cpp:183,215–235`. |
| ED-04 | Boss/elite finisher phase consumed without outcome | **Fixed** | Phase is committed only after accepted damage on the same target generation, and the event is published regardless of ability cancellation (`P/Private/Abilities/SovGameplayAbility_Finisher.cpp:228–231,253–256`; `P/Private/Combat/SovFinisherTargetComponent.cpp:66–80`). Regression cancels during the Health callback and asserts one outcome and no replay: `T/SovFinisherProjectileRuntimeTests.cpp:117–157`. Residual: the outcome event is not durably queued if no listener exists at that instant (acceptable, not a finding). |
| ED-05 | Weak-point receipt history unbounded | **Fixed** | `ResolvedHitTransactions` set removed. Duplicate rejection now consumes a receipt on the damage result (`P/Private/Components/SovWeakPointComponent.cpp:537`), and pending hits are bounded to 32 (`:551`). |
| ED-07 | Drone interruption not immediate or callback-safe | **Fixed** | Epoch-scoped tag and health listeners cancel immediately (`P/Private/Abilities/SovGameplayAbility_ReformationDrone.cpp:401–445`). Release revalidates after the Blueprint hook (`:474–496`). Regressions: `T/SovDroneContinuationRuntimeTests.cpp:146–201`. |
| C02 | Final kill during Mass promotion loses victory | **Fixed** | Director `Tick` re-evaluates completion after promotions (`P/Private/Campaign/SovEncounterDirector.cpp:1028–1031,674–690`). Tick stays enabled during promotion (`SovEncounterDirectorMass.cpp:293,656–657`). Regression covers success and failure branches: `T/SovCampaignMassRuntimeTests.cpp:446–505`. Related residual: EA2-10. |
| C03 | Required participant loss without receipt; wave gate disagrees | **Still open** | See EA2-04. |
| C05 | FocusTarget never approaches | **Partially fixed** | An approach exists (`SovCompanionCommands.cpp:297–317`) but is gated to within 10 m of the leader and to line of sight. No FocusTarget test and no production caller. See EA2-07. |
| C06 | Mass conversion synchronous loads / no residency | **Still open** | `LoadSynchronous` during Active promotion (`SovEncounterDirectorMass.cpp:268–269`) and in proxy visuals (`SovCampaignMassProxy.cpp:39,44,47,54`). No async or streamable lease in `P/Private/Campaign`. Impact is reduced by EA2-08 (no production caller). |
| C07 | Tier-C proxy is frozen pose | **Still open** | `UPoseableMeshComponent` with one captured pose and tick disabled (`SovCampaignMassProxy.cpp:49–59`), with whole-actor sliding along routes (`:119–133`). |

## 5. Test adequacy summary

The coordination, threat-memory, perception-fairness, Mass round-trip, finisher, drone, Aurelion role and crucible suites are
real transient-world tests that drive native paths, not only policy headers. That is a genuine strength. Key TDD behaviours
without tests:

- a participant destroyed without death (C03/EA2-04);
- summon cleanup on success, phase transfer or retry (EA2-01), and summons' coordinator binding (EA2-02);
- ranged admission when the selection target differs from the payload target (EA2-03);
- the FocusTarget/DefendPerson approach (EA2-07);
- protagonist companion defeat when failure is refused (EA2-11);
- boss phase monotonicity across healing and durability re-arm (EA2-12);
- any statistical §8.11 or §12.8 acceptance harness (99.5% co-action, 0.4 s obstruction, 90% visible lethal attacks, AI count).

## 6. Domain alignment estimate

**Estimate for §8, §12, A.4, B.2 and D: 38–50%, midpoint 44%.**

The encounter director, checkpoint/retry transaction, wave and protection gates, bot-attack coordination (melee slots, role quotas,
relief, off-screen warning receipts), perception/threat memory with full debug fields, and Mass identity-preserving conversion
are engineered well above prototype level. The four prior enemy-side lifecycle findings and C02 are genuinely fixed with native
regressions. That work would support a figure in the 55–60% range for the engineering sections alone.

Five things pull the figure down:

1. Several TDD-named capabilities exist only as unreached APIs: C/D tiers, runtime tier changes, FocusTarget/DefendPerson commands, and three stimulus producers.
2. The one shipped boss has a lifecycle leak (EA2-01), a coordination bypass (EA2-02), a warning bypass (EA2-03) and a punitive failure rule (EA2-06).
3. Companion contribution is structurally capped below its acceptance band.
4. Commander slot changes, Tarrik challenge intent, civilians, Remnant threats, the Appendix D specs and most B.2 fields are absent.
5. Every §8.11 and §12.8 acceptance criterion lacks measurement.

This is a narrower and later-evidence denominator than the 11 September combined combat+AI range of 42–57%. My midpoint sits
near the low-middle of that range because the recent fixes are offset by the new boss and companion defects and by unreached APIs.
