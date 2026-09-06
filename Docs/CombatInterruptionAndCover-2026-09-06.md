# Combat interruption and cover correctness

Source baseline: `93bf5c2c6e292e7ca0df05c3fe3ae677cadca57e` (6 September 2026 alignment assessment, slice 2). This change closes source-confirmed interruption and cover gaps in existing attacks; it adds no new hero abilities.

## Implemented contracts

- Drone windup, release, recovery, burst and pursuit timers retain one activation serial. Blocking tags and zero Health immediately retire the activation, including a disable added and removed in the same Blueprint callback. Release callers revalidate after callbacks and cannot cancel, fire, increment counters, or schedule timers for a replacement activation.
- Drone source ownership retains the exact ASC, avatar, combat actor-info epoch and character-ready epoch. Returning the ASC to an old avatar does not revive its retired release. End requests fence continuations before GAS scope-lock deferral; derived cleanup runs after the scope lock releases.
- Rocket construction, gun presentation and pursuit movement callbacks have post-callback fences. A stale move can abort only the request it actually issued. Uncommitted rocket construction is destroyed if its release loses ownership.
- A committed suicide explosion deliberately survives cancellation and source death on the same source generation. Its outward damage values, tags, target identities and source ownership are captured before presentation/damage callbacks. ASC rebinding or checkpoint readiness change stops remaining writes; fatal self-damage cannot be redirected to a replacement avatar.
- Cinder Judgement uses native eye-to-muzzle visibility/weapon obstruction traces, then a forward-converging native muzzle trace. A muzzle already beyond thin cover returns to the eye, preserving that obstacle as the impact instead of shooting backward or detonating through it.
- Judgement freezes its source, actor-info/readiness epochs, level, source object, effects, damage, blast/physics parameters and radial target generations before direct damage. Source rebinding, including switch-away/back, stops the old blast. Radial candidate rebinding cannot redirect damage to a new avatar. Shared Tarrik generation-bound timers own release and recovery.
- Judgement and drone damage presentation consume canonical native damage receipts keyed to the exact effect context and target life. Immediate healing in another damage callback cannot erase a real accepted hit, and forged public broadcasts cannot claim success.
- Shield/Poise passive ownership, restore fences and recovery timers are documented separately in `PassiveResourceOwnership-2026-09-06.md`.

## Verification and remaining gate

Eight new native automation registrations under `ProjectVelkorran.Campaign.CombatInterruption` exercise release-callback disable, cancel/restart, burst disable and avatar retirement, thin cover/forward geometry, immediate healing, source rebind, target candidate rebind, and committed suicide cancellation. The fixtures use real GAS activation, physics traces, damage execution and timer managers, with only authored values and callback probes substituted.

The existing portable policy runner completed 42 suites successfully during implementation. These host checks do **not** execute the changed Unreal classes. `git diff --check` passed. Unreal Editor compilation and native execution remain required; no engine pass, packaged gameplay result or console qualification is claimed.

Engine acceptance: run the new native group and the existing Tarrik/drone, combat-transaction and passive-resource groups on the final source revision. Exercise an authored rocket construction script and warning presentation callback, then repeat checkpoint recovery and protagonist switching. Preserve the committed-suicide lifetime rule when tuning visuals or AI cancellation.
