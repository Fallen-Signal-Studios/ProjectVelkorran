# Aurelion AI hostile-acquisition delay — measured, 11 September 2026

**Correction, same day.** This document first reported an intermittent *stall* reproduced at
18/20. That finding was wrong and is retracted below. The startup ordering race is real and
deterministic, but its cost is a **~1.18 s delay in acquiring the attack goal**, not a failure
to acquire one. No run in any sweep, before or after the repair, ever failed to acquire it.

Measure with:

```powershell
.\Scripts\Run-AIStartupColdStarts.ps1 `
  -ArchiveDirectory '<archive>\Windows' -Runs 20 -HoldSeconds 25 -ApproachHostile
python Scripts\Classify-AIStartupRuns.py <after-directory> <before-directory>
```

Map `/Game/Maps/Development/L_SeleneCombat` (3 Dominion Hounds + Handler), packaged
Development, `sov.AIStartupTrace` armed through `-dpcvars`. 20 runs x 4 controllers = 80
samples per configuration.

## What the retracted claim got wrong

The original classification read each run's **last** snapshot and called "zero goals while
still seeing a hostile player" a stall. Re-examined per controller over time:

- **100% of goal drops occur because the player is already dead.** The NPC engaged, killed
  Selene, dropped the attack goal and correctly returned to spawn. A terminal snapshot taken
  after that is indistinguishable from a stall by the old rule, and is correct behaviour.
- The trace line read as `Sight(player) sight=False <- LOSS, not gain` was misread. In this
  schema `sight` means *the stimulus is the Sight sense*, and `success` means *successfully
  sensed*. That row is `sight=False, success=True`: a **different sense succeeding** — and it
  is the recovery edge, not a loss.
- The claim that "no attack goal is created" and that the loop has "no retry edge" was
  therefore false in both halves.

## What is actually true

| Measure | Baseline | With repair |
|---|---:|---:|
| Controllers that acquired an attack goal | **80 / 80** | **80 / 80** |
| Attack-goal acquisition, median | **1.4064 s** | **0.2285 s** |
| Acquisition range | 1.4012 – 1.4242 s | 0.2241 – 0.3226 s |
| Goal drops explained by player death | 72 / 72 (100%) | 56 / 57 (98%) |

The precondition is still deterministic: in 20/20 runs the player is first seen at t≈0.0005 s
with attitude **Neutral (1)**, because the player's factions do not exist yet, so the attack
predicate correctly rejects the target.

What recovers it in the baseline is **another sense** firing at t≈1.41 s, which produces a new
`OnPerceptionUpdated` callback and re-runs the predicate against now-valid factions. That
recovery is remarkably consistent (1.401–1.424 s across all 80 baseline controllers), which is
why the behaviour looked like a hang for a little over a second rather than a random stall.

## The causal chain

```
t=0.0005s  seq 38   Sight(player)      sight=True  success=True  attitude=1 Neutral, factions empty
t=0.186s   seq 71   player_faction_publication -> Narrative.Factions.Heroes
                    (baseline: reaches nothing that re-evaluates)
t=1.4108s  seq 91   NON-Sight stimulus sight=False success=True  attitude=2 Hostile
t=1.4108s  seq 92   attack goal created -> BT_Attack_DominionHound
```

1. **Sight is acquired ~185 ms before the player's factions exist.** `GetAttitude` resolves
   **Neutral** against a factionless player, so the target is correctly rejected.
2. **Faction publication reaches no re-evaluation path.** `OnRep_Faction` broadcast only
   `ANarrativeCharacter::OnFactionUpdated`, a bare member with no `UPROPERTY`. The generator
   binds the controller's perception delegate and the game state's `OnFactionAttitudeChanged`;
   a membership change raises neither.
3. **UE suppresses the same-state Sight notification**, so merely continuing to see an
   already-seen actor produces nothing.
4. **Recovery waits for an unrelated sense.** The decision is corrected only when some other
   stimulus happens to generate a fresh callback — here consistently ~1.18 s later.

So there *is* a retry edge; it is incidental and slow. Closing the intended edge removes the
wait.

## Why this was not visible before

**Every unattended packaged run was paused.** A fresh `-UserDir` leaves
`bAccessibilitySetupCompleted=False`, so the build correctly opens the first-boot
accessibility menu and pauses gameplay. World time never advanced (`world_time=0.0000` at
2.27 s real), controllers never ticked, perception never ran, and the trace fell silent after
world initialization.

This invalidates an earlier claim in `AwayEngineeringLog-2026-09-11.md` that the Hounds
"stay dormant until a human walks into the encounter". They were not dormant; **nothing was
ticking**. The harness now seeds `bAccessibilitySetupCompleted=True` into each run's
profile. That is harness-side only — shipping first-boot behaviour is unchanged, and
TDD §13.10 still requires that menu.

The `sov.DebugApproachHostile` placement probe turned out to be **unnecessary** for
reproduction: the Hounds already see Selene at spawn. It is retained because it is useful for
driving other encounters, but the stall needs no player movement at all.

## Option C survey — framework-native mechanisms

Surveyed the local fork for an existing invalidation/reconsideration entry point. Five
relevant mechanisms exist. **None closes this gap unmodified**, but one is the obvious
foundation for the repair.

| # | Mechanism | Exposure | Verdict |
|---|---|---|---|
| 1 | `UNPCActivityComponent::RescoreGoals()` → `PerformActivitySelection(true)` | `UFUNCTION()`, on a **repeating timer** (`RescoreInterval`) | **Wrong layer.** Scores activities against goals that already exist (`GetGoals(...)`). With zero attack goals there is nothing to select, which is exactly why the stall survives a periodic rescore. |
| 2 | `ANarrativeGameState::OnFactionAttitudeChanged` | `BlueprintAssignable` | **Right semantics, never raised.** Its own comment: *"bots bind this to recheck if they are perceiving someone who has become a hostile."* Broadcast **only** from `SetFactionAttitude`, i.e. a change to the faction-pair attitude table. Faction *membership* changes never reach it. |
| 3 | `ANarrativeCharacter::OnFactionUpdated` | **Not** `UPROPERTY`/BlueprintAssignable | **Exists but unreachable** from the stock Blueprint generator. Adjacent framework comment: *"Factions are getting a little messy - possibly fold this into a FactionComponent?"* |
| 4 | `ANarrativeNPCController::RefreshThreatMemory()` | `BlueprintCallable`, called at `BeginPlay` and periodically | **Refreshes threat, not goals.** Notable as precedent: the framework already re-examines currently perceived actors on a timer *without manufacturing perception events*. Consistent with the 6 Sept note that threat memory allowed direct targeting while the NPC still stalled. |
| 5 | `UNPCGoalGenerator::AddGoalItem(Goal, bTriggerReselect)` / `InitializeGoalGenerator()` | `BlueprintCallable` / `BlueprintNativeEvent` | **The correct write path.** `InitializeGoalGenerator` is where the once-only "refresh currently perceived Sight actors" pass lives; `AddGoalItem` is how a goal is added, with reselect built in. |

### Where the gap actually is

The generator is **already subscribed to the right kind of signal** (#2). The defect is not a
missing subscription — it is that **a faction *membership* change does not raise the faction
*attitude* signal.** Both change the effective attitude toward an actor; only one is
announced.

### Recommendation: C1, raise the existing signal on membership change

Have player faction publication (`ANarrativePlayerState::SetFactions` / `OnRep_Faction`)
raise `OnFactionAttitudeChanged` for the affected faction pairs.

Why this over Options A and B:

- **No Blueprint edits.** The stock `GoalGenerator_Attack` already binds this delegate, so
  the repair is native-only and every existing subscriber benefits.
- **Framework-consistent.** It uses the mechanism the framework documents for precisely this
  case rather than inventing a parallel path.
- **Bounded fan-out.** One publication at character initialization, not an ongoing signal.
- **Touches no perception.** No refresh, no UE semantics change.

### Risks to resolve before implementing

| Risk | Assessment needed |
|---|---|
| Semantic honesty | The delegate's parameters describe a faction-pair attitude change. Raising it for a membership change is arguably a misstatement unless the broadcast reports the genuinely current pair attitude. |
| Fan-out | Broadcasts to every bound bot. One-shot at init is acceptable; a per-pair loop over all factions would not be. |
| Duplicate goals | The 2/20 runs that already recover must not gain a second goal. `AddGoalItem` must remain idempotent for an existing target. |
| Repeated restart | Re-selection must not restart a running attack tree. |
| Checkpoint/restore | Restoring a save republishes factions; that must not re-trigger goal churn on NPCs already mid-activity. |
| Late spawn | NPCs spawned after publication must still work — they run `InitializeGoalGenerator` against already-valid factions, so they should be unaffected, but this needs confirming. |
| Multiplayer | `OnRep_Faction` is a replication callback. On a listen server the broadcast would fire on both server and client paths; goal generation is server-authoritative, so the client path must be a no-op rather than a second generation. |

### Alternatives if C1 is rejected

**Option A (subscribe generator to faction publication)** is effectively mechanism #3 plus
Blueprint work: expose `OnFactionUpdated` as `BlueprintAssignable` and bind it in the stock
generator. More precise per-actor, but requires editing fork *content*, couples the generator
to player-state internals, and sits on the area the framework author flagged as unstable.

**Option B (targeted native reevaluation)** adds a new native call that re-runs generator
predicates against already-perceived actors. Most surgical, but introduces a parallel
reconsideration path alongside the existing one, which is the outcome to avoid.

---

## Repair options — none implemented

Per instruction, no fork modification. The requirement is that **hostile goal evaluation
becomes correct once the player's faction information is authoritative**, not that another
perception event is forced.

| Option | Shape | Notes |
|---|---|---|
| A. Subscribe the goal generator to player faction publication | Generator (or its owning runtime layer) listens to the player's `OnFactionUpdated` in addition to GameState `OnFactionAttitudeChanged` | Narrowest edge that matches the actual missing signal. Needs care that one publication does not fan out to duplicate goals across generators. |
| B. Targeted reevaluation when faction state becomes valid | On faction publication, ask affected generators to re-run their predicate against currently perceived actors | Uses existing "currently perceived" data rather than new perception events. Must be idempotent. |
| C. Existing Narrative invalidation/reconsideration mechanism | If the framework already exposes a goal invalidation or activity reconsideration entry point, route publication into it | Preferred if it exists — no new subscription surface. **Not yet surveyed.** |

**Explicitly rejected:** forcing a perception refresh, clearing perception state, or altering
UE perception semantics. Those change global behaviour to fix a local initialization-order
problem, and would mask rather than repair the missing edge.

**Recommended next step before any edit:** survey option C. If the fork already has a
reconsideration hook, it is almost certainly the smallest framework-consistent repair.

## The implemented repair — targeted reevaluation

C1 was rejected: raising `OnFactionAttitudeChanged` for a membership change would leave that
delegate's name and parameters untrustworthy for every existing subscriber. What follows
instead reuses the reconsideration path the framework already has.

### What the Blueprint already contains

`GoalGenerator_Attack` was inspected by exporting it to T3D and reading the node graph
(Python cannot enumerate Blueprint-defined functions, and the Kismet text backend produced
nothing). It already implements the whole flow:

```
InitializeGoalGenerator (override)
  ├─ bind BP_NarrativeNPCController.OnPerceptionUpdated → OnPerceptionUpdated_Event
  ├─ bind NarrativeGameState.OnFactionAttitudeChanged   → OnFactionsUpdated → RefreshPerceivedActors()
  └─ call RefreshPerceivedActors()        ← once-only initial pass, ~55 ms too early

RefreshPerceivedActors()   [Public | BlueprintCallable, 0 parameters]
  └─ OwnerController → GetAIPerceptionComponent → GetCurrentlyPerceivedActors
       ForEach → Try Add Attack Goal From Actor

Try Add Attack Goal From Actor   ← the sole hostility predicate
  └─ GetAttitude → Map_Find(AttackAffiliationMap) → DoesAttitudeMatchFilter → AddGoalItem
```

Two of the author's own comments confirm the intent. On `OnFactionsUpdated`: *"when factions
update a previously sensed friendly/neutral may now be an enemy - check for this"*. On
`RefreshPerceivedActors`: *"TODO figure out how to actually refresh UE5's perception. This
works okay in the meantime however, where we just make a fake AIStimulus and send that
through."* — it synthesises a local stimulus struct rather than pushing into the perception
system, which is exactly the constraint the repair had to respect.

`OnFactionsUpdated` discards all three delegate parameters and calls `RefreshPerceivedActors`.
So the generator never needed the attitude payload; it only needed to be told to look again.

### The missing edge

`RefreshPerceivedActors` runs at initialization and on a faction **attitude** change. A faction
**membership** change raises neither. `ANarrativePlayerState::OnRep_Faction` — the single
funnel for `SetFactions`, `AddFaction`, `RemoveFaction` and replication — broadcast only
`ANarrativeCharacter::OnFactionUpdated`, a bare member with no `UPROPERTY`, so nothing outside
the character could bind it.

### Changes

| # | File | Change |
|---|---|---|
| 1 | `NarrativeGameState.h/.cpp` | New `FOnFactionMembershipChanged(AActor*, FGameplayTagContainer)` + `NotifyFactionMembershipChanged`. Separate name, parameters and meaning from `OnFactionAttitudeChanged`; no existing subscriber's contract changes. |
| 2 | `NarrativePlayerState.cpp` | `OnRep_Faction` republishes the same fact through the game state. |
| 3 | `NPCGoalGenerator.h/.cpp` | New `ReevaluatePerceivedActors()` `BlueprintNativeEvent`. Default implementation forwards to an authored zero-parameter `RefreshPerceivedActors`, refusing any other signature. |
| 4 | `NPCActivityComponent.h/.cpp` | Subscribes in `BeginPlay`, unsubscribes in `EndPlay`; gates on authority, on a restore not being in flight, and on actually perceiving the changed actor; then asks each generator. |

**No fork content was modified.** Headless Blueprint graph authoring is not available —
`BlueprintEditorLibrary.add_function_graph` creates only an empty graph and there is no
node or pin API — so a one-node override could not be authored without a GUI session. The
native default therefore surfaces the existing Blueprint-callable entry point by its
established name, which is preferable to restating the predicate in native code. When the
editor is next open this can be replaced by a proper override node with no behaviour change.

### How each constraint is met

- **Not a rebroadcast under false semantics** — a new delegate that says what happened.
- **No duplicated predicate** — the hostility test stays only in `Try Add Attack Goal From Actor`.
- **No forced perception update** — nothing calls into the perception system; the existing pass
  reads `GetCurrentlyPerceivedActors` and synthesises its own stimulus, as it already did.
- **No global rescoring** — an NPC that does not currently perceive the changed actor is
  skipped outright. It holds no stale decision, and its next perception event evaluates against
  valid factions.
- **Server-authoritative** — `UNPCActivityComponent::ReevaluatePerceivedActors` returns without
  authority. The delegate itself fires on both sides because the membership change is true on
  both; the consumer gates.
- **Idempotent** — `UNPCActivityComponent::AddGoal` already rejects a second goal for a
  registered key via `GoalUniqueObjectMap`, so a repeated pass cannot duplicate a goal, and the
  existing recovery runs cannot gain a second one.
- **No behaviour-tree restart** — nothing calls `PerformActivitySelection` directly; goals are
  offered through the existing `AddGoalItem(..., bTriggerReselect)` path.
- **Checkpoint/restore** — a reevaluation is refused while a saved-activity restore is pending.

## Regression coverage a fix must carry

1. A hostile that first perceives the player before faction publication **eventually acquires
   its attack goal** once factions become authoritative.
2. Normal post-initialization perception is unchanged — a hostile that first sees the player
   after factions are valid behaves exactly as today.
3. Reevaluation **cannot duplicate goals** or repeatedly restart the behaviour tree. Every
   controller must still end with exactly one attack goal for the target, not two.

### Coverage as implemented

`Source/ProjectVelkorranTests/Private/Tests/SovFactionMembershipReevaluationTests.cpp`, five
suites under `ProjectVelkorran.Campaign.AI`. The first drives the **real** publication path —
`ANarrativePlayerState::SetFactions` on a player state bound to a narrative player character —
rather than calling the new notifier directly, so it exercises the edge that was missing.

| Suite | Asserts |
|---|---|
| `FactionMembershipChangeReachesPerceivingNPC` | Publication after perception reaches the NPC; a repeat is delivered once more, not amplified |
| `FactionMembershipChangeSkipsUnperceivingNPC` | An NPC not perceiving the actor is untouched |
| `ReevaluationRequiresAuthority` | Refused as a simulated proxy; honoured once authority is restored |
| `ReevaluationUsesAuthoredRefreshEntryPoint` | The default hook invokes a zero-parameter `RefreshPerceivedActors`, and refuses a same-named function with a different signature |
| `ReevaluationCannotDuplicateGoalsForSameTarget` | A second distinct goal for the same target is rejected; exactly one remains |

**Negative control performed.** With change 2 removed and change 3's body emptied, rebuilt and
rerun: `FactionMembershipChangeReachesPerceivingNPC` fails (`0`, expected `1` and `2`) and
`ReevaluationUsesAuthoredRefreshEntryPoint` fails (`0`, expected `1`); the three invariant
suites correctly stay green. The implementation was then restored. An earlier control attempt
did not compile under warnings-as-errors, so its apparently passing run reused a stale binary
and was discarded rather than reported.

Full suite after the repair: **612 passing, 0 failing** (607 before, +5 new).

### Packaged result

80 controllers per configuration, same archive settings, classified by
`Scripts/Classify-AIStartupRuns.py`:

```
median acquisition: baseline 1.4064s -> with repair 0.2285s  (-1.1779s)
never acquired a goal: 0 / 80 in both configurations
goal drops with player already dead: baseline 72/72, repaired 56/57
```

Acquisition now lands ~40 ms after faction publication (0.186 s -> 0.229 s) instead of waiting
~1.22 s for an unrelated sense. The improvement is deterministic: every one of the 80 repaired
controllers acquired between 0.224 s and 0.323 s, with no overlap against the baseline's
1.401–1.424 s band.

**What this repair does not do.** It does not fix a stall, because there was no stall. It
removes a consistent ~1.18 s delay before hostiles engage. That is worth having for encounter
feel, and the ordering defect it closes is real, but it should not be described as fixing a
hang. The 6 September observation that originally motivated this work is **still unexplained**
— nothing in these 40 packaged runs reproduces an NPC that never engages.
