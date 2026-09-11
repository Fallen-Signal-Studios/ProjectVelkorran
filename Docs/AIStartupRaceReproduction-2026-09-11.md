# Aurelion AI startup stall — reproduced, 11 September 2026

The intermittent hostile-startup stall first recorded in
`WorkPCAIStartupObservation-2026-09-06.md` is **reproduced deterministically under
automation**, with a measured rate. No Narrative Pro code was modified to obtain this.

Reproduce with:

```powershell
.\Scripts\Run-AIStartupColdStarts.ps1 `
  -ArchiveDirectory '<archive>\Windows' -Runs 20 -HoldSeconds 25 -ApproachHostile
```

Map `/Game/Maps/Development/L_SeleneCombat` (3 Dominion Hounds + Handler), packaged
Development, `sov.AIStartupTrace` armed through `-dpcvars`.

## Distribution over 20 cold starts

| Measure | Result |
|---|---:|
| Player first seen **before** faction publication | **20 / 20** |
| Attitude at that first Sight | **Neutral (1) in 20 / 20** |
| Ended with **zero** goals, still seeing the player, attitude Hostile, in `BT_ReturnToSpawn` | **18 / 20** |
| Recovered and acquired an attack goal (`BT_Attack_DominionHound`) | 2 / 20 (runs 8, 17) |
| Ended not currently seeing the player (legitimate loss) | 1 / 20 (run 13) |

The **precondition is deterministic**; the **failure is 90%**. The two recoveries show a
working path exists — most likely a genuine sight-lost/sight-regained cycle producing a new
event after factions became valid — which is why the defect reads as intermittent in play.

## The causal chain

```
t=0.178s  seq 14,22,30,38   Sight(player)  sight=True   attitude=1 (Neutral)  factions empty
t=0.381s  seq 71            player_faction_publication  -> Narrative.Factions.Heroes
t=1.580s  seq 85+           Sight(player)  sight=False  attitude=2 (Hostile)  <- LOSS, not gain
t=18.4s   final snapshot    tree=BT_ReturnToSpawn  goal_count=0
                            players.currently_seen=True
                            players.attitude=2 (Hostile)
                            players.factions=Narrative.Factions.Heroes
```

1. **Sight is acquired ~200 ms before the player's factions exist.** `GoalGenerator_Attack`'s
   attack predicate uses `GetAttitude`, which resolves **Neutral** against a factionless
   player, so the target is rejected and no attack goal is created.
2. **Faction publication does not reach the generator.**
   `ANarrativePlayerState::OnRep_Faction` broadcasts the *player's* `OnFactionUpdated`. The
   generator listens to the controller's perception delegate and GameState's
   `OnFactionAttitudeChanged` — neither fires, so nothing triggers reevaluation.
3. **No further Sight event arrives.** UE 5.7 `UAIPerceptionComponent::ProcessStimuli`
   suppresses same-state notifications, so continuing to see an already-seen actor generates
   nothing. Subsequent callbacks in the trace are `sight=False` losses.
4. **Terminal state is self-consistent and wrong**: attitude correct, factions valid, player
   actively perceived, zero goals, permanently in the fallback tree.

The loop has **no retry edge**. Every input that could correct the decision has either
already fired (factions) or is suppressed (Sight).

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

## Regression coverage a fix must carry

1. A hostile that first perceives the player before faction publication **eventually acquires
   its attack goal** once factions become authoritative.
2. Normal post-initialization perception is unchanged — a hostile that first sees the player
   after factions are valid behaves exactly as today.
3. Reevaluation **cannot duplicate goals** or repeatedly restart the behaviour tree. The
   18/20 stalled runs must become attack-goal runs without the 2/20 recovering runs gaining a
   second goal.
