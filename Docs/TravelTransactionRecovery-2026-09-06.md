# Mission travel ownership and origin recovery

Baseline: `93bf5c2c6e292e7ca0df05c3fe3ae677cadca57e`. Implements the source portion of the 6 September alignment audit's travel/recovery slice against August TDD §§11.7–11.9, 15.4, 15.9 and 18.5.

## Behavior

Ordinary non-seamless mission travel now requires a successfully written, readback-verified origin checkpoint. Acknowledging a failed checkpoint write remains available for the existing in-world handoff policy, but does not authorize abandoning the origin map without a durable checkpoint.

`USovSaveSubsystem`, owned by the GameInstance, retains the exact origin envelope and decoded Narrative snapshot before travel. This uses the existing pending-operation exclusion for saves, cloud imports, profile changes and other loads. The owner records account namespace/local user, source checkpoint generation, source controller/pawn/PlayerState/ASC and transition epoch, destination mission asset/map, operation GUID and phase request GUID. The player-only travel slot serializes the operation GUID and origin generation alongside the destination mission. GameMode consumes the request-bound readback retained by the owner; it no longer loads whichever record currently occupies a per-account slot.

The native `UEngine::OnTravelFailure` delegate and the existing 120-second foreground watchdog detect accepted-then-failed travel. The failure delegate only records failure. Recovery occurs on the core ticker after initialization/failure callbacks unwind. It makes one attempt to load the original checkpoint using the existing phased Narrative world-load path. Recovery rotates the phase request GUID and retains the operation GUID, so the original destination cannot subsequently publish completion. Suspension holds the deadline. An engine failure event carries no request URL, so the handler requires the current phase token captured by its registered delegate and a known source/destination world in the same GameInstance.

Recovery retains the original checkpoint bytes and A/B save banks. It never reports that the failed destination mission succeeded. Successful origin recovery reports a successful load with an explicit origin-recovery message. A second failure ends the pending operation, preserves the retained origin envelope, and broadcasts recovery availability. `HasTravelRecovery` and `RetryTravelRecovery` provide an explicit further retry after the original platform account is available; no unbounded automatic reload loop runs.

Destination acceptance validates URL phase/request, account, mission asset and actual map package before player staging. PIE prefixes are removed only from the map's short package name for comparison. Managed restore binds the exact controller, pawn, PlayerState, ASC, actor-info epoch, pawn initialization generation and controller transition epoch before applying protagonist records. Only that bound restoration can publish readiness. In-world protagonist rollback remains in its existing controller owner.

## Validation and remaining engine gates

New native regressions exercise phase/request/account rejection, the real engine failure delegate, old callback isolation across operations, deferred origin preservation, one automatic recovery, rotated recovery request identity, account-scoped explicit retry, and rejection after an ASC retires and rebinds to the same pawn pointer. The recovery transport is injected in the automation test so no authored map is required; request construction, pending-state transitions and recovery-envelope retention still execute production code. Existing save test staging now declares the account and actual map metadata required by the stricter validation.

These native tests were added but have not been executed in Unreal in this environment. Required next engine gates: compile current UE 5.7 Editor and Development game targets; run `ProjectVelkorran.Campaign.Travel` and `ProjectVelkorran.Campaign.Save`; test missing-package/load-map failure after `ServerTravel` acceptance, source teardown, destination actor/pawn restore rejection, timeout, suspend/resume and account revocation; verify the recovery menu on the authored frontend and campaign maps.

This is an operation-level recovery contract, not atomic rollback of a partially mutated world. Recovery reconstructs the origin through a new load. Across process termination, the durable origin checkpoint remains selectable from normal saves; the in-memory operation is deliberately not resumed from a stale travel URL. A failed origin map or unavailable original account can require explicit recovery after the fault is repaired.

Native travel failure API reference: [Epic UEngine::FOnTravelFailure](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UEngine/FOnTravelFailure).
