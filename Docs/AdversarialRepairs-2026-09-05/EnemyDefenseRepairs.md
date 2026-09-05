# Enemy and defense engineering repairs — 2026-09-05

Scope: ED-03, ED-06 and ED-07 from the adversarial enemy/defense audit. This is a source repair report, not an Unreal compilation, gameplay acceptance, or console certification claim. Existing GAS abilities, Narrative attributes, resource components, projectile classes and presentation actors remain authoritative.

## Implemented

| Finding | Repair and resulting contract | Integration risk |
| --- | --- | --- |
| ED-03: Deflection startup/cleanup ownership | The ability claims a specific component/window generation before the component's synchronous tag/start callbacks. Cancelling from either callback closes that generation and stops startup before cost/montage/recovery continuation. A deferred GAS end retains the exact original component even if the avatar binding changes. Component-owned tags are marked owned before GAS callbacks and retired before removal callbacks. End is guarded by GAS validity/scope locking and idempotent cleanup. | Existing Blueprint children may have relied on continuing their start hook after cancellation. Extra external Busy contributions now interrupt defense; the ability's own Busy count is explicitly allowed. |
| ED-06: Shield/Poise continue after death or avatar retirement | A shared private owner predicate requires the authoritative component owner to be the ASC's current avatar, positive finite Health, no Narrative death and no Dead/Fatal tags. Health and life-tag delegates immediately retire timers and owned state. All timer/manual recovery writes enforce that predicate. A real revive on the same owner starts fresh recharge/regeneration delays; elapsed dead time is never credited. Lifecycle, resource-change and Poise-state generations stop obsolete callback stacks. Foreign tag contributions survive cleanup. | Restores must continue to use the existing checkpoint restore/reset seam. A Health value alone does not revive an ASC while Dead/Fatal remains present. Retired avatar timers stop at their next callback if an avatar change emits no readiness/life signal; they cannot write during that interval. |
| ED-07: Drone interruptions disappear between timer polls | Native interruption listeners cover death, Fatal, Poise break, freeze, DeviceDisabled, interaction, ragdoll, sequencer control and BlockFiring. Release, recovery, watchdog, gun-burst, pursuit and fuse callbacks validate activation generation. An obsolete generation cannot finish/cancel a replacement activation; a current generation with retired ownership cancels itself. Scoped end handling preserves derived burst/self-destruct cleanup. Release hooks and deferred spawning are checked before continuing. | Authored callbacks that disable and immediately re-enable a drone now cancel its current attack. A rocket that completed spawning remains an independent committed projectile. |
| Self-destruct contract strengthened during ED-07 | The committed blast retains its original source, source object, effect level, location and presentation through movement-abort and damage callbacks. Source death/interruption does not erase the remaining outward blast. Post-blast ability completion targets only the original activation. Fatal self-damage cannot follow an ASC into a replacement avatar. Pursuit cleanup uses the exact path-following component and request it acquired. | Target/source actor destruction still causes normal liveness checks; committed damage does not attempt to dereference destroyed actors. Navigation and authored warning presentation require engine validation. |

Deflection direct entry also rejects incompatible defense/action states, nonfinite tuning/resource values and an ASC bound to another owner. Resource state tags are still authored by their original components; no second attribute store, AI framework or combat resolver was added.

## Files

- `Source/ProjectVelkorran/Public/Abilities/SovGameplayAbility_SeleneDeflection.h`
- `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_SeleneDeflection.cpp`
- `Source/ProjectVelkorran/Public/Components/SovDeflectionComponent.h`
- `Source/ProjectVelkorran/Private/Components/SovDeflectionComponent.cpp`
- `Source/ProjectVelkorran/Public/Components/SovShieldComponent.h`
- `Source/ProjectVelkorran/Private/Components/SovShieldComponent.cpp`
- `Source/ProjectVelkorran/Public/Components/SovPoiseComponent.h`
- `Source/ProjectVelkorran/Private/Components/SovPoiseComponent.cpp`
- `Source/ProjectVelkorran/Private/Components/SovResourceOwnerPolicy.h`
- `Source/ProjectVelkorran/Public/Abilities/SovGameplayAbility_ReformationDrone.h`
- `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_ReformationDrone.cpp`
- Native test files: `SovDefenseLifecycleTestFixtures.h`, `SovDefenseLifecycleTestFixtures.cpp`, `SovDefenseLifecycleRuntimeTests.cpp`, originally added under the existing `Private/Tests` tree for the coordinated test-module migration.

## Verification and required engine gate

Executed here:

- `python Scripts/Test-NativePolicies.py`: **39 portable production-policy suites passed** with the runner's C++17 warnings-as-errors and undefined-behavior sanitizer settings. This is broad portable regression coverage; it does **not** execute GAS, timers, or the new Unreal tests.
- `git diff --check` for the changed defense/resource/drone source and headers: passed.
- Read-through of callback ordering, tag contribution ownership, timer generations, deferred end dispatch, source/target liveness and native test fixture/tag references.

Added native runtime regressions, **not executed here**:

| Automation test | Required observation |
| --- | --- |
| `ProjectVelkorran.Campaign.Deflection.ReentrantStartAndOwnedTags` | Cancellation during real tag-add or component-start callbacks leaves no owned defense/Busy state or late ability-start hook. Subsequent activation works. Effect-owned Deflecting and external Busy contributions survive cleanup. |
| `ProjectVelkorran.Campaign.Resources.DeathReviveAndRetiredAvatar` | Timers first demonstrate actual living regeneration, then stop on Health zero. Dead resources remain unchanged. Dead/Fatal gates must both clear before fresh revive delays start. Reassigning ASC avatar prevents the old component from writing. |
| `ProjectVelkorran.Campaign.Resources.BrokenDeathDoesNotRecover` | Real Poise attribute depletion starts Broken; death cancels its fallback and removes only the component's tag count. Neither refill nor recovery immunity appears afterward. |
| `ProjectVelkorran.Campaign.Drone.SynchronousReleaseInterruption` | A real Blueprint release callback briefly grants DeviceDisabled. The attack cancels immediately and leaves no deferred rocket. A later activation is usable; effect-owned interruption tags remain intact. |
| `ProjectVelkorran.Campaign.Drone.CommittedBlastSurvivesSourceInterruption` | Two hostile targets receive a real committed radial blast even when the first target's resolved-damage callback interrupts/kills the source's combat lifecycle. |

The native fixture uses real Narrative ASCs, attributes, dynamic delegates, gameplay effects, GAS activations and world timers. It intercepts the actual Blueprint `ProcessEvent` hook instead of substituting a mirrored release implementation.

Before accepting ED-03/06/07 as engine-validated: run UHT and UE 5.7 compilation, these five tests, existing Guard/Threat/Drone/Deflection coverage, and an in-engine avatar-switch/checkpoint/death sequence. Validate self-destruct path cancellation against StateTree/BT move ownership and committed presentation on standalone/listen/dedicated authority paths. Console performance and certification evidence remain separate gates.

## Related shared repairs

ED-01 damage transactions, ED-02 pickup reentry, ED-04 finisher progression and ED-05 weak-point history are owned by the coordinated root repair. An independent ED-01 review identified additional required callback guards: exact target ownership after resource/delegate/policy callbacks, suppressing stale death delivery after revival/avatar replacement, and validating the original source avatar before killed-enemy dispatch. Those requests were sent to the resolver owner; this report does not claim their implementation through the files above.
