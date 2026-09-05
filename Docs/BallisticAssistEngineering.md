# Gravity-aware projectile assistance

Authority: TDD v2 §§6.9–6.10 and the projectile-lead accessibility option in §13.9. This closes the native launch gap recorded as R5 in `TDDNativeCompletion.md`; UE5.7 execution remains a required gate.

## Audit and disposition

| Finding | Existing implementation | Required correction | Disposition, dependency and risk |
| --- | --- | --- | --- |
| Gravity was excluded from assistance | `SovAimAssist::GetProjectileLead` solved constant-speed, zero-gravity interception for Hunger only. Cinder already had its own stationary ballistic launch. | Lead actual moving hostiles while respecting each attack's launch speed, range, correction limit, gravity and lifetime. | Extend the existing aim helper and existing release paths. Preserve ability costs, weapon gates, source validation, damage, fuse and collision owners. |
| A clear straight ray did not prove a clear projectile path | Target acquisition and prediction used visibility lines. | Assistance cannot steer a physical projectile through geometry or ignore its collision volume. | Sweep the solved arc using the configured projectile sphere, collision channel and response container. The actual projectile still resolves every real impact. Conservative rejection may reduce assistance near clutter. |
| Stillpoint used a different physics model | Its native swept motion subtracted hard-coded 980 cm/s² and then advanced using the new velocity. | The launch solver and flight must share world gravity and coherent integration. | Extend its existing motion with analytic constant-acceleration position updates at its existing bounded substeps. This intentionally removes frame-step-dependent ballistic drop. Keep its field, sweep, timers and damage paths. |
| High arcs and speed clamps could invalidate a prediction | Cinder permits an authored high arc; Hunger caps movement speed at launch speed. | Assistance must preserve those attack contracts. | Reject a requested high arc outside the horizon rather than substitute a low arc. Reject solutions whose launch or impact speed would engage the existing movement cap. No movement cap is changed. |

## Delivered native consumers

- **Cinder Sticky Grenade:** `ReleaseCinderStickyGrenadeFromAim` retains the current socket, authoritative aim trace and native static ballistic fallback. The optional assisted velocity uses actual world gravity times `GrenadeGravityScale`, the configured fixed launch speed, aim range, remaining release-time fuse, authored arc choice and the configured class's native sphere/movement defaults. The static solver now also respects the sign of world gravity. The advanced caller-supplied `ReleaseCinderStickyGrenade` entry point preserves the caller's explicit launch.
- **Stillpoint:** its real GAS activation submits the existing 1500 cm/s launch, 18 cm sweep radius, configured fuse and range. Its existing actor advances under world gravity times its native `GravityScale`. `GetVelocity` now reports that actual native velocity. Wake and Dispatch retain their existing movement.
- **Velkorran's Hunger:** its native aimed release now uses the same helper for both zero and nonzero gravity. It supplies its configured collision sphere, range, flight duration and existing speed cap. A speed-capped downward solution is deliberately rejected when the real component could not follow the solved parabola.

Assistance remains opt-in through the existing persistent `bProjectileLead` preference. The old public zero-gravity helper remains a compatibility wrapper; its existing quadratic policy is still the production fast path for zero gravity.

## Bounds and math

The gravity solver finds roots of `|R + Vt - 0.5Gt²|² = s²t²`, where R is target offset, V is measured target velocity, G is world gravity and s is the unchanged release speed. It isolates quartic roots using derivative roots and bounded bisection, including a tangent root at maximum ballistic range. It validates the resulting speed before returning it.

Selection requires a locally possessed shooter, an actual living Narrative ASC/avatar, hostility, current visibility and a narrow aim cone. Both current and predicted target positions must remain inside the weapon range, capped at 50 metres. Prediction lasts no more than **0.6 seconds**, and no longer than the supplied fuse or flight duration. Launch correction is at most **8 degrees**, or a smaller caller-supplied attack limit. No pawn translation, damage increase, resource change, guaranteed hit or post-launch homing is added.

Every accepted arc is swept in at most 36 segments. Each sphere is inflated by that segment's parabola/chord deviation, covering the complete curved volume rather than just the sampled centers. The intended target and shooter attachments are excluded from clearance; other collision bodies remain eligible to block assistance. Current and predicted visibility rays are also required. Any failure returns the exact original velocity.

## Validation

Executed in the container with C++17, `-Wall -Wextra -Werror -pedantic`, undefined-behavior sanitization and no sanitizer recovery:

- `Tests/Portable/SovBallisticAssistPolicyTests.cpp`: **28,477 assertions passed** against production math. Covers constant and oblique gravity, rising/falling/lateral targets, zero-gravity compatibility, both arcs, tangent roots, unreachable/fleeing targets, fuse/horizon rejection, nonfinite input, fixed-speed interception residuals and agreement across time partitions.
- Existing `SovAccessibilityAssistPolicyTests.cpp`: **6,333 assertions passed**, retaining the existing zero-gravity and automatic-sprint checks.

Authored UE automation, not run here:

- `ProjectVelkorran.Campaign.Targeting.BallisticAdmissionAndGeometry`: actual settings owner, local possession, hostility, physics target acquisition, correction/fuse/speed bounds, high-arc rejection, a visible target whose curved path hits overhead geometry, original-velocity fallback and ownership loss.
- `ProjectVelkorran.Campaign.Targeting.NativeGrenadeLeadConsumers`: real GAS Cinder and Stillpoint activation/release, moving-target launch velocity, unchanged fixed speed, one Echo spend, duplicate Cinder release rejection and Stillpoint motion in a world with nondefault gravity.

## Acceptance and remaining limitations

Native implementation order was shared math and admission, Cinder/Hunger launch integration, Stillpoint physics alignment, then regression sources. No additional module dependencies or parallel projectile framework were introduced.

Definition of done requires UE5.7 UHT/editor target compilation, these tests and the existing targeting/Tarrik/Selene payload suites. In PIE, validate ordinary and low frame rates, cover near the muzzle and arc apex, speed-limited Hunger falling shots, Cinder high-arc fallback, target death/possession changes and each authored projectile class. The source-only container cannot execute these engine gates.

The prediction assumes the target maintains its measured velocity and gravity stays constant during its short horizon. Later target movement, world-gravity changes, moving cover and collisions remain authoritative gameplay. Blueprint construction or custom movement that changes the configured collision body, drag, gravity or homing after the native release requires its own integration validation. Arbitrary Blueprint-owned projectile launches and enemy auto-aim are outside this opt-in player consumer. These limitations are not a claim of universal ballistic tracking or engine-tested playability.
