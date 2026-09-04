# Native enemy combat selection

This slice extends Narrative's existing combat abilities, ASC and attacker-token leases. It does not replace enemy payloads, Behavior Trees, NPC definitions or movement. The native task is ready for the existing Blueprint combat tree to use; no `.uasset` assets were available or edited.

## What changed

The previous native queries searched one input tag, returned the first activatable instanced ability, and returned a 1 cm attack range when none could activate. A Blueprint task that pressed a shared input could also activate multiple granted specs.

The new chooser enumerates granted `UNarrativeCombatAbility` actions with a valid input tag and selection enabled; this includes all current native enemy attacks. Event-only reactions and passive grants should remain outside this action repertoire. It filters stale/holstered weapon grants, exposes range/LOS/native availability, preserves native GAS cost/cooldown/Handler authorization checks, ranks ready attacks deterministically and activates exactly one granted spec. Equal priorities use least-recently-used order, then class path and grant order. No random stream or input fan-out is involved.

The existing `GetBotAttackRange(InputTag)` and `GetBotAttackFrequency(InputTag)` signatures remain. Range now survives cooldown and temporary state blocks. An empty repertoire has a 250 cm positioning fallback, not 1 cm. `GetBotCombatMovementRange` prefers a ready attack's positioning range, otherwise the closest granted attack band. It never authorizes an attack merely because its movement range is useful.

## Native APIs

On `UNarrativeAbilitySystemComponent`:

| API | Contract |
| --- | --- |
| `GetBotAttackCandidates(Target, InputFilter)` | Snapshot of granted combat specs, their geometry and current availability. Empty filter includes primary, alternate and Ability1/2/etc. No mutation or activation. |
| `SelectBotAttack(Target, InputFilter, OutCandidate)` | First ready candidate in deterministic rank order. Repeated reads do not rotate the choice. |
| `TryActivateBotAttack(Target, Handle)` | Authority-only exact-spec activation. Revalidates source, target, native activation gates, range, LOS and source weapon before and after token callbacks. |
| `TryActivateBestBotAttack(Target, InputFilter, OutCandidate)` | Tries ready candidates in rank order until one actually activates. Failure of a special can fall back to a ready basic attack. |
| `GetBotCombatMovementRange(Target, InputFilter)` | Positioning distance remains available while temporarily blocked or cooling down. |
| `IsBotAttackExecutionValid(Target, Handle)` | Active-task guard for death, changed possession, invalid targets, weapon/state changes and lost owned leases. Allows the active ability's Busy/IsFiring states. |

Activation sets the AI controller's gameplay focus to the requested target. Native Hound and Drone payloads already consume this focus. Read-only native availability checks observe the controller's current focus; set that focus before using those snapshots for UI/debugging or movement decisions. The provided BT task does this.

Each successful activation receives an exact-spec input tap and exact-spec release. The selected ability's normal `EndAbility` remains responsible for completing its payload. The chooser also enforces that ability's authored attack frequency between accepted activations; native payload cooldowns remain in force independently.

## Ability authoring options

`UNarrativeCombatAbility` exposes:

- `bBotSelectionEnabled`: turn off for passive, reaction, defensive-only or player-only grants.
- `BotSelectionPriority`: higher priority first; equal priorities rotate when they successfully activate.
- `bBotRequiresLineOfSight`: defaults true. Disable only for explicitly authored indirect attacks.
- `bBotRequiresAttackToken`: defaults true for selected direct attacks.
- `GetBotAttackMinimumRange` and `GetBotAttackMaximumRange`: default 0 and existing `GetBotAttackRange`. Existing range remains the preferred positioning distance.
- `RequiresBotAttackToken` and `ManagesBotAttackToken`: narrow extension points for payloads with established token ownership.

Hound metadata exposes its existing actual minimum/maximum ranges and self-managed token lease. The chooser never reserves a second token for Bite/Pounce/Horn Charge. Existing Handler authorization still rejects unsanctioned Horn Charge; Sever does not remove independent attacks from the repertoire. Handler Command Hound does not reserve a direct token itself because its dispatched Hound owns the attack reservation.

Other direct attacks use an ASC-owned lease tied to the exact spec. Ending/cancelling that spec releases newly acquired ownership. A borrowed Behavior Tree token remains with the behavior that originally owned it. Availability checks include Narrative's existing free-slot and token-steal rules; activation still makes the authoritative claim. Component teardown removes only leases owned by this chooser.

## Blueprint integration

1. In the existing combat Behavior Tree, replace the task that broadcasts `AbilityInputTagPressed(Attack)` with **Sovereign: Use Combat Ability** (`USovBTTask_UseCombatAbility`). Keep the existing target acquisition and movement branches.
2. Set `TargetActorKey` to the hostile target actor. Leaving the key empty uses current AI focus. An assigned key with a null value fails instead of targeting someone else.
3. Leave `InputFilter` empty for the complete repertoire. Set a filter only for an intentionally restricted branch.
4. Optionally assign `DesiredRangeKey` to a float blackboard entry used by movement. A native/API service can also call `GetBotCombatMovementRange` as the target moves. The task updates the key each time it attempts an attack.
5. The task yields for `NoReadyRetryDelay` (default 0.2 seconds) before reporting that no attack is ready, preventing a tight empty retry loop. Keep any additional authored movement/retry branch. Do not add an unconditional basic-input press on failure, which would bypass selection/token rules.
6. Existing BT claim/return-token tasks can remain: the selector borrows and preserves their token. Alternatively, let the chooser acquire its own token. Do not reserve a direct-ability lease externally before calling the chooser.
7. Set `MaximumAbilityDuration` above the longest authored attack duration. Default is 12 seconds. The task waits for the selected ability to end, fails on cancellation, and cancels a stuck activation at timeout.
8. Retain `bCancelAbilityOnAbort=true` unless the attack is deliberately allowed to outlive its branch. The task observes exact-spec end events and will not cancel a later activation after the selected one has ended. It restores previous focus only while it still owns current focus.

Grant native abilities through the existing NPC definition/ability configuration. No new loadout registry is needed. Enemy-specific montage, projectile, cue, VFX, sound and payload Blueprint configuration remains required where the existing native ability declares it.

## Validation and acceptance

Added real GAS/world automation tests under `ProjectVelkorran.Campaign.BotSelection`:

- `RepertoireAndExactSpec`: all inputs, legacy filtering, stable choice, one activation/release among shared-input grants, no simultaneous attack, cooldown fallback to Ability2 and the remaining basic attack.
- `AvailabilityAndMovement`: native rejection, priority, minimum range, occlusion, usable movement range while blocked, cinematic state, friendly targets, revoked handles and zero-health targets.
- `TokenOwnership`: exhausted budget, successful reservation, generic active-attack validity under its own Busy tag, device shutdown invalidation, cancellation cleanup and preservation of a borrowed BT token.

These tests are authored but have not run here. Unreal Engine 5.7, UHT, UBT and the project's binary assets are unavailable in this environment. `git diff --check` is the local whitespace check, not an engine build. Run the existing `Scripts/Validate-Unreal.ps1` with `-TestFilter ProjectVelkorran.Campaign.BotSelection` in the UE 5.7 workspace.

PIE acceptance remains required: Drone gun/rocket variants; Handler-issued Horn Charge followed by Sever and independent Hound Bite/Pounce; two same-input grants firing only one attack; weapon swap, cinematic interruption and target death mid-attack; token exhaustion/recovery; a blocked/cooling-down enemy approaching a useful distance; BT abort and timeout; NPC destruction and respawn. Verify native/BP attack presentation still finishes the actual GAS ability. No Level 1/2 end-to-end claim is made until the authored trees and encounter content are wired and played.
