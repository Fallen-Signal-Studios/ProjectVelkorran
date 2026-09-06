# Drone continuation reliability — 6 September 2026

Source-only follow-up to the drone liability in `CodeErrorPass-2026-09-05-Combat.md`.
This extends the existing Narrative/GAS integral-weapon abilities; no parallel
damage, AI, status, projectile, or presentation system is introduced.

## Verified gap and repair

The shared release event previously returned `IsActive()` after authored code ran.
That allowed DeviceDisabled applied in the hook to leave the rocket release live.
Checking only `IsActive()` also accepted the same instanced ability after synchronous
cancellation/restart. A burst presentation callback could reset its counters, then
the retired shot would increment the new counters and arm a timer over them. Self
destruct had equivalent continuation risks around presentation construction,
warning events, and AI `MoveTo` completion callbacks.

The existing base now owns an activation epoch and a witness of its original
world, ASC, avatar, AttributeSet, actor-info epoch and combat-life epoch. Admission
requires a canonical, living authority owner. The same witness is checked after
startup/commit/release callbacks and before delayed work. An A-to-B-to-A avatar
handoff is not treated as uninterrupted ownership. A zero-to-positive Health reset
does not revive a previous attack. Validation can cancel only its own activation,
never a replacement created by a callback.

Nine interruption tags (death, interaction, sequencing, ragdoll, weapon block,
fatal, broken Poise, Frozen, DeviceDisabled) and zero/nonfinite Health now end an
active attack immediately. Listeners are attached to the exact original ASC and
removed during cleanup; externally owned tag counts are left intact. Rebinding
without a tag or Health event is detected at the next native continuation, not
claimed as a new immediate actor-info notification.

Native release, recovery, watchdog, burst, pursuit and warning timers carry their
activation epoch. End requests validate the current spec and GAS end validity.
Scope-locked ends immediately revoke native continuation, while cleanup waits for
the GAS scope to unlock. Derived payload cleanup runs only after that admission;
an invalid end no longer clears gun counters or a live self-destruct fuse. Timers
are cleared on their original world, and montage delegates are retired before the
task ends. Movement cleanup retains the original path-following component and
request ID, so it cannot abort another controller/task's newer request.

## Commit boundaries and preserved behavior

- A gunshot already resolved through damage execution remains committed. Its old
  presentation return cannot schedule additional shots or alter a replacement.
- A rocket becomes independent when its initialized actor enters `FinishSpawning`.
  Cancellation before that boundary destroys the uncommitted deferred actor. A
  rocket already released is not recalled by subsequent source interruption;
  its return cannot finish a replacement action or consume its release gate.
- Self destruct still acquires/pursues a live hostile, stops in range and exposes
  the authored warning window. Before blast commitment, interruption cancels the
  fuse and presentation without outward damage or self death. Target loss after
  arming still does not cancel the fuse.
- After commitment, outward blast and final presentation retain their captured
  source and configuration rather than reading a newly activated avatar. Source
  death/cancellation does not retroactively undo committed damage. An actor-info
  change, destroyed source or restored life stops further use of the old source;
  the final fatal self-hit cannot kill a revived/rebound owner. Death-explosion
  suppression remains on the existing drone base, with no duplicate blast path.
  Its suppression flag is claimed only after fatal-spec construction and exact
  ownership validation, immediately before applying the fatal effect. Rejecting
  a stale fatal spec cannot silently suppress a later ordinary drone death.

This does not change damage tuning, Heavy guard classification, hostile/LOS
filtering, cloak tracking policy, default attack inputs or the default requirement
for manual editor asset setup described in `ReformationDroneAbilities.md`.

## Native regression coverage

`SovDroneContinuationRuntimeTests.cpp` adds 15 automation registrations beneath
`ProjectVelkorran.Campaign.Drone.Continuation`. The fixtures grant and activate real
GAS specs, invoke production release APIs, advance real `FTimerManager` timers,
perform actual gun traces/damage, and exercise actual deferred-actor construction
callbacks. They use one world initialization and an explicit script execution
guard for non-BeginPlay editor fixtures.

Coverage includes all three release-hook DeviceDisabled and cancel/restart paths;
avatar replacement, ABA restoration and life replacement; immediate tag teardown
with external tag preservation; burst presentation disable/restart; independently
committed rocket spawn with action restart; warning interruption/restart; outward
damage restoring the source before the final fatal hit; successful committed
detonation and armed-target loss; windup ownership invalidation; and invalid or
scope-locked end requests. An ASC fixture also overrides the real virtual
`MakeOutgoingSpec` boundary: point-hit interruption/restart, outward/fatal
spec-construction life and actor-info changes, and mutation of the fatal effect
configuration from actual detonation presentation are covered. Source ownership
is rechecked after spec construction, and all committed blast/fatal configuration
is captured before presentation callbacks.

The Editor-only `SovDroneDeathSuppressionRuntimeTests.cpp` adds one further
registration, `RejectedFatalPreservesOrdinaryDeathExplosion`. It uses the real
`ASovDroneNPCBase` and Narrative death callback, not the generic character fixture
alone: a fatal-spec actor-info ABA rejects self death, the committed presentation
is retired, and a later ordinary fatal effect must still produce native death
explosion damage. A fresh ordinary-death control validates the same setup.

## Acceptance gates still outstanding

UE 5.7/UHT is unavailable in the source workspace. These native tests are authored,
not reported as executed or passing. The portable host suites do not compile this
Unreal ability and are only a regression safety check for shared portable policies.

Required engine follow-up:

1. Full non-unity Editor compile/UHT, then run the drone continuation group plus
   existing threat/native combat groups. No Live Coding for the reflected changes.
2. Real AI path-following tests for interruption/restart inside `MoveTo`, controller
   replacement and unrelated StateTree movement during the warning. This source
   pass hardens request ownership but its new fixture does not prove navigation.
3. Listen/dedicated-client tests: one shot packet per committed shot, no warning
   loops left after cancellation, one committed detonation and one fatal self-hit,
   no duplicate ordinary-death explosion, and late-relevancy reconstruction.
4. Authored montage/appearance replacement, freeze/device-disable integration,
   packaged target cook and current-console performance/certification validation.

Definition of done for source delivery: the continuation/ownership changes and
native regressions are checked in with a clean diff and host checks reported
separately. Engine/runtime acceptance requires the gates above; source delivery
alone is not certification of those results.
