# Remaining native combat engineering

This pass addresses TDD v2 §§6.4, 6.7, 6.9 and 6.11 on top of the campaign engineering branch. It preserves Narrative's ASC, damage execution/attribute routing, projectile actor, owned GameplayEffects, and the existing weapon-specific abilities. Melee socket tracing and input buffering are documented separately by the shared combat integration work.

## Findings and disposition

| Gap | Existing implementation | Requirement and correction | Dependencies, risk and order |
| --- | --- | --- | --- |
| Mixed channel immunity | A mixed packet dealt full damage whenever any declared channel was nonimmune; conditional mitigation evaluated against all channels together. | Extend the current execution. Each declared channel has an equal default portion, optionally weighted using that channel tag's SetByCaller magnitude. Immune/zero-weight portions disappear; each surviving portion evaluates conditional AttackDamage, AttackRating, Armor and DamageResistance independently. Preserve single-channel and untagged behavior. | Existing damage tags and execution are retained. Balance risk for mixed attacks is intentional: an immune half no longer deals damage through the other half. Integrate before tuning enemy resistances. |
| Damage result provenance | Consumers saw only declared channel tags. | Extend `FSovDamageResult` with `RejectedDamageChannels` and `AcceptedChannelFraction`; status producers must exclude their rejected semantic channel. Explicit Poise receives the same fraction exactly once. | Corruption production now reads this evidence. No random elemental weakness system is introduced. |
| Hard control during protected actions | Poise recovery/super armor prevented re-break; native Selene freeze had a refreeze immunity GE. Finisher protection was missing. | Preserve these systems. Add a finite owned interruption-protection tag, a Poise floor during protection, central requested-status immunity filtering, and downgrade Selene freeze to chill during recovery, super armor or protected actions. Already frozen actors cannot have hard-freeze duration repeatedly refreshed. | This does not grant Health/Shield invulnerability. Device and equipment disruption remain independently authored; Blueprint status consumers must honor the accepted result and GAS immunity policy. |
| Finishers | No native execution action existed in available source. NarrativeCombatAbility already supplied GAS lifecycle and attack identity. | Extend NarrativeCombatAbility with `USovGameplayAbility_Finisher` and explicit `USovFinisherTargetComponent` eligibility. Reserve one actual hostile vulnerable target, own only finite action/protection effects, validate range/LOS/facing/height/nav/exit clearance, bound action duration, strike once, and release every owned state on completion/cancellation. No alignment or missing animation uses a short native damage strike. | Targets explicitly opt in, preventing accidental generic executions of canon actors. Input tag and ability grant use the existing GAS/input setup. Native behavior is testable without animation assets. Actor/ASC replacement, callback cancellation, blocked geometry and death are main acceptance cases. |
| Elite and boss outcomes | No native phase gate existed. | A non-normal target requires an active authored phase tag. A valid aligned sequence records each phase once in Narrative's component save record and emits `Sov.Event.Finisher.PhaseResolved`. Elite/boss strikes leave at least one Health. Missing alignment never consumes the cinematic phase outcome. A boss-tagged actor cannot use normal-enemy automatic death even if its component is misconfigured as Normal. | Mission/boss abilities implement the authored response to the phase event. Saved phase facts restore without replay. Validate one configured boss phase after ordinary finisher integration. |
| Physical projectile defense | The drone rocket immediately exploded on impact; damage negation did not change its flight or protect the surrounding lane. | Extend the existing physical rocket. Resolve its real direct damage transaction first. An exact successful Deflection receipt transfers source ownership and reverses the actual moving collision body toward the old shooter; Perfect Guard dissipates it without the radial payload. Normal impacts exclude the already processed direct target from radial damage. | Reuses actual stamina/arc/attack-class defense, so unblockable attacks do not become reflectable. Defaults allow one reflection; subsequent successful deflection dissipates. Source ownership/faction, movement-component restart and duplicate hit callbacks need PIE validation. |

## Native contracts and integration

Damage channel tags themselves are the optional weights. For example, add `Sov.Damage.Channel.Kinetic` and `Sov.Damage.Channel.Thermal` as asset tags, then assign magnitudes 3 and 1 under those same SetByCaller tags for a 75/25 packet. Omitting magnitudes gives equal weights. Negative, nonfinite or all-zero weights reject the packet. Untagged packets retain the previous generic behavior. `Damage.AlreadyResolved` still avoids repeating mitigation but respects channel immunity; the existing explicit fatal policy retains its authored bypass semantics.

`GetIncomingDamageScale()` from the existing Narrative user settings extension applies only to hostile body damage received by a locally controlled player. It runs after independent Poise calculation and excludes explicit fatal policy. It changes no max Health or max Poise attributes.

Grant `USovGameplayAbility_Finisher` through the existing protagonist ability definition and bind `Sov.Input.Finisher`. Add `USovFinisherTargetComponent` only to eligible enemy definitions. The default vulnerability is Poise Broken or at/below 20% Health. NPCs lacking the component remain ineligible. Elites and bosses need both a live configured phase tag and an earned vulnerability.

A finisher acquires its target lease before cost/delegate callbacks. Source identity, life, exact ASC avatar and target reservation are rechecked after callback boundaries and every 25 ms. Finite GameplayEffects provide movement lock and interruption protection; their exact handles are removed during cleanup. Only active `UNarrativeCombatAbility` actions are canceled on the target, preserving unrelated passives. Other enemies continue applying Health/Shield damage and can kill the player. No world slow motion, global invulnerability, permanent input lock or free healing loop is added.

The optional montage uses a bounded 0.8–1.8 second rate. Its `Sov.Event.Finisher.Strike` event can deliver the single strike, with native timed delivery as a fallback and 0.2 seconds of post-strike recovery. Missing assets/nav or unsafe alignment use a 0.35 second non-cinematic action with strike at 0.15 seconds. The montage must be authored for the validated local position; this code never teleports either participant through a hazard. Gore remains with the existing dismemberment/presentation pipeline.

The projectile publishes `bAbsorbed` on its existing replicated resolution structure. Dissipation presentation is reused. Reflected flight updates the existing replicated velocity, disables homing and restarts the movement component on the next tick, because UE's impact callback may clear its UpdatedComponent after the collision delegate returns. The projectile keeps its bounded launch lifetime and is never duplicated.

## Validation

Executed locally: `SovRemainingCombatPolicyTests.cpp`, compiled as C++17 with warnings as errors and undefined-behavior sanitization, **29 assertions passed**. It uses the production weighted-channel, finisher eligibility/damage/duration, and projectile-defense policies.

Added Unreal automation tests, not executed here:

- `ProjectVelkorran.Campaign.Defense.WeightedChannels`: equal and explicit weights, partial immunity, Poise allocation and complete rejection.
- `ProjectVelkorran.Campaign.Defense.InterruptionProtection`: body damage remains dangerous while Poise break and hard freeze are prevented; control returns after release.
- `ProjectVelkorran.Campaign.Finisher.FallbackAndOwnership`: earned target reservation, cancellation, preservation of another Busy owner, elite phase gate, bounded nonlethal fallback and no premature phase consumption.
- `ProjectVelkorran.Campaign.Projectile.PhysicalReflection`: actual direct defense receipt, projectile source transfer, later impact and one direct/radial application.
- `ProjectVelkorran.Campaign.Projectile.PerfectGuardAbsorption`: final dissipation without area damage.

Required UE5.7 acceptance: UHT and full editor target compile, these automation tests, then an authored finisher montage at ordinary/low frame rates, blocked/no-nav/unsafe-exit scenarios, repeated input/cancellation/death/avatar replacement, a saved elite phase, direct versus splash guard/deflection, reflected rocket collision and replicated presentation. The local environment lacks Unreal and content assets, so none of these engine results are claimed.

## Definition of done

Native policy and lifecycle implementation is present and portable policies pass. Engine acceptance requires the full build and automation above. Content acceptance requires an ordinary enemy, an elite phase, a finisher montage and the existing rocket art configured in the editor. Generic status families continue to use authored GAS effects and the existing weak-point/control producers; this pass does not substitute an additional parallel status manager.

## Integration review corrections

The native melee loop is described in `Docs/MeleeEngineering.md`. The final review also fixed global immunity for untagged legacy packets, exact finisher lease fencing across protection-effect and montage callbacks, and snapshotting the resolved phase tag before damage delegates can change live state. A cancelled protection application cannot leak its returned effect handle or end a replacement activation.

Reflected rockets now retain launch expiry if that one-shot timer fires while movement restart is deferred. Rocket payload validation rejects nonfinite geometry/timing, and splash damage rejects a replaced source ASC avatar both before and during its target loop. Companion damage budgets and corruption status-duration producers continue through their existing accepted-result contracts.

Additional authored UE tests, not run here: `Finisher.ReentrantProtectionOwnership`, `Projectile.ReflectionRetainsLaunchExpiry`, and untagged global immunity assertions in `Defense.WeightedChannels`. The full portable runner passed 23 suites after these changes; these policy results do not substitute for UE5.7/UHT or gameplay automation.
