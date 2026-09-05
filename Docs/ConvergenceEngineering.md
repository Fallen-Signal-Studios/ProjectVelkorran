# Convergence, Resonance and companion engineering

Implements the native portions of the current v2 TDD sections 5.4, 7.6 and 12. This pass does not create Blueprint, animation, StateTree, mesh, mission-map or audio assets. Narrative's existing ASC, activity slot, damage execution, actor records and campaign journal remain the owners of those systems.

## Native behavior

| Requirement | Implementation and ownership |
|---|---|
| Authored convergence only | `USovCampaignDefinition` permits joint interactions in M12/M13, names both protagonist companion profiles, allowed companion IDs, enabled interaction types and prerequisite beats. `USovCampaignStateComponent` records the actual controlled protagonist. |
| Separate ASCs | `ASovProtagonistCompanionCharacter` has its own Narrative ASC, defense/resource components and native activity identity. `USovConvergenceCompanionState`, on the player controller, owns its staging, save record, replacement and destruction. |
| Actual unlocked AI kit | `PrepareProxy` reads the outgoing protagonist's granted classes and actual levels, intersects them with the explicit curated list and leaves the player's grants, progression and Echo untouched. First convergence entry reads the inactive protagonist's previously captured kit evidence. |
| Authored switching | Existing controller handoff stages the outgoing character as an independent companion and restores the incoming player's durable kit with the actual incoming companion's current combat resources. There is no unrestricted switching API. |
| Transactional companion replacement | Source companion survives until the destination pawn and staged companion initialize. Failure restores the incoming protagonist's pre-stage ledger record and destroys only the staged replacement. Recovery stages the origin companion record before the controller's existing asynchronous readiness poll. |
| Save ownership | The controller component stores one native companion snapshot. The proxy opts out of separate world records, preventing duplicate ownership. Both restore in Narrative's `Companions` phase. An active convergence mission with no savable companion fails capture. Missing or mismatched convergence records fail loading. |
| Command surface | `USovCompanionComponent` validates focus, hold, defend, move, interaction, co-action and regroup requests against mission permission, life/state and current Narrative activity. Move/interact use the existing `ASovCoActionAnchor` path. |
| Native follow/combat decisions | `USovCompanionCommandActivity` runs in Narrative's single existing activity slot. Protagonist proxies use the distinct `USovProtagonistCompanionActivity`. Curated AI selects real existing bot ability candidates and uses copied guard/deflection abilities on bounded cooldowns. |
| Contribution budget | Ordinary autonomous damage is capped at an authored fraction of combined player+companion damage, clamped to 15–25%. The source-side damage policy also caps delayed impacts. No damage is available before the player contributes. Player-confirmed Resonance tickets allow their own explicit payoff. |
| Required targets | Autonomous selection withholds boss/required interaction windows. Source-side damage policy prevents required-target damage and leaves bosses at least 1 Health. |
| Nonblocking movement | Leader and companion capsules receive only the owned movement-ignore entries; leader replacement and end play remove those entries without clearing unrelated ignores. |
| Separation recovery | At 25 m separation, the companion may correct to its explicit recovery anchor only outside an authored split phase, within 20 m of the leader, with a full nav path, clear capsule, and occlusion of both departure and arrival capsule samples from every player. Failed normal paths retain command intent and retry at a bounded cadence. |
| Real defeat | Native required/protagonist companion death asks the existing fatal-recovery owner to retry the active segment before failing the encounter. Ordinary allies stop acting when disabled; an explicitly bound `RecoveryEncounter` can revive them at 50% Health after success, with one second of source-owned protection. |
| Rescue query | Existing recovery asks `CanProvideRescue`: allowed difficulty, mission, living/unreserved companion, range and complete path are all required. Rescue expenditure remains owned by `USovFatalRecoveryComponent`. |

## Four Resonance interactions

`USovResonanceComponent` observes actual combat/exposure/protection results. A short-lived offer contains a unique interaction ID, partner, target, expiry and type. Confirmation grants one private `USovResonanceAbility` to each ASC, with source tickets bound to that exact ASC and interaction. Either interruption invalidates both tickets before abilities are canceled. Ordinary external Busy or available-tag owners survive cleanup.

| Setup | Native payoff | Required target configuration |
|---|---|---|
| Tarrik perfect-guards a marked heavy | Selene severs the real `USovCommandLinkComponent`; only a successful new sever extends Tarrik's still-open counter window. Optional associated weak point is broken without a duplicate reward. | `SupportSever`, support-link owner and optional weak-point ID. |
| Selene exposes a command node | Tarrik makes a bounded swept breach toward it. Only friendly movement ignores are added and later released. World/enemy obstruction cancels the action. | `FormationBreach`, existing weak-point component, permitted breach distance/speed. |
| Tarrik protects Selene during a held terminal action | A validated protection-intercept receipt adds capped pressure. Completing the hold offers a release whose two half-payloads retain their distinct instigator ASCs. | `TerminalRelease`, physical terminal range/hold, pressure cap and release radius. |
| Selene routes hostile fire into a shielded lane | A matching near-term source subsequently guarded by Tarrik offers a bounded resistance corridor. Both protagonists must be inside the explicit authored lane. | `AdvanceCorridor`, lane anchor, length, width and duration. |

Targets, heroes, mission identity, separation, visibility and paired activation validity are checked again while an offer/action is active. An offer cannot survive a changed ASC or mission. The action has a two-second completion/recovery deadline. Successful actions have an eight-second coordinator cooldown. These are prototype native tuning defaults, not playtest-certified balance.

The terminal API intentionally exposes begin/end for the authored interaction binding. It does not award protection from an arbitrary Blueprint float: native committed protection receipts are the only pressure producer. Completion is saved; offers, held operation, exposure and pressure are transient. A missed/canceled release does not replay a completed terminal for a free second payoff.

## Remaining content authoring

1. Author both companion profiles on M12/M13 with unique companion IDs, concrete proxy subclasses/NPC definitions, unique entry/recovery-anchor tags and curated ability lists. Both player kits must have been learned and captured before convergence entry.
2. Place exactly one anchor for each profile tag. Runtime staging rejects missing/duplicate anchors. Move the current companion's `RecoveryAnchor` at authored scene boundaries when the original entrance is no longer a valid reunion point. Set `bInAuthoredSplitPhase` for intentional separation.
3. Set actual weapon/equipment, factions, visual definitions and animation behavior on the NPC definitions. The native curated ability copy is not a duplicate player inventory. The NPC's authored equipment must support each copied weapon-specific ability.
4. Bind the contextual UI/Enhanced Input to existing command APIs and exact offer-ID confirmation. Bind terminal interaction begin/end to its authored hold/presentation. Supply the attack, defense, co-action and recovery animations and feedback.
5. Keep protagonist/required companion actors alive as objects until the native retry owner handles defeat. Ordinary recoverable companion death presentation must retain the actor for `RecoveryEncounter` success. Asset logic that destroys the actor cannot later be revived by its component.
6. Compose mission-specific lead/scripted/split behavior and cinematics over the native activity slot. No second StateTree runner is introduced. The TDD's bespoke StateTree asset and scene temperament remain content work; the native protagonist activity supplies the engineering contract.

## Validation performed

- `python3 Scripts/Test-NativePolicies.py`: 19 portable suites passed at this checkpoint, built with C++17, warnings as errors and undefined-behavior sanitizer.
- `Tests/Portable/SovResonancePolicyTests.cpp`: 23,032 counted cases across complementary identities, offer capability combinations, 15–25% accumulated damage budgets and bounded pressure, plus invalid-time and separation boundaries.
- `git diff --check`: passed.
- Added `SovResonanceRuntimeTests.cpp` with real ASC tests for paired grants/tickets, cancellation ownership, stale-ticket rejection, copied unlocked levels, no player Echo spending, and staged-only rollback.
- Unreal 5.7/UHT/editor automation could not be run in this workspace. The runtime tests are authored, not reported as executed.

## Engine validation gate

Run `ProjectVelkorran.Campaign.Resonance.*` and `ProjectVelkorran.Campaign.Companion.*`, followed by campaign handoff, encounter, save and recovery suites in an Unreal 5.7 build. In authored M12/M13 fixtures, verify all four actual setup-to-payoff loops in both controlled-lead configurations; interrupt at each offer/commit/restore boundary; and save/load before and after each handoff. Confirm only one uncontrolled protagonist exists after success, timeout, failed spawn, failed visual initialization, full load and encounter retry.

Measure the 15–25% contribution band in representative encounters, command start latency, capsule contact frequency, 25 m recovery with blocked and visible anchors, and required-target survival. The code establishes deterministic admission and ownership; the TDD's 99%/99.5% experiential thresholds still require engine playtests and authored assets.
