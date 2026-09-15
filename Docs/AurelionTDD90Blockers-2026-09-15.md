# Aurelion 90% alignment — blocker ranking and execution log, 15 September 2026

The 90% target is the M12–M13 slice rubric (26 requirements, 100 points) summarised in
[AurelionTDDAlignment-2026-09-13.md](AurelionTDDAlignment-2026-09-13.md). The supported score entering
this pass was **63.75**. Reaching 90 needs 26.25 more points. Scores below change only on measured
evidence, never on implementation alone.

## What gates the missing points

| Gate | Rows (missing points) | Can this workstation close it? |
|---|---|---|
| Fresh unattended M12→M13 route | M1 (2), M2 (1.5), M3 (2), R1 (1), R2 (2.25), P1 (0.5), A4 (1.5), C2 (1.5) | **Yes**, once the route runs end to end |
| Measured performance on an approved target | P2 (5) | **Partly.** Workstation captures are possible; an approved target hardware/profile is a user decision |
| Recruited-player studies | C1, C4, A1, A2, D2 recognition/comprehension thresholds, M3 noticed consequence | **No.** Needs human participants |
| Canon, scene and cost review | M1, D3 | **No.** Needs a reviewer; D3 also needs authored voice and scenes |
| Content presentation (dense-combat readability, minimap, final scenes) | V2, U3, U4, D3 | Partly, with authoring and rendered review |

Because several rows can only reach full credit through studies or review, automation alone is
unlikely to exceed the low-to-mid 80s under this rubric. This is recorded so the score is not inflated
by treating implementation as acceptance.

## The route blocker, measured

Across 27 retained fresh route runs before this pass, E1 passed about half the time. 17 of 22 E1
failures were player deaths, 2 were ammunition exhaustion and 2 were deadlines. After E1, runs stopped
at E4A (manual Axiom main-hand click, five times), E4B (three times) and E3 (twice). No run reached
M13 unattended.

## Changes in this pass

1. **E4A no longer needs an operator.** `USovAurelionPIEInputLibrary::InjectAurelionPIELeftClick`
   routes an ordinary left press/release through Slate at the PIE viewport centre, the same application
   path a desktop click takes. The wheel selector clicks, re-observes and retries within a bound because
   the first click can only focus the viewport. In `ClickPressureRoute-20260915-130812-498b8cf9` the
   first click focused the viewport and the retry selected the Axiom main hand; E4A passed in 12.6 s.
   This is synthetic application input, not physical mouse validation.
2. **E1 pressure is now observed.** The E1 driver records every incoming native damage receipt with its
   source and the coordination relief state, plus shield and relief samples. In the same run E1 passed
   in 115.6 s with seven damage receipts and no relief activation. The earlier recorded death took the
   player from 89 to 7.5 health in about three seconds with three drones alive, before the 25% health
   relief trigger could help; further death runs are needed before changing the TDD 8.7 relief rule.
3. **E4B: the Elite Core broke before Thermal Fracture.** The route reached E4B unattended and failed
   because the Core weak point was already broken. The layout contract makes the Core the exposed joint
   of E4's single Thermal Fracture payoff, but the native zone accepted any sufficient hit. Weak points
   now have an owner rule, `CanBreakWeakPoint`; the Aurelion Core allows a break only after the current
   attempt's completed fracture. A refused hit stays a hit with no break, reward or consequence.
   The editor build and all 667 `ProjectVelkorran` automation tests passed in `20260915-132436-58be319b`
   (576 clean, 91 with existing warnings, none failed), including the new owner-gate and Elite checks.
4. **P2 memory capture.** The performance harness now records one used-memory reading per mission
   world after warm-up, current and peak use, and judges the TDD 18.8 rule "no growing memory trend
   across three consecutive mission reloads" through a portable `EvaluateLoadMemory` policy. Six
   `ProjectVelkorran.Diagnostics.Performance` tests passed in `20260915-131937-281813d7`.
5. **R2 checkpoint soak harness.** `soak_aurelion_checkpoint_reload.py` repeats fresh M12 starts and
   public CP0 reloads in one editor process. Its first five-cycle run
   (`CheckpointSoak-20260915-132205-0957e717`) had five successful native load callbacks, but the harness
   rejected three because a newer same-boundary CP0 landed between the header read and the load. That
   is a harness race, not a failed reload. The harness now accepts a newer same-boundary CP0 and records
   that it advanced; an older or different checkpoint still fails. No reliability figure is claimed yet.
6. **M3 both outcomes.** The E4-entry driver now plays either legal priority through
   `SOV_AURELION_PRIORITY` (WestStretchers default, or EastWalkers), checking the matching native
   consequence, outcome tag, aftermath identity and which barrier opens. Neither outcome has been
   replayed end to end yet.

## First unattended fresh M12→M13 route

`CoreGateRoute-20260915-132715-08bff503` passed the entire chain through ordinary input with no
operator step: entry (39 s), E1 (121 s), E2 (70 s), E3 entry (94 s), E3 rescue (80 s), E4 entry with
the WestStretchers priority (98 s), E4A with the automated Axiom click (13 s), E4B (21 s) with one
native Thermal Fracture receipt followed by the ordinary Core follow-up, M13 entry (77 s) and M13
(287 s). M13 recorded all 35 journal receipts, M12 and M13 completion, the paired physical lift and all
ten scenes completed without skipping. Every stage reported unchanged asset hashes. This is the first
retained fresh route to reach separate departures without an operator. It is one pass, not
reliability; the CP9 reload, the EastWalkers outcome, physical input, rendered review and packaged
execution remain separate evidence.

## Score

Unchanged at **63.75** until the route, soak and captures above produce passing evidence.
