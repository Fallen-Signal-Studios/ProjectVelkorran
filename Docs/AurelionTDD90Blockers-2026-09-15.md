# Aurelion 90% alignment — blocker ranking and execution log, 15 September 2026

The 90% target is the M12–M13 slice rubric (26 requirements, 100 points) summarised in
[AurelionTDDAlignment-2026-09-13.md](AurelionTDDAlignment-2026-09-13.md). The supported score entering
this pass was **63.75**. Reaching 90 needs 26.25 more points. Scores below change only on measured
evidence, never on implementation alone.

## What gates the missing points

| Gate | Rows (missing points) | Can this workstation close it? |
|---|---|---|
| Fresh unattended M12→M13 route | M1 (2), M2 (1.5), M3 (2), R1 (1), R2 (2.25), P1 (0.5), A4 (1.5), C2 (1.5) | **Yes**, once the route runs end to end |
| Measured performance on an approved target | P2 (5) | **Yes, for PC.** On 15 September the creator approved this Windows workstation (AMD Ryzen 9 7900X) as a PC target profile. Packaged captures here are admissible; editor/PIE captures and console claims are not |
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

## Second outcome, CP9 chaining and a measured relief gap

`EastCP9Route-20260915-134512-73ea7536` committed the EastWalkers priority through ordinary input: the east
flank opened, the west cache stayed blocking and the aftermath identity became `M12_EastWalkersPrioritized`.
It then failed at E4B when the three-second native frost window expired during the heat approach. The
layout contract makes Thermal Fracture recoverable, so the E4B driver now retries a missed or rejected
window through a fresh ordinary frost setup, bounded to three misses.

`EastRetryRoute-20260915-135911-1147bc03` died in E1 before reaching the retry. Its damage-share observer
recorded 1,244 damage to E1 hostiles, all from the player (E1 has no companion). Its pressure record
explains the death. At 74.3 s the shield was gone and health was 50.3 with three drones alive; relief did
not open because health was above 25%. A drone burst at 79.0 s left 22.9 health, relief opened at 79.5 s,
and the next attack the relief interval admitted, at 81.7 s, dealt exactly 22.9. A standard burst against
a broken shield removes up to about 36 health, so relief that waits for 25% health can never be the
recoverable low-pressure state TDD 8.7 requires.

Relief thresholds are now authored encounter data on the coordination component. Critical health keeps
its 0.25 default, and a new shield-depleted trigger opens relief at or below half health
(`ReliefShieldDepletedHealthFraction` 0.5). Relief still grants no resources, and the 1.5 s relief attack
spacing is unchanged until the earlier trigger has been measured in play.

## Checkpoint soak with the corrected harness

`CheckpointSoak30-20260915-140340-a3940143` completed 30 of 30 cycles in one editor process: 30 fresh M12 starts reached a ready mission with a
new CP0 checkpoint, and 30 of 30 public `load_slot(CHECKPOINT, 0)` requests completed with one successful
native callback, a new world, an unchanged journal and a ready protagonist for three seconds. The median
reload took 4.03 s (maximum 4.38 s); warm starts were ready in about 0.9 s. In 28 cycles a newer
same-boundary CP0 landed between the header read and the load and was accepted. R2 asks for more than
99.5% reload success and 100 consecutive starts in both frame profiles, so this is supporting evidence,
not R2 qualification; the 100-cycle 60 fps and 30 fps runs are next.

## Observer crash and the approved PC target

`EastReliefRoute-20260915-141228-d3c167f8` passed entry, E1 (149 s, relief never needed) and E2 (70 s), then
the editor terminated 19 seconds into E3 entry with `EXCEPTION_ACCESS_VIOLATION` in CoreUObject called from
the Python plugin during garbage collection. It was the first run past E2 with the new damage-share
observer, which retained Python delegate wrappers on enemy ability systems that E2's death cleanup then
destroyed. The repository's Elite Core observer met the same wrapper-lifetime class earlier and moved to
retained paths. The observer now keeps only actor paths and callables, rescans four times a second,
unbinds any participant that stops being alive while it still exists, and forgets bindings on a world
change. This is the evidenced cause, not yet a confirmed one; the next route run confirms or refutes it.

On 15 September the creator approved this Windows workstation (AMD Ryzen 9 7900X) as a PC target
profile, so packaged captures here are admissible for P2. The capture harness now supports an unattended
packaged capture through opt-in cvars, all off by default: `sov.PerfCapture.ExportOnEnd` writes a
uniquely named report when each captured world ends, `ReloadCount` and `ReloadAfterSeconds` reopen the
packaged map on a schedule so load memory is judged across three reloads, and `QuitAfterReloads` exits
afterwards. Reload and quit act only in packaged game worlds, never in the editor or PIE.
`Scripts/Capture-AurelionPerformance.ps1` launches the packaged build rendered with that schedule and
collects the reports, and `Play-Aurelion.ps1 -PerfCapture` arms capture for a played session. The
unattended capture covers startup, streaming, steady play and reload memory; it cannot drive combat, so
the worst-case combat percentile needs a played capture or a packaged autoplay path.

## Crash fix confirmed; E4B driver defects

`EastObserverRoute-20260915-145036-f81b59b4` passed entry through E4A with no editor crash, so the observer
lifetime change removed the E2-cleanup crash. It also exposed that dropping every delegate wrapper removes
the Python binding itself: only six damage receipts were recorded. The observer now keeps each wrapper
while its participant lives and unbinds it as soon as the participant stops being alive.

E4B died in 30 s. The native frost rule first rejected the setup because Selene was 129 cm from the
clean mark but did not satisfy its full condition (on the mark, with sight of the elite, Tarrik in support
range); the driver's partner check used distance alone. On the retry a WallRunner's interactable held
Tarrik's focus in front of the frost control ("The interaction is obstructed") and the driver aimed in
place while the Weaver, elite and WallRunner took him from 80 to 2 health. Both are driver defects: a
partner-position rejection now re-issues the ordinary partner move, and focus held by another interactable
for three seconds moves Tarrik to an alternate standing point, bounded to four repositions.

## Entry timeout during the played capture

`EastFocusRoute-20260915-151536-0f792fd1` failed at entry: "Campaign initialization failed: Campaign
character readiness timed out" 40 seconds after PIE began. A packaged `Play-Aurelion.ps1 -PerfCapture`
session (the creator's played combat capture) had started two minutes after the editor, rendering on the
same GPU and CPU. The overlap starved PIE loading; this is recorded as resource contention, not a
regression, and no editor workload runs during a played capture so neither measurement is skewed.

## Packaged performance capture on the approved PC target

`PackagedCapture-20260915-150900-faab9793` ran the cooked Win64 Development package
(`AurelionPerfCapture-20260915-150424`, BuildCookRun successful) rendered at 1920×1080 on D3D12 SM6, NVIDIA
GeForce RTX 5070 (12 GB, driver 591.86) and AMD Ryzen 9 7900X. The opt-in schedule captured M12 for
60 seconds of steady play, reloaded it three times and exited normally, writing one report per world:

| World | Samples | p50 | p95 | p99 | Max | Frames over 16.67 ms | Longest streak | Harness verdict |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| 1 | 4096 | 12.67 | 14.99 | 16.86 | 50.80 | 42 | 3 | Pass |
| 2 | 4096 | 12.43 | 14.49 | 16.01 | 63.02 | 26 | 1 | Pass |
| 3 | 4096 | 12.52 | 14.50 | 17.12 | 58.98 | 45 | 2 | Pass |
| 4 | 4096 | 12.15 | 14.05 | 16.48 | 108.38 | 37 | 2 | Fail (one frame at or above the 100 ms hard stall) |

Load memory across the four worlds was 2,823, 2,358, 2,818 and 2,683 MB, which the portable rule judges
Stable: no growing trend across three consecutive reloads. The log shows no shader compilation during play;
blocking shader preload waits (up to 1.15 s) occurred only in frames 1–7 at startup, before warm-up discard.
Against the TDD 18.8 baseline, 99% of frames are at or within 0.46 ms of the 60 fps target (worlds 2 and 4
meet it, worlds 1 and 3 miss by 0.19 and 0.46 ms), no frame streak is sustained, memory does not grow and no
shader compiles in play. The single 108 ms frame in world 4 has no hitch, GC or streaming log entry; that
world ended on the requested exit, and the frame remains unexplained rather than excluded.

Limits: a Development build, not Shipping; the player stood at M12's start, so this is startup, streaming,
steady exploration and reload memory, not the worst-case combat capture P2 also requires; one run.

## Packaged crash during the played capture

The creator's packaged `Play-Aurelion.ps1 -PerfCapture` session reached real combat (twelve enemy deaths across
E1 and E2), then crashed at the Selene handoff with `Assertion failed: AbilityActorInfo.IsValid()`. The call
path was `USovSaveSubsystem::Tick` → `CaptureAndWrite` → `UNarrativeSaveSubsystem::CreateActorRecord` →
`ANarrativeNPCCharacter::IsSaveRecordDestroyed` → `UAbilitySystemComponent::GetAvatarActor()`. The checkpoint
capture reached an NPC whose ability system had no actor info (the respawned companion or a corpse awaiting its
90-second cleanup), and the asserting accessor terminated the game. Because the capture exports only when a
world ends normally, the played combat report was lost with it.

`IsSaveRecordDestroyed` now reads the avatar only when actor info is valid; without it the record is not
destroyed. The three `SovSaveSubsystem` player-avatar comparisons (save admission, restoration capture and
restore-owner matching) use the same non-asserting read, because a handoff can transiently leave the player's
ability system without actor info on the save tick. A new regression test captures an NPC whose actor info
was cleared. Automated routes had not reached this state, likely because they reach the handoff sooner than
the corpse cleanup delay. This is a save/reload (R1, R2) and packaged-execution (P1) defect, not a
performance-capture defect.

## Creator decisions on relief and route retries

`EastUnstickRoute-20260915-153804-dad2354e` died in E1 again. The half-health trigger opened relief at 48.5 s
(health 45.1, shield 0), but the fixed 4-second window lapsed at about 52.6 s with health still 33 and no shield,
and the 15-second cooldown blocked renewal until about 63.5 s. A drone burst at 55.3 s left 6.2 health; the
player died at 58.6 s having defeated none of the four drones.

On 15 September the creator decided two design questions:

1. **Relief persists while resources stay low.** An active relief now renews while the player remains nearly
   out of resources, lingers for its duration after recovery, and its cooldown counts from the lapse. The
   existing no-renewal-after-lapse test is unchanged; a new test proves renewal while low and none after
   recovery.
2. **The route pilot may take the native death retry.** Instead of ending the run on death, the pilot follows
   the game's own fatal recovery and encounter retry like a player, within a small bound, and every death,
   restore and resume is recorded as R1 death/retry evidence rather than hidden.

## First route with persistent relief and native retry

`EastRetryReliefRoute-20260915-154907-170d30ae` ran both creator decisions. E1 lasted 175 s, longer than the
115–121 s passes, and the player dealt 2,140 native damage across 104 observed receipts. Relief opened at
170.7 s (health 32.7, no shield) and, unlike the previous run, stayed active while resources remained low
until death at 175.6 s. It slowed but did not stop damage already committed: drone hits of 12 landed at
174.5 and 175.5 s, and the player died with two of the drones alive. Persistent relief is working as
decided; whether it is enough recovery for an ordinary player remains study evidence, not a pilot claim.

The death then exercised native recovery. The fatal recovery component moved from ResolvingFatal to
Retrying, and because the encounter retry could not start for a dead protagonist it fell back to the
checkpoint 0 load. That load briefly left the campaign without an active mission, and the pilot's per-tick
mission assertion ended the run 1.5 s later. This was a pilot defect, not a game failure: nothing reported
a failed recovery. The pilot now treats pawn, controller and mission absence during recovery as transient
within its 60-second bound, re-resolves the E1 director and rebinds its damage observer if the protagonist
is respawned. Companion damage share was zero, as expected in Tarrik's solo E1; the A4 15–25% measure
needs a completed route through Selene's encounters.

## E1 passes with relief; a scene refuses to play on a blocked exit mark

`EastRecoveryLoadRoute-20260915-160009-0a4a4dc8` passed E1 in 291 s with no death, so the native retry was
not needed, and then passed E2, E3 entry and E3 rescue. It failed at E4 entry: after `DestroyReformationCage`
completed, the `ShareIsolatedThreatData` scene moved from Loading to Failed in 60 ms with the native reason
`Cinematic exit capsule overlaps blocking geometry`. `ValidateParticipants` ignores every scene participant
and its attached actors, so the exit mark was occupied by some other character, and the same beat had played
in both earlier passing routes. The pilot asserted on the failed phase and ended the chain, but the native
request actor kept accepting play requests afterwards, and `RequestPlay` admits a replay from the Failed
phase.

That makes this a recoverable refusal rather than a dead end, so the pilot now replays the station up to
three times, four seconds apart, and records each refusal with a census of the characters standing within
2.5 m of every authored exit mark. The census is the evidence needed to decide whether this is ordinary
transient occupancy or a placement defect: if a rescued survivor or companion idles on the mark, an ordinary
player could face the same refusal, which is a content and design question rather than a pilot one. The
shared scene observer keeps its strict default; only this driver overrides it.

Damage share across E1, E2 and E3 was 4,413 to the protagonist and zero to companions in 184 native
receipts, all against required hostiles. The A4 band needs the encounters where a companion fights.

## Score

**P2 moves from 0 to 2.5 of 5** (the rubric's 0.5 level: material subset on the approved target with the
current outcome incomplete, because worst-case combat is not yet captured). All other rows are unchanged
pending the route, soak and study evidence, so the supported slice estimate is **66.25** (from 63.75).
