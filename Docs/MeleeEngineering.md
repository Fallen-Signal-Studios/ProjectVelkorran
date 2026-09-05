# Native melee completion

TDD v2 section 6.8 is implemented through Narrative's existing combat ability, weapon visual, input routing, ASC and damage execution. `USovGameplayAbility_Melee`, `USovMeleeAttackDefinition` and `USovAbilityTask_MeleeSweep` add an executable attack loop; Blueprint supplies authored assets and presentation.

## Behavior and integration

- An attack definition contains one to eight nodes with finite startup, active, recovery and branch windows. Follow-ups must move forward in the array. Each node renews the existing combat attack ID and its Echo receipt.
- Swept spheres span the real weapon mesh's two sockets, with bounded spatial samples and up to 24 temporal subdivisions. Previous/current transforms determine collision; animation time opens the active interval. A per-node actor ledger prevents duplicate damage from blade samples. Environment contacts use a separate presentation event. Source-to-contact cover checks reject a pose that extends through a wall. Teleports, invalid geometry and stale attack IDs stop the task.
- Native damage uses `UNarrativeDamageExecCalc` and the existing Shield, Health, Poise, Guard, Deflection, channel mitigation, accepted damage results and Echo producers. Definitions cannot inject fatal or immunity-bypass policy tags into ordinary melee classifications.
- Charged release and defensive cancel use the existing `UNarrativeCombatAbility` exertion API. A full release pays once, an early release uses the uncharged scalar, and an exhausted release fails cleanly. Cancel first validates the granted evade's combined cost and state. A buffered press already released by the player immediately releases a following charge node.
- Follow-up and defense inputs use the ASC's explicit combat window and freshness policy. Aim assistance uses the saved melee setting, live hostile candidates, distance and LOS, and a maximum 25-degree authored correction. Zero assistance produces no correction.
- Weapon replacement, return/recall state, death, sequencer ownership and avatar replacement invalidate the action. Cancellation removes this action's task, montage and input window. Damage, montage and payment callbacks are fenced against a renewed attack on the same ability instance.

Author the existing weapon visual's start/end socket names and choose an attack definition on a granted `USovGameplayAbility_Melee` subclass. Optional montages retain their existing root motion. `OnMeleeNodeStarted`, `OnMeleeImpact` and `OnMeleeEnvironmentContact` provide trails, camera, hit-stop, controller feedback and sound hooks. Assets, montages and Enhanced Input binding remain editor work. No alternate combat component or input system is required.

## Validation and remaining acceptance

Executed: all 23 portable suites via `python3 Scripts/Test-NativePolicies.py`, C++17, warnings as errors and undefined-behavior sanitizer. Melee's production window, finite branch, spatial/temporal sample and assistance policies pass 26 assertions.

Authored Unreal automation covers fast socket crossing, exact Shield damage, one hit per node, renewed attack identity, finite input branching, stale task isolation, invalid definitions and animated blade movement beyond blocking cover. These tests were not run because Unreal is unavailable in this environment.

Required engine acceptance: UE5.7/UHT compile; `ProjectVelkorran.Campaign.Melee` automation; both protagonist weapon sockets and montages at 30/60/120 fps; released/held buffered charge branches; insufficient stamina; evade admission and cancellation; ability replacement inside damage callbacks; friendly bodies and solid cover; safe root motion against level geometry. Collision samples cannot reconstruct an intermediate animation pose that was never evaluated, so authored fast-rotation montages need this frame-rate acceptance pass.

Definition of done for native source: attack lifecycle, input/payment ownership, actual damage and collision, feedback seams and tests are present. Build/automation and content acceptance remain explicit gates, not claimed results.
