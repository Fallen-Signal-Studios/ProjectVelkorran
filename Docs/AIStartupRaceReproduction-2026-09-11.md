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
