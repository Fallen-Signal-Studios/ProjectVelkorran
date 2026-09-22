# Departure checkpoint companion movement

Status: native ordering repair implemented September 22; repeat runtime
qualification is in progress. The earlier intermittent failures remain retained.

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

## Source cause and repair

`USovConvergenceCompanionState::CommitStaged` activates activities and calls
`SetLeader`, which installs Regroup. It then publishes the companion and enables
movement through `SetProxyStaged(false)`. Separately,
`ASovAurelionDeparturePresentation` ticks every 0.2 seconds and requests an
explicit self-targeted HoldPosition once the completed M13 route is idle.
HoldPosition captures the actor's position when requested. This permits a
follow interval before the departure hold captures its location; runtime
evidence has not yet captured the failing interval with this observer.

`USovConvergenceCompanionState::CommitStaged` now establishes an accepted
self-targeted HoldPosition after `SetLeader` and partner registration, while
staged movement remains disabled and before `SetProxyStaged(false)`. It does so
only for the actual `USovAurelionContraryWitnessMissionDefinition` with the
exact M13 ID, succeeded mission, and completed SeparateDepartures beat. The
existing active-companion rebind path also restores the hold after SetLeader.
Unfinished and unrelated routes retain Regroup. A rejected hold aborts staged
publication; reentrant ownership is rechecked before publishing. The departure
presentation actor retains its normal scene-completion role. No transform is
snapped and the acceptance tolerance is unchanged.

The existing `SeparateApproachesAndSavedMembership` native test now exercises
an established partner through the no-new-stage commit: incomplete M13 keeps
ordinary Regroup, completed SeparateDepartures accepts an explicit self-hold
without moving the actor, and repeated commit retains the hold. Existing tests
also cover accepted holds during suspended activity selection and staged proxy
publication. The precise staged-release order is checked by the earned CP9 PIE
reload below; the native test does not directly simulate a completed M13 stage.

The September 22 user brief explicitly authorizes routine reversible project
repairs and prioritizes real in-engine testing. That supersedes the earlier
handoff restriction against native edits for this scope.

## Validation

Full gate `Saved/Validation/20260921-003717-0570c7bf` completed successfully:
build exit 0, all 726 matching automation tests passing, report coverage and
source integrity passing. It did not use SkipBuild. The baseline gate was
`20260921-001410-e2a4e17f`. These existing tests do not establish a repair for
the intermittent displacement. M12 remains at its protected SHA-256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.

Post-repair full gate `Saved/Validation/20260922-160330-1044df19` passed the
build, all 726 matching automation tests, report coverage and source integrity
without SkipBuild. The extended native approach test passed. Earned CP9 PIE run
`DepartureHoldAfterRepair-20260922-160548-54c61d7e` passed the unchanged
original reload comparison and its final comparison. Across 348 samples Selene
first appeared at x=1350, y=48000; after brief vertical floor settling she
remained stationary at (1350, 48000, 90.15) through game time 24.07. The maps
remained unchanged. This is one successful run; a four-run repeat is underway.

The four additional earned loads completed in
`DepartureHoldSoak1-20260922-160756-fc2ff75a`,
`DepartureHoldSoak2-20260922-161017-f9411981`,
`DepartureHoldSoak3-20260922-161157-dcb45d72`, and
`DepartureHoldSoak4-20260922-161333-6560b106`. Each retained the original
reload pass, passed the fifteen-second final earned-state comparison, and
exited the editor normally. This is five post-repair successful loads total.
It raises confidence in the previously intermittent checkpoint behavior but
does not prove that all future timings or other campaign checkpoints are safe.
