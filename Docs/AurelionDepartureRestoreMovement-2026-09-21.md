# Departure checkpoint companion movement

Status: intermittent displacement remains unresolved. No native repair applied.

The unchanged earned CP9 acceptance check has failed with Selene 8.4 cm and
30.2 cm toward Tarrik from her saved position. The second failure was already
stationary, so waiting for zero velocity alone would not repair the saved pose.
Those runs are recorded in `AurelionSeleneHeldColorProbe-2026-09-20.md` and
`AurelionM13DeparturePaving-2026-09-21.md`.

## Passive observation

`Scripts/Validation/Aurelion/observe_m13_departure_settle.py` wraps the existing
earned-checkpoint checker. It retains any original failure, observes fifteen
seconds beyond that result, and repeats the original earned-state comparison.
It records actual character position, velocity, staging visibility, appearance
readiness, path status, activity, goal identity and active animation montages.
The underlying checker copies the original earned save into its isolated run;
the observer does not change characters, command AI, or manufacture progress.

`DepartureMovementTrace-20260921-002913-c6dbc5b2` completed with 341 samples,
no observer errors, and both original and final earned-state checks passing.
Selene remained at x=1350, y=48000 while staged. The first visible sample at
game time 3.5993 had the second command goal selected, no active path, and a
brief downward floor-settling velocity. By 3.6814 she was stationary at
z=90.15. Horizontal position remained unchanged through game time 24.4140.
This is a successful restore, not a reproduction or a reliability claim.

A second process, `DepartureMovementRepeat-20260921-003447-c9395962`, also
passed both comparisons with no observer errors. Its final sample at game time
23.6859 was stationary at (1350, 48000, 90.15). Two successful observed runs do
not invalidate the earlier intermittent failures or prove the proposed cause.

The goal target property is protected from Unreal Python. Its recorded value
`unexposed` must not be interpreted as a null target. Earlier adapter attempts
failed on unavailable Python methods and are not gameplay failures.

## Source lead and proposed repair boundary

`USovConvergenceCompanionState::CommitStaged` activates activities and calls
`SetLeader`, which installs Regroup. It then publishes the companion and enables
movement through `SetProxyStaged(false)`. Separately,
`ASovAurelionDeparturePresentation` ticks every 0.2 seconds and requests an
explicit self-targeted HoldPosition once the completed M13 route is idle.
HoldPosition captures the actor's position when requested. This permits a
follow interval before the departure hold captures its location; runtime
evidence has not yet captured the failing interval with this observer.

A proposed native repair would establish the departure hold before releasing
staged movement, only for the actual completed M13 SeparateDepartures route.
It must retain mission/identity/readiness validation and reentrant ownership
checks, fail safely if the hold cannot be accepted, and cover repeated commits
that currently call SetLeader again. Ordinary incomplete routes must still
Regroup. The presentation actor can retain its normal scene-completion role.
Do not snap transforms, loosen the acceptance tolerance, or disable normal AI.

Regression coverage should exercise actual companion staging and publication:
completed departure has an accepted self-hold before movement is enabled;
incomplete and unrelated missions retain Regroup; repeated commit does not
release the departure hold; rejected/reentrant commands do not publish a stale
proxy. Existing tests cover accepted holds during suspended activity selection
and native staged publication separately, but do not prove this ordering.

The engineering handoff's native-change restriction requires approval for this
additional campaign ownership repair. The existing cinematic startup approval
does not establish approval for it. No production source has been edited here.

## Validation

Full gate `Saved/Validation/20260921-003717-0570c7bf` completed successfully:
build exit 0, all 726 matching automation tests passing, report coverage and
source integrity passing. It did not use SkipBuild. The baseline gate was
`20260921-001410-e2a4e17f`. These existing tests do not establish a repair for
the intermittent displacement. M12 remains at its protected SHA-256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
