# Adversarial audit: campaign, narrative, progression, world, checkpoints and travel

Snapshot: `F:\ProjectVelkorran`, HEAD `f07538c9` on branch `codex/aurelion-tdd-content-20260913`, with the uncommitted working tree listed in git status (binary Content and `.uproject` edits only; no source changes). This was a read-only source review. Nothing was built, run or launched in the editor. Binary Content was listed but could not be read.

TDD authority: `Docs/Design/Sovereign_Call_Origins_TDD_v2_2026-08-14.md` ("August TDD"), with the approved changes in `Docs/CampaignV2ChangeLog.md`. None of those six changes (ability rosters, health recharge, Cinderline ammunition, kill drops, primary-fire variation) touches this domain, so no finding below depends on them.

Paths are relative to `Source/ProjectVelkorran/` unless they start with `Plugins/`, `Docs/`, `Content/` or `Source/ProjectVelkorranTests/`.

## 1. Scope

| TDD section | Covered |
|---|---|
| §1.4 content budget | Compared against the Content listing and the native mission definitions |
| §3 campaign and adaptation (3.1–3.7) | Mission manifest, protagonist ownership and handoff, mission grammar, hubs, choice envelope |
| §9 narrative systems (9.1–9.14) | Consequence records, choices, dialogue adapters, relationship memories, evidence, viewmaker, mission and objective state, objective presentation, interruption, validation |
| §10 progression (10.1–10.10) | Technique ledger, rewards, safe points, augments, field recovery, rewards policy |
| §11 world and interaction (11.1–11.11) | Interaction, doors and lifts, travel, checkpoints, manual saves, streaming contracts |
| Appendix B.3, B.4, C, E, F | Dialogue node fields, checkpoint actor contract, mission brief, canon guardrails, definition of done |

Primary files read in full or in the relevant sections:

- **Campaign:** `Campaign/SovCampaignStateComponent`, `SovCampaignDefinition`, `SovAurelionMissionDefinition`, `SovNarrativeTypes`, `SovCampaignNarrativeQueries`, `SovEvidence*`, `SovAurelionPriority*`, `SovAurelionCheckpoint`, `SovAurelionWorldPresentation`, `SovCampaignInteractionTerminal`, `SovCampaignHandoffAnchor`, `SovAurelionRequestActor` (travel section)
- **Narrative:** `Narrative/*` (cue arbiter, adapters, validation library, viewmaker)
- **Progression and recovery:** `Progression/*`, `FieldRecovery` (headers)
- **Save:** `Save/SovSaveSubsystem` (admission, checkpoint, autosave, load, acknowledgement), `Save/SovMissionTravelRecovery.cpp`, `Save/SovSavePolicy.h`
- **Framework:** `Framework/SovPlayerController` (transition, handoff, travel), `SovCampaignGameMode`, `SovApplicationLifecycleComponent`
- **World and UI:** `World/SovWorldTransitActor`, `Recovery/SovFatalRecoveryComponent` (retry), `Cinematics/SovCampaignCinematicComponent` (request path), `UI/SovAccessibleRecordMenu`, `UI/Dialogue/SovDialoguePresentationComponent`, `UI/SovFrontendComponent` (objectives), `Validation/SovValidateCampaignCommandlet`
- **Plugin:** `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/Dialogue.cpp` (completion and pause guards)
- **Tests:** the test names and the relevant bodies in `Source/ProjectVelkorranTests/Private/Tests`

Context documents were treated as claims only: `Docs/TDDAlignment-2026-09-11.md`, `Docs/AurelionTDDAlignment-2026-09-13.md` and `Docs/AurelionTDD90Blockers-2026-09-15.md`.

**Not established by this audit:** anything that needs map contents, Blueprint graphs, the actual Narrative dialogue or quest assets, or runtime behaviour.

## 2. Denominator: what content exists

| §1.4 / §3.3 budget item | Baseline | Found in any form | Evidence |
|---|---|---|---|
| Major missions, prologue and epilogue | 17 (P00, M01–M16, I11, E17) | **2 with maps and data** (M12, M13); 2 more as native beat skeletons only (M01, M02); 13 absent | `Content/Aurelion/Maps/L_Aurelion_M12.umap`, `L_Aurelion_M13.umap`; `Content/Aurelion/Data/DA_M12_FireAndFrost`, `DA_M13_ContraryWitness`; `Campaign/SovCampaignDefinition.cpp:453-503` (M01 and M02 have no data asset or map anywhere in Content) |
| Hubs | 5 | 0 | No hub map or definition |
| Boss or command encounters | 8–10 | 0–1 (the M12 E4 Eclipse Elite and Thermal Fracture phase at most) | `SovAurelionMissionDefinition.cpp:224-233` |
| Authored combat arenas | 25–35 | About 5 (M12 E1, E2, E3, E4A, E4B); M13 has none | Same file, :159-233 |
| Optional investigation or rescue spaces | 20–30 | About 2 (M12 priority choice with the west medical cache or east flank) | `SovAurelionPrioritySupport.cpp`, `SovAurelionMedicalCache.cpp` |
| Progression branches (3 per protagonist, 8–10 nodes each) | 6 | 0 (no Technique skill or perk asset in any local Content) | `find Content -iname "*Technique*"` returns nothing; tracked Content has only the Narrative plugin's demo perks |
| Dialogue graphs, quests, narrative cues | Campaign-wide | 0 | No Dialogue, Tale, Quest or cue assets in `Content/` or `Content/Aurelion` |
| Evidence records | Campaign-wide | 3 (Fifth Witness, Cauldron recorder, Record 7283) | `Content/Aurelion/Evidence/*` |
| Story sequences | Campaign-wide | 16 Aurelion level sequences (their quality cannot be read) | `Content/Aurelion/Cinematics/LS_*` |

Honest denominator: about **12%** of the mission manifest (2 of 17) exists as playable content, about **24%** (4 of 17) if native skeletons count, and **0%** of hubs, progression trees and dialogue.

## 3. TDD coverage table

Legend: **I** implemented · **P** partial · **M** missing · **A** adapted (functionally mapped to Narrative Pro under §19.2 "retain/adapt") · **DC** deliberately changed in the change log (none here).

| Subsection | Status | Evidence and notes |
|---|---|---|
| 1.4 Content budget | P (about 8% of budget) | See §2 |
| 3.1 Adaptation rule | P | Canon facts for M01, M02, M12 and M13 are protected writes: `SovCampaignDefinition.cpp:475-477,495-502`; `SovAurelionMissionDefinition.cpp:193-305`. There is no content for the other 13 canon gates |
| 3.2 Three-act structure | M | No Act entity or act-level state |
| 3.3 Mission manifest | P (2/17) | See §2. `SovValidateCampaignCommandlet.cpp:359-360` requires M01 and M02 assets, which do not exist (CN2-14) |
| 3.4 Mission ownership and handoff | I | Handoff-only beats, anchors, ordering validation (`SovCampaignDefinition.cpp:155-163,306-369`) and a free-switch block (`SovCampaignStateComponent.cpp:270-275`); `SovPlayerController.cpp:860-881`. Tested by `SovHandoffRuntimeTests.cpp` |
| 3.5 Mission grammar | P | The beat schema carries canon gate, lead, writes, choices and checkpoints. Brief-level fields (player question, exit state, technical risks) are only in docs for Aurelion (`Docs/AurelionAdaptationContract-2026-09-06.md`, `AurelionLayoutContract-2026-09-07.md`) |
| 3.6 Hubs | M | None |
| 3.7 Choice envelope | P | Choice groups with reconciliation and no canon writes (`SovCampaignDefinition.cpp:42-86`). One human-tier choice is authored (M12 ImmediateProtection) |
| 9.1 Goals | P | Save-migration resilience is weak (CN2-10) |
| 9.2 No morality grid | I | No morality or approval code in game source; commandlet rejects `/Morality/` roots (`SovValidateCampaignCommandlet.cpp:145`) |
| 9.3 Consequence records | I (schema) / P (use) | Every field is present: `Campaign/SovNarrativeTypes.h:35-65`. Timestamp is `Sequence` plus `PlaySeconds`. `Persistence` is never enforced (CN2-16) and consumers are not validated (CN2-07) |
| 9.4 Choice categories | P | Writing lenses only; one choice authored |
| 9.5 Protagonist boundaries | Editorial | Not verifiable from source |
| 9.6 Dialogue interface | P (engineering) | Reply revisions, silence and pressure validation (`SovNarrativeValidationLibrary.cpp:57-75`), choice widget recovery. No authored dialogue |
| 9.7 Dialogue graph architecture | A / P | Narrative Tales graphs plus `USovCampaignNarrativeCondition` and `USovCampaignNarrativeEvent` replace `USovDialogueSubsystem`. The validator covers unreachable nodes, cycles, fallback, speakers and subtitle timing. It does not check localization IDs outside shipping mode, cinematic participants or "writes to unknown flags". CN2-06 covers the event adapter |
| 9.8 Relationship memories | P | Schema, witness validation and queries exist (`SovCampaignStateComponent.cpp:341-353`; `SovCampaignNarrativeQueries.cpp:114-121`). **No authored memory in M12 or M13**, so the system is unused |
| 9.9 Evidence and witness | I (engineering) / P (content) | Provenance stages, custodians, authentication and distribution (`SovCampaignNarrativeQueries.cpp:42-94`); critical-path acquisition at mandatory beats (`SovCampaignStateComponent.cpp:354-368`); replay validation. Three records exist |
| 9.10 Viewmaker | P / M (integration) | Cone, LOS, weak-point and link logic exist (`Narrative/SovViewmakerLibrary.cpp`), but nothing in production calls them (CN2-09) |
| 9.11 Mission and quest state | P | Seven objective states and a policy (`Public/Campaign/SovObjectivePolicy.h`), a journal, replay validation and 10 objective types. **No Act or Sequence levels** (CN2-11). `CompleteBeat` can be called from Blueprint but is validated |
| 9.12 Objective presentation | I | Actionable, knowledge-filtered, optional marking and failure-rule text (`UI/SovFrontendComponent.cpp:185-205`); hide toggle (`UI/SovAccessibilitySettingsMenu.cpp:162`). Well tested |
| 9.13 Interruption and recovery | P | Priority tiers, combat suspension, critical requeue for barks, unheard archive. Gaps: CN2-04 and CN2-05. The archive is read-only (UI-07 partial) |
| 9.14 Narrative validation | P | Beat DAG, canon-fact conflicts, choice legality and dialogue graph checks exist. Missing: canon reachability across missions, consumer checks, knowledge-before-acquisition checks in dialogue, conversation save boundaries |
| 10.1 Philosophy | I | No XP, levels or gear score. `GiveSkillPoints` is disabled (`Progression/SovTechniqueComponent.cpp:54-58`) |
| 10.2 Technique Points | P (engineering) / M (wired) | Budget 18–22, proof-backed rewards, no farming, free respec. **No caller and no reward sources** (CN2-08) |
| 10.3 Node rules | P | Grant policy, incompatibility and investment rules exist; telemetry and UI preview hooks do not |
| 10.4 Tree structure | P | Three branches, 24–30 ranks, acyclic (`SovTechniqueComponent.cpp:102-159`). No trees authored |
| 10.5 Augments and loadout | P | Selection at safe points with rollback (`SovTechniqueComponent.cpp:385-429`); no caller |
| 10.6 Equipment refinement | P | Only as perk grants; no refinement data |
| 10.7 Inventory policy | P | Cinematic inventory transactions exist. The earlier alignment document reports template inventory UI (`Docs/TDDAlignment-2026-09-11.md:93`); not re-verified in binary content |
| 10.8 Healing and consumables | I / P | Capacity 2, stations (`Public/FieldRecovery/SovFieldRecoveryComponent.h:22,54`). No automatic refill at major checkpoints |
| 10.9 Rewards | P | Technique, evidence and staging rewards exist; one Aurelion instance |
| 10.10 Progression balance | Unverifiable | No content |
| 11.1 World structure | P | Wide-linear maps with travel; no replay menu (NG+ deferred) |
| 11.2–11.5 Level design, languages, exploration, navigation | Content | Graybox Aurelion only; waypoint and objective toggle exist |
| 11.6 Interaction | A / I | Narrative interaction component plus native owners (terminals, transit, priority) with range, LOS, busy and alive checks; hold scale setting. Evidence acquisition lacks some gates (CN2-12) |
| 11.7 Doors, lifts, travel | I | `World/SovWorldTransitActor.cpp` (power, lock, structure, nav links, streaming timeout, checkpoint); mission travel transaction (`Save/SovMissionTravelRecovery.cpp`). CN2-01 and CN2-15 apply |
| 11.8 Checkpoints | P | Entry, pre-cinematic, pre-choice and pre-transition writes go to the checkpoint slot. Mission start, arena exit and canon gate go only to rolling autosaves, and fatal retry ignores them (CN2-02). Admission contract: `Save/SovSaveSubsystem.cpp:413-490` |
| 11.9 Manual saves | I | 10 manual, 3 auto, 1 checkpoint (`Save/SovSavePolicy.h:9-10`); unsafe-state admission; write and readback. No playthrough identity (CN2-03) |
| 11.10 World Partition and streaming | P | Cinematic partition sources and transit streaming timeouts exist; HLOD and data layers are content |
| 11.11 Level acceptance | Runtime | `Docs/AurelionTDD90Blockers-2026-09-15.md` claims a 30/30 CP0 soak and one unattended M12→M13 route. These are claims, not qualification |
| B.3 Dialogue node fields | P | The Narrative node has ID, speaker, lines, conditions, events and replies. Missing per-node interruption priority, schema version and localization or performance status |
| B.4 Checkpoint actor contract | P | Narrative savable GUIDs and restore phases; some components carry a schema version; fail-closed load. Missing an explicit capture policy, per-actor migration and dependency declarations |
| C Mission brief template | P | Only Aurelion has docs approximating a brief |
| E Canon guardrails | P | Enforced where data exists (protected facts, no choice may write a canon key); editorial otherwise |
| F Definition of done | M | 2 of 17 missions, no hubs, no epilogue or credits path; `bCompletesCampaign` is reserved for an E17 that does not exist (`SovCampaignDefinition.cpp:124`) |

## 4. Findings

Severity: **P1** blocks acceptance, loses progress or corrupts state · **P2** significant · **P3** minor. Evidence class: **SP** source-proven defect · **MI** missing integration · **RT** needs runtime or content evidence.

### CN2-01 — Checkpoints that must be written before a canon scene, the M12 choice or mission travel ignore an acknowledged save failure, so persistent low storage permanently blocks M12 (P1, SP, high confidence; extends prior C04)

**TDD:** 11.7 (explicit, recoverable transitions), 11.8, 18.9 (low-storage handling), 15.16.

**Existing protections:**

- Arena entry, traversal and transit call `ConsumeAcknowledgedBoundary` before writing: `Campaign/SovEncounterDirector.cpp:441-442`, `World/SovTraversalAnchor.cpp:107-108`, `World/SovWorldTransitActor.cpp:192-193`.
- A player's "continue without saving" choice records an exact receipt: `Save/SovSaveSubsystem.cpp:1024-1033`.
- The consume function is at `Save/SovSaveSubsystem.cpp:1011-1024`.
- `WriteCheckpoint` itself never consumes a receipt: `Save/SovSaveSubsystem.cpp:683-690`.

**Defect.** These irreversible owners call `WriteCheckpoint` directly and abort on failure:

- **Every required cinematic:** `Cinematics/SovCampaignCinematicComponent.cpp:555`. There are 11 in M12 and M13 combined.
- **The only authored choice:** `Campaign/SovAurelionPriorityTerminal.cpp:170`. CP4b must succeed before `ResolveChoice`.
- **Mission travel:** `Framework/SovPlayerController.cpp:771` passes `bRequireDurable = true` into `PrepareTransitionCheckpoint` (`:334-335`), which bypasses the receipt. `ArmMissionTravelRecovery` then also requires a verified checkpoint for the current mission (`Save/SovMissionTravelRecovery.cpp:77-82`).
- **Checkpoint terminals:** `Campaign/SovCampaignInteractionTerminal.cpp:189` does not complete its boundary; `Campaign/SovAurelionCheckpoint.cpp:86` does the same.

**Failure sequence.** Storage is full or the platform write fails.

1. The player reaches `LS_MeetingAndCarrierRescue`. `RequestPlay` calls `WriteCheckpoint(CanonGate)`, which returns `WriteFailed` and puts up the pause and decision UI.
2. The player chooses "continue". `AcknowledgeSaveFailure` records a receipt for `M12_MeetingAndCarrierRescue`.
3. The player retries the scene. `WriteCheckpoint` runs again, fails again, and the scene never plays.

`MeetingAndCarrierRescue` is a mandatory prerequisite of every later beat, so M12 cannot progress. The priority panel (CP4b) and the travel to M13 fail in the same way. The encounter, transit and traversal paths already show the intended contract.

**Fix (existing owners).**

- Route all irreversible pre-boundary writes through one `USovSaveSubsystem` helper, for example `EnsureBoundary(Kind, Id)`. It accepts either a successful write or an exact, unexpired, owner-matching receipt.
- Use it in the cinematic component, the priority terminal, the interaction terminal and the Aurelion checkpoint.
- For mission travel, either accept the receipt and retain an in-memory origin snapshot (the `MissionTravelOrigin` mechanism already holds decoded bytes), or show an explicit "cannot travel without storage" recovery screen offering "free space / return to title". The current path is a silent dead end.

**Tests to add:**

- Force a checkpoint write failure, acknowledge the exact beat, then call the next `RequestPlay`, `Execute` or travel. It proceeds once without another write.
- A receipt for the wrong beat, world, protagonist or account is rejected.
- No test currently covers this; `SovCinematicLifecycleRuntimeTests.cpp` has no acknowledgement case.

### CN2-02 — Fatal retry always prefers the checkpoint slot, which is written only before boundaries; progress recorded after an arena victory, a canon gate or a mission start is skipped, and after cross-map travel the slot still points at the previous mission (P2, SP, high confidence; impact depends on content)

**TDD:** 11.8 (checkpoint at mission start, after major arenas, after a canon gate and its writes; "no lost irreversible choice"), 11.11 (reliable recovery at canon gates).

**What gets written where:**

- Mission start queues only a rolling **autosave**: `Framework/SovPlayerController.cpp:667-668`.
- Arena exit queues only an autosave: `Campaign/SovEncounterDirector.cpp:170-172`.
- Canon-gate commit and mission success queue only an autosave: `Campaign/SovCampaignStateComponent.cpp:418-419`.
- Autosaves are single-entry and coalesce to the latest (`Save/SovSaveSubsystem.cpp:691-698`). They wait for full safety, including no hostile pawn within 15 m (`:474-486`).
- Fatal retry loads `Checkpoint 0` first and falls back to an autosave only if that load itself fails: `Recovery/SovFatalRecoveryComponent.cpp:307-316`.

**Failure sequences:**

1. **Arena win lost.** The player wins M12 E2; the canon-gate autosave is written. The player then dies to a hazard or stray enemy before CP3. Retry loads the E2 *entry* checkpoint, and the won encounter must be replayed.
2. **Travel back to M12.** M12→M13 travel writes the checkpoint slot in M12 (`SovPlayerController.cpp:771`). In M13 the only checkpoint-slot writers before CP7 are the pre-cinematic writes, and the start of M13 queues only an autosave. A death or retry before the first M13 scene request loads the M12 origin checkpoint and sends the player back to the M12 map with M12 state.
3. **Canon scene replayed.** A cinematic canon gate commits and is autosaved; the player dies before the next checkpoint. Retry loads the pre-scene checkpoint, and the scene must be watched again. `ViewedCinematics` reverted with the checkpoint, so it cannot be skipped (`SovCampaignStateComponent.cpp:376,439-444`).

The Aurelion manual actors (CP0, CP2, CP3, CP6–CP9) mitigate the M12 cases. They do not make the engine contract hold, and future missions will inherit it.

**Fix.**

- Either write the checkpoint slot (as a queued, safety-gated write) at the three boundaries the TDD names, or have fatal retry select the newest verified same-campaign, same-mission record across the checkpoint slot and recovery autosaves (by generation plus mission identity).
- Never let retry cross a mission boundary backwards unless the player explicitly chooses to.

**Tests:** die after an arena exit before the next checkpoint; die in M13 before the first scene; die after a canon gate. Assert the restored mission, beat set and viewed-scene set.

### CN2-03 — Saves carry no playthrough identity, so fatal retry and "load checkpoint" can restore another campaign run (P2, SP, medium-high confidence; the window depends on content)

**TDD:** 11.9 (manual load returns to the nearest compatible state and clearly shows it), 15.9.

**Evidence:**

- The save header has account, mission, generation and map, but no campaign or playthrough identifier: `Public/Save/SovCampaignSaveGame.h` (header fields).
- `LoadSlot` validates product, account, schema and assets only: `Save/SovSaveSubsystem.cpp:798-828`.
- Fatal retry and the Aurelion pause menu load `Checkpoint 0` without comparing it to the live campaign: `Recovery/SovFatalRecoveryComponent.cpp:307`; `UI/SovAurelionPauseMenu.cpp:201`.
- No source path clears or retires the checkpoint slot when a new campaign starts; a fresh campaign is admitted only because the state is empty (`SovCampaignStateComponent.cpp:181`).

**Failure sequence.** Run A reaches M13 CP8. The player starts a new M12 run on the same account and dies (or uses "Load checkpoint") before the first new checkpoint write; CP0 needs the player inside its threshold and a safe capture. Retry loads run A's M13 checkpoint. Only the priority terminal's travel path checks mission identity (`SovMissionTravelRecovery.cpp:80-81`).

**Fix.** Add a `CampaignRunId` GUID to `USovCampaignStateComponent` (saved) and to the save header. Retry and "load checkpoint" must require the same run, or else fall back to explicit slot selection. Starting a new campaign should mark earlier checkpoint and autosave slots as belonging to another run rather than deleting them.

### CN2-04 — A critical conversation cue playing at save time is neither saved nor archived; loading silently discards it (P2, SP, high confidence on mechanism; RT on reachability)

**TDD:** 9.13 (canon-critical lines defer or replay at the next valid point), 9.14 (save and load inside a conversation resumes or safely restarts at a declared boundary).

**Evidence:**

- `PrepareForSave_Implementation` captures only `CurrentBark`: `Narrative/SovNarrativeCueComponent.cpp:284-288`.
- When a Dialogue-backed cue starts, it is removed from `Pending` (`:243`) and held only in the transient `CurrentConversation`.
- `Load_Implementation` nulls `CurrentConversation` and exits the old dialogue (`:289-297`). It never calls `RememberUnheard` or requeues, and `HandleDialogueFinished` then returns early because `OwnedDialogue` is already null (`:275`).
- Save admission blocks only when the player has `State_DialogueControlled` or `State_Interacting` (`Save/SovSaveSubsystem.cpp:455-456`). A walk-and-talk graph that does not apply that tag can be captured by a queued autosave.
- The test `SovNarrativeCueRuntimeTests.cpp:118-136` covers a critical *bark*, not a conversation.

**Failure sequence.** A critical walk-and-talk cue whose graph ends with a `USovCampaignNarrativeEvent` CompleteBeat is playing. An arena-exit autosave is captured. The player later reloads it. The cue does not replay, is not in `UnheardRecords`, and the beat never commits. If content has no other trigger, the objective is stuck.

**Fix.** In `PrepareForSave`, also capture `CurrentConversation` into `InFlightCriticalSave` when it is `bCritical`, reusing the same field; on load, requeue it. Alternatively, have save admission treat any active owned critical conversation as "unresolved" (the `UnresolvedChoice` path). Add a conversation variant of `CriticalSaveAndCallbackIsolation`.

### CN2-05 — Pending critical cues are permanently deleted when the context briefly mismatches: a handoff, an invalid state during load, or a mission change (P2, SP, high confidence)

**TDD:** 9.13.

**Evidence.** `Narrative/SovNarrativeCueComponent.cpp:221-224` removes every pending request for which `MatchesContext` fails, with no `bCritical` exception and no archive. `MatchesContext` (`:49-56`) fails whenever:

- `IsStateValid()` is false, which includes a load in progress;
- the active protagonist differs from `RequiredProtagonist`, which happens after every M12 or M13 handoff;
- the active mission differs.

`RequestAuthoredHandoff` does not refuse while a conversation is running (`Framework/SovPlayerController.cpp:869-872`). It checks Busy and Sequencer, but not `Tales->IsInDialogue()`.

**Failure sequence.** A critical Tarrik line is queued but deferred by combat (`MayPlayInCombat`). The encounter ends, and the player immediately uses the Selene handoff anchor. On the next tick the Tarrik cue is removed and never replayed; if it was archive-eligible, its summary is not recorded.

**Fix.** For `bCritical` cues, keep the request while the mismatch is protagonist- or mission-scoped (it can become valid again after the reverse handoff), or `RememberUnheard` and drop it only when the context can never recur. Treat an invalid state as "hold", not "drop". Refuse authored handoffs while an owned critical conversation is active.

### CN2-06 — The Narrative dialogue event adapter discards `CompleteBeat` failures and never refires, so a dialogue-authored beat or choice write can be lost (P2, SP mechanism, RT reachability)

**TDD:** 9.7 (consequence writes, mission events), 9.11 (every transition validated and journaled), 9.14.

**Evidence.** `Narrative/SovCampaignNarrativeAdapters.cpp:91` calls `State->CompleteBeat(BeatId, false)` and ignores the result. The event is `EEventRuntime::End` with `bRefireOnLoad = false` (`:78-82`). `CompleteBeat` returns `Busy` inside any campaign mutation or notification (`SovCampaignStateComponent.cpp:246`), `Invalid` when the current pawn is not the active protagonist (`:248-249`, for example during a handoff respawn), and `ProtectedStateConflict` and others. Validation forbids proof beats but allows choice-outcome beats (`:138-146` in the adapter), so a dialogue choice can be the only writer of a choice group.

**Failure sequence.** A dialogue node's End fires from a callback inside `OnBeatCommitted` (the notification runs under the mutation guard, `SovCampaignStateComponent.cpp:396-405`). `CompleteBeat` returns `Busy` and the choice is dropped. The dialogue has already exited, the reconciliation beat waits forever, and a reload does not refire.

**Fix.** On a non-terminal result (`Busy`), defer to the next tick with the dialogue identity and revision; on a hard failure, keep the conversation open or log and surface it. Add a validator rule that a choice-group outcome beat may not be written only by a dialogue End event without retry semantics.

### CN2-07 — Consequence "consumers" are unchecked labels; the declared M13 consumer does not exist natively, and no relationship memory is authored (P2, MI, high confidence)

**TDD:** 9.3 (reactions derive from knowledge), 9.8, 9.14 ("consequence writes have downstream consumers or an explicit archival-only purpose").

**Evidence:**

- `FSovConsequenceDefinition::Validate` requires only that `ConsumerIds` is non-empty: `Campaign/SovNarrativeTypes.cpp:33`.
- M12 declares `{"M12_PriorityEvacuation", "M13_PriorityAftermath"}`: `Campaign/SovAurelionMissionDefinition.cpp:217`. Neither ID appears in any other source file.
- The only native reader of the priority is `ASovAurelionPrioritySupport` / `ASovAurelionPriorityTerminal`, which read `M12_FireAndFrost` choice state (`SovAurelionPrioritySupport.cpp:31`) and gate M12 barriers. `GetAftermathConsequenceId` has no C++ caller.
- M13 declares no `RequiredPriorConsequenceIds` and no beat reads it (`SovAurelionMissionDefinition.cpp:245-306`).
- The commandlet checks successor and inherited-consequence writers (`Validation/SovValidateCampaignCommandlet.cpp:317-357`) but not consumers.
- No beat in M01, M02, M12 or M13 authors `RelationshipMemories`.

**Consequence.** The only campaign-persistent choice has no verified later effect. The "M13 aftermath" may exist in a Blueprint (RT) but nothing enforces it.

**Fix.** Make consumer IDs resolvable: a registry of consumer IDs declared by native readers, dialogue conditions (`USovCampaignNarrativeCondition` with `ConsequenceKnown`) or cue assets. Have the commandlet fail when a non-archival consequence has no resolvable consumer in the manifest. Author the M13 aftermath reader, or mark the record archival-only.

### CN2-08 — Technique progression is engineered but not wired: nothing earns, spends, respecs or selects augments, and no trees exist (P2, MI, high confidence)

**TDD:** 10.2–10.5, 2.6 progression loop, Appendix F.

**Evidence:**

- `USovTechniqueComponent::ClaimReward`, `RespecAtSafePoint`, `SelectAugmentAtSafePoint` and `GetSelectedAugment` have **no callers** outside `Progression/SovTechniqueComponent.cpp` and tests. No UI or menu in `UI/` references them.
- No Technique skill, perk or reward-source asset exists in Content (§2).
- `HasValidActiveTree` requires exactly three branches with 24–30 ranks (`SovTechniqueComponent.cpp:129`), so `CanModifyTechniques` is always false in the current build.
- `GiveSkillPoints` is intentionally a no-op (`:54-58`). Technique Points are therefore unearnable.

This is not a corruption defect. The ledger's exactly-once handling is sound: unique `RewardId` plus the encounter `ClaimCompletionReward` latch (`:243-256`), stored per protagonist snapshot. Replaying a reward on reload restores to the checkpoint's ledger.

**Fix:**

- Add an authored reward trigger on mission success and beat commit (for example, `USovTechniqueRewardSource` actors or a beat field `TechniqueRewardId` claimed inside `CompleteBeatInternal` after commit).
- Add a Techniques menu bound to the safe-point API.
- Author the 6 branches.
- Test: complete M12, claim once, reload the pre-claim and post-claim checkpoints, hand off to Selene, and assert ledger isolation.

### CN2-09 — The viewmaker investigation layer has no production entry point (P2, MI, high confidence)

**TDD:** 9.10.

**Evidence.** `USovViewmakerLibrary::ScanTarget` and `RequestCompanionAnalysis` (`Narrative/SovViewmakerLibrary.cpp:37-73`) and `USovEvidenceSourceComponent::TryScan` are called only by each other and by `SovNarrativeStateRuntimeTests` (ViewmakerPhysicalBounds). No Selene input action, ability or HUD in source invokes a scan. Blueprint use is RT, but no Aurelion evidence source uses `bRequiresViewmaker` (the three evidence records are critical-path beat grants).

**Fix.** Add a Selene viewmaker ability (existing GAS ability base) that resolves a focused target and calls `ScanTarget`, plus a presentation surface for trace IDs and route hints. Author at least one optional scan-gated source in M12 or M13 to prove the loop.

### CN2-10 — Any content revision to a mission definition invalidates every existing save; there is no migration beyond schema 1→2 (P2, SP, high confidence)

**TDD:** 9.1 (survive save migration and cinematic revision), 15.9, B.4 (migration behaviour).

**Evidence:**

- `ValidateSavedState` replays the entire journal against the *current* definition objects and requires `ValidateDefinition` to pass for each (`Campaign/SovCampaignStateComponent.cpp:553-842`, especially `:567`, `:700-702`, `:705` for the exact consequence and memory equality, and `:816-826`).
- Aurelion's `ValidateAurelionContract` requires exact equality with the native constructor contract: `Campaign/SovAurelionMissionDefinition.cpp:317-344`.
- A failure sets `bStateValid=false` (`:849`), after which every mutation returns `NotAuthority` and all queries return empty. The only migration is `MigrateLegacyObjectives` (1→2, `:895-911`). The save header's `SchemaMinor` does not version mission content.

**Failure sequence.** A patch adds an optional beat to M12, rewords a consequence's `SubjectIds`, or adds a relationship memory. Every save taken in M12 or M13 now fails validation on load. Fail-closed is correct for tampering; for legitimate content evolution there is no path.

**Fix.** Version `USovCampaignDefinition` (a content revision) and record the revision seen per mission in the journal. Add registered per-mission migrations (the precedent is the existing `MigrationHistory`). Distinguish "tampered" from "authored revision". A test should load a v1 M12 journal into a v2 definition with an added optional beat.

### CN2-11 — The mission hierarchy has no Act or Sequence level, and a fresh campaign may start at any mission, including one whose first beat can never be reached (P3, SP)

**TDD:** 9.11 (Campaign › Act › Mission › Sequence › Beat › Objective), 3.2.

**Evidence.** There is no Act or Sequence type. `CanEnterMission` admits any definition when the campaign is empty (`SovCampaignStateComponent.cpp:181`). M13's first beat requires the M12 fact `ContraryWitnessesRecognized` (`SovAurelionMissionDefinition.cpp:254-255`), but M13 declares no `RequiredPriorConsequenceIds` or entry requirement. A boot directly into `L_Aurelion_M13` with `BP_AurelionGameMode_M13` creates a campaign that can never progress. This is acceptable for development, but it is not a shipping guard.

**Fix.** Add a definition-level `EntryRequiredState` (checked in `CanEnterMission`), or an explicit `bCampaignEntryPoint` flag with the commandlet asserting exactly one entry. Consider Act and Sequence grouping for journal and UI.

### CN2-12 — Evidence acquisition is not gated on life, fatal, cinematic or traversal state (P3, SP)

**TDD:** 11.6 ("failure to align never consumes a resource or writes state"), 9.9.

**Evidence.** `USovCampaignStateComponent::AcquireEvidence` checks the owner, mission, protagonist identity, range and LOS (`Campaign/SovCampaignStateComponent.cpp:469-522`), but not alive, `State_Fatal`, `State_SequencerControlled` or `State_Traversal`. The dialogue-event path and a Blueprint `TryAcquire` can therefore write evidence, and grant knowledge that unlocks objectives, during death or a scene. Contrast the terminals (`SovAurelionPriorityTerminal.cpp:111-139`).

**Fix.** Reuse the terminal readiness predicate in `AcquireEvidence`.

### CN2-13 — Full definition validation runs on hot paths (P3, SP performance)

**Evidence.** `ValidateDefinition` is called:

- on every `CompleteBeatInternal` and `TransitionObjective` (`SovCampaignStateComponent.cpp:140,248`);
- in `CanEnterMission` (`:178`);
- in every priority-terminal `Validate` (`SovAurelionPriorityTerminal.cpp:117`), which Narrative calls from `CanInteract` while the terminal is focused;
- in `SovCampaignInteractionTerminal::CanUseInternal` (destination) and `SovAurelionRequestActor.cpp:220`.

For Aurelion it walks roughly 30 beats with ancestor sets and calls `LoadSynchronous` on every story sequence (`SovAurelionMissionDefinition.cpp:362`) and on the companion classes and definitions (`SovCampaignDefinition.cpp:411-413`). This is cheap once loaded, but a synchronous-load hitch if GC has evicted a sequence. There is no measured frame cost here; confirm with a capture.

**Fix.** Cache the validation result per definition object and content revision (invalidate on `PostEditChange`), and keep synchronous loads out of interaction polling.

### CN2-14 — The campaign validation commandlet cannot pass on current content (P3, SP)

**Evidence.** `Validation/SovValidateCampaignCommandlet.cpp:359-360` fails unless `M01_Mantle` and `M02_OneDegree` assets are in the manifest, and none exist. Either the gate is not run in the pipeline, or it is red. Any "validation passed" claim for campaign data should be checked against this.

**Fix.** Make the opening-mission requirement conditional on a shipping-manifest flag, or author the M01 and M02 data assets.

### CN2-15 — Mission travel recovery runs on a wall-clock deadline and can start an origin `OpenLevel` while a slow but valid destination load is still in progress (P3, SP; residual of C01)

**Evidence.** `Save/SovMissionTravelRecovery.cpp:19` sets `TimeoutSeconds = 120`, measured in `FPlatformTime` (`:93`). `TickMissionTravelRecovery` starts origin recovery once the deadline passes (`:240-242`), whatever the destination load phase. Cold packaged loads on slow storage or console were not measured.

**Fix.** Extend the deadline while the engine reports an active map load or while the destination world is admitted but readiness is pending, and bound it by the progress signal, not only elapsed time.

### CN2-16 — Consequence `Persistence` and `CanonClass` are stored but never used (P3, MI)

**Evidence.** No source outside `SovNarrativeTypes.cpp` reads `Persistence` or `CanonClass`. Encounter- or mission-scoped records stay queryable for the whole campaign, so later content can react to a fact the TDD scoped as "current encounter".

**Fix.** Filter `FindConsequence` by persistence scope relative to the record's mission (and act, once acts exist), or have the validator reject short-persistence records with consumers outside their scope.

### CN2-17 — The unheard-record archive is display-only and not protagonist-filtered (P3, SP; residual of UI-07)

**Evidence.** `UI/SovAccessibleRecordMenu.cpp:170-176` lists every critical unheard cue summary, whatever the cue's `RequiredProtagonist` or `RequiredKnowledge`, and offers no replay. After a handoff, Selene's record view shows Tarrik-only summaries.

**Fix.** Filter with the cue's `MatchesContext`-equivalent knowledge check for the viewing protagonist. Add a replay action through `RequestCue` where the context is legal.

### Observations that are not defects (verified)

- **Choice durability after a checkpoint reload is exactly-once.** CP4b is written before the choice (`SovAurelionPriorityTerminal.cpp:168-172`); the consequence ID must be unique in the journal (`SovCampaignStateComponent.cpp:332-336`); CP5 is written after. Reloading CP4b re-offers the choice; reloading CP5 keeps it; replay validation rejects duplicates (`:709-713`).
- **Technique reward duplication across reload or handoff:** none found (CN2-08 notes).
- **Protagonist handoff mid-combat is refused** (`SovPlayerController.cpp:873-877`). Handoff mid-cinematic is refused by the Sequencer tag. A conversation is not refused (CN2-05).
- **Doors and lifts:** streaming timeout, power, lock, structure, a checkpoint before an irreversible transition, and companion boarding (`World/SovWorldTransitActor.cpp:185-247`) all have tests.

## 5. Prior-finding dispositions

| Prior ID | Topic | Disposition | Evidence |
|---|---|---|---|
| C01 (CampaignWorldAudit) | Async mission travel has no origin recovery transaction | **Fixed** (residual CN2-15; packaged multi-world evidence still absent) | GameInstance-owned transaction armed before travel (`Save/SovMissionTravelRecovery.cpp:58-98`); `OnTravelFailure`/`OnNetworkFailure` bindings (`:28-35`); destination identity checked in `InitNewPlayer` (`Framework/SovCampaignGameMode.cpp:36`, `SovMissionTravelRecovery.cpp:130-151`); readiness notify (`:182-196`); single recovery attempt plus explicit retry (`:198-275`); tests `SovTravelTransactionRuntimeTests.cpp` (accepted failure, stale failure, single attempt, owner epoch, suspend deadline) |
| C04 (CampaignWorldAudit) | Cinematic start ignores acknowledged save failure | **Still open, and wider** (CN2-01) | `Cinematics/SovCampaignCinematicComponent.cpp:555` unchanged; the same pattern appears in the priority terminal, interaction terminal, Aurelion checkpoint and durable travel |
| SP-03 (SavePlatformAudit) | Interruption does not freeze a travelling source world | **Fixed** | `Framework/SovApplicationLifecycleComponent.cpp:167-174` now includes `Travelling` in `bCanPause`; resume is refused outside Idle or Failed (`:182-187`). A native viewport test for travelling hazards is still not evident |
| UI-03 (UINarrativeAudit) | Queued dialogue completion advances a paused graph | **Fixed** | Tokenized completion deferred under world pause or suspension (`Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/Dialogue.cpp:450-474,1504-1505,1541-1542`) and replayed once on an unpaused tick (`:706-713`); test `SovDialogueNarrationRuntimeTests.cpp:261` (QueuedMediaCompletionAcrossWorldPause) |
| UI-05 (UINarrativeAudit) | Account switching keeps another account's settings. The brief called this "choice recovery"; the prior report's choice-recovery item is UI-06 | **Still open** (outside this domain; brief check only) | `Public/Settings/SovGameUserSettings.h` has no account or namespace keying; campaign completion unlock is still global (`SovCampaignStateComponent.cpp:413`, `SovPlayerController.cpp:671`) |
| UI-06 (UINarrativeAudit) | Removed choice widget leaves a live dialogue invisible | **Fixed** | `UI/Dialogue/SovDialoguePresentationComponent.cpp:171-176` resets `SeenRevision` on removal; tick re-presents (`:226-230`); test `SovDialogueNarrationRuntimeTests.cpp:303` (RemovedWidgetRebuildsCurrentChoices) |
| UI-07 (UINarrativeAudit) | Unheard critical archive has no consumer | **Partially fixed** (CN2-17) | Displayed in scene history (`UI/SovAccessibleRecordMenu.cpp:170-176`); no replay, and no protagonist or knowledge filter |

## 6. Test coverage against key TDD behaviours

**Covered well:**

- beat, commit and replay tampering (`SovObjectiveLifecycleTests`, `SovCampaignRuntimeTests`)
- evidence provenance and witnesses (`SovNarrativeStateRuntimeTests`)
- the exclusive choice and its projection (`SovAurelionPriorityRuntimeTests`, `SovObjectivePresentationRuntimeTests`)
- Technique ledger, grants and augment rollback (`SovTechniqueRuntimeTests`)
- travel transaction phases (`SovTravelTransactionRuntimeTests`, `SovMissionTravelRecoveryTests`)
- checkpoint banks and migration (`SovCheckpointContractRuntimeTests`)
- cinematic ownership (`SovCinematicLifecycleRuntimeTests`)
- transit and traversal (`SovWorldRuntimeTests`)
- the Aurelion native contract (`SovAurelionMissionContractTests`)

**No test found for:**

- acknowledged save failure on cinematic, choice or travel (CN2-01)
- fatal retry after a post-boundary autosave or after cross-map travel (CN2-02)
- cross-playthrough checkpoint load (CN2-03)
- a critical *conversation* across save and load (CN2-04)
- a pending critical cue across a handoff (CN2-05)
- a dialogue event commit failing with `Busy` (CN2-06)
- consumer existence (CN2-07)
- an end-to-end reward claim from real content (CN2-08)
- a content-revision save migration (CN2-10)
- the full-campaign canon reachability required by 9.14 (only a per-mission DAG exists)

**Structural limit:** most tests are isolated fixture worlds. The only end-to-end evidence is the scripted Aurelion route in `Docs/AurelionTDD90Blockers-2026-09-15.md` (one unattended M12→M13 pass and a 30-cycle CP0 soak), which is a claim outside this audit.

## 7. Alignment estimate for §1.4, §3, §9, §10, §11 and Appendix B.3/B.4/C/E/F

| Measure | Range | Midpoint |
|---|---|---|
| **Engineering foundation** | 55–65% | **60%** |
| **Authored content** | 5–11% | **8%** |
| **Combined** | 24–33% | **28%** |

**Justification.** The engineering for these sections is unusually deep and defensive:

- a replay-validated campaign journal with protected canon facts, choice groups and reconciliation;
- consequence records that carry every §9.3 field;
- a provenance-based evidence model;
- objective lifecycle and presentation;
- a transactional travel recovery that now closes C01;
- stateful doors and lifts;
- a tested, exactly-once Technique ledger;
- a large, mostly behaviour-level automation suite.

It falls short of the TDD in five ways:

1. Progression and the viewmaker are not wired to any production entry point (CN2-08, CN2-09).
2. The Act and Sequence hierarchy, relationship-memory use, consequence persistence and consumer enforcement are missing (CN2-07, CN2-11, CN2-16).
3. The checkpoint policy does not match §11.8's post-boundary placements, and retry can cross missions or playthroughs (CN2-02, CN2-03).
4. Narrative interruption still loses critical conversations in save, load and handoff cases (CN2-04, CN2-05, CN2-06).
5. One P1 progress-block path under storage failure remains open and has spread (CN2-01).

Content-revision save migration (CN2-10) is also absent.

Authored content is the dominant gap. 2 of 17 manifest missions exist as a graybox slice, with 0 of 5 hubs, 0 of 6 progression trees, no dialogue graphs, 3 evidence records and about 5 of 25–35 arenas. §1.4, §3 and Appendix F are almost entirely content, while §9–§11 are about half engineering. Weighting content at roughly 55% gives about 28%.

That is within the 2026-09-11 estimate of 22–30% for the wider "campaign, narrative, progression, world, companions" bucket. Since then, travel recovery, dialogue pause and widget recovery have been fixed. CN2-01 has widened, and the progression and viewmaker integration gaps are now explicit, so the number moved only modestly.
