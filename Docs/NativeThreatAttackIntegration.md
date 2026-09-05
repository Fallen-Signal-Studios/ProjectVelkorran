# Native enemy attacks consume threat authorization

This integration extends the controller contract in `ThreatMemoryEngineering.md` to enemy paths that can execute outside Narrative's normal behavior-tree attack selector.

## Gap and correction

Drone and Hound abilities could reacquire the nearest living hostile directly from the world, read an AI focus actor's current position, or snapshot a target after perception was lost. A released rocket retained its homing component indefinitely. The generic attack-selector gate alone did not protect these routes.

`Combat/SovThreatTargeting.h` is a small query adapter over the existing `ANarrativeNPCController::CanDirectlyTargetThreat`. It owns no memory, selection state or perception service. Existing life, faction, ability ownership, timing and damage validation remain in their original owners.

| Native path | Integration |
| --- | --- |
| Drone activation and gun burst | Reject invalid actor focus during admission and immediately before each actual shot. This catches cloak/lost sight between behavior-tree updates. A fixed focal point remains usable for deliberate suppression without following an actor. |
| Drone rockets | Validate actor aim and homing acquisition at release. While homing, an actor tick precedes the existing projectile movement tick and retires tracking on lost authorization or replaced source ownership. Physical velocity, collision and lifetime remain active. Tracking never reacquires later. Replicated tracking retirement updates homing without resetting a proxy to its original muzzle velocity. |
| Explosive drone | Focus preference and nearest-target fallback both require direct authorization. Validate again during pursuit, after presentation callbacks and before/after requesting a move. Once the warning has armed, its existing physical timed explosion does not require the original target to remain visible. |
| Hound | Focus preference, nearest-target fallback and windup release all require direct authorization. Pounce validates before snapshotting the target position. A committed charge or pounce continues its existing fixed physical path, without hidden-target tracking. |
| Handler | Its existing exact Hound ability remains the hostile-target selector. The Handler validates the returned target against that Hound's controller before accepting the order, including changes during synchronous activation callbacks. Handler-to-Hound command eligibility and link ownership remain unchanged. |

`IsHostileTarget`, `IsValidAttackTarget`, projectile contact and radial-damage predicates intentionally remain life/faction based. Concealment is not immunity: an existing charge, fixed-position shot or explosion can still hit a concealed character physically in its path or area.

## Validation and acceptance

Three engine regression suites are authored using actual NPC controllers, registered sight stimuli, GAS abilities and projectile movement:

- `ProjectVelkorran.Campaign.Threat.NativeAbilityAcquisitionAndWindupLoss`: direct Hound activation cannot acquire an unobserved nearest target; current sight permits it; loss before the real impact timer prevents the delayed snapshot. Explosive-drone world scanning also rejects the unobserved target.
- `ProjectVelkorran.Campaign.Threat.NativeBurstLossAndCollateral`: one real drone shot lands; cloak between BT ticks stops the following tracked shot; subsequent fixed-position suppression can still physically damage the concealed actor.
- `ProjectVelkorran.Campaign.Threat.ReleasedRocketRetiresHoming`: the existing rocket movement homes under current sight, then loses only tracking before its next movement step. Renewed sight cannot reactivate that tracking, and nearby splash still damages a concealed target.

These are **authored, not executed** in this container. `git diff --check` passed. Required gates are UE5.7 UHT/editor target compilation, these tests, the generic threat suites, existing Hound/Handler/Drone/projectile-defense tests and PIE with configured NPC perception. Validate target loss during windup, between burst shots, during pursuit, after explosive arming, during released homing and during an actual Handler dispatch. Also verify proxy flight when homing retires and when reflection changes the flight velocity.

The main change risk is intended behavior: perception-managed enemies can no longer use a native world scan to bypass missing sight. Legacy controllers without a managed perception contract retain the controller's documented fallback; generic non-Narrative controllers receive concealment rejection but do not gain a second threat-memory implementation.
