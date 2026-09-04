# Selene Echo Ability Kit

Selene's kit uses the same replicated 0–100 Echo resource and shared native transaction as Tarrik, but its identity is precision control rather than Cinder pressure. Each action has a separate `MinimumEchoRequired` and `EchoCost`; the defaults below interpret the requested thresholds as full spend.

## Final set

| Input | Ability | Weapon context | Minimum Echo | Default spend | Combat job |
|---|---|---|---:|---:|---|
| `Narrative.Input.Ability1` | Stillpoint Grenade | Any Selene weapon | 35 | 35 | Delayed area lockdown |
| `Narrative.Input.Ability2` | Verity's Wake | Verity | 30 | 30 | Immediate lane pressure and frost primer |
| `Narrative.Input.Ability2` | Staccato Zero | Staccato | 30 | 30 | Single-target precision execution |
| `Narrative.Input.Ability2` | Axiom Null Pulse | Axiom | 30 | 30 | Anti-Shield and system disruption |
| `Narrative.Input.Ability3` | Dispatch | Selene signature / Verity | 90 | 90 | Steerable double-pass spatial signature |

The Stasis Grenade was refined into **Stillpoint Grenade**, Frost Wave into **Verity's Wake**, and Staccato Overdrive into **Staccato Zero**. The mechanical split is deliberate:

- Stillpoint is delayed, localized, reliable crowd control;
- Wake is fast forward pressure and rewards lane accuracy;
- Zero is one high-value precision shot with no area damage;
- Null Pulse attacks Shields and systems instead of Health;
- Dispatch is a high-commitment trajectory skill with outbound and return paths.

## Native parents

Create these Blueprint children:

- `GA_Selene_StillpointGrenade` from `USovGameplayAbility_SeleneStillpointGrenade`
- `GA_Selene_Dispatch` from `USovGameplayAbility_SeleneDispatch`
- `GA_Selene_StaccatoZero` from `USovGameplayAbility_SeleneStaccatoZero`
- `GA_Selene_AxiomNullPulse` from `USovGameplayAbility_SeleneAxiomNullPulse`
- `GA_Selene_VeritysWake` from `USovGameplayAbility_SeleneVeritysWake`

The shared `USovGameplayAbility_EchoBase` now owns predicted activation, server-only Echo spending and termination, cancellation, weapon validation, server targeting, animation hooks, and the stuck-ability failsafe for both protagonists. Tarrik's existing native parent path remains intact through a thin compatibility adapter.

Do not add an Echo Cost Gameplay Effect and do not implement the generic Blueprint `Event ActivateAbility`. Use:

- `Echo Ability Started` for predicted/authority montage and task setup;
- `Echo Ability Local Presentation` for owner-only camera, audio, rumble, and cosmetic hit-stop;
- `Echo Ability Authority Committed` as the only point that may spawn gameplay actors or apply gameplay effects;
- `Echo Ability Ended` for presentation cleanup;
- `Finish Echo Ability` when the authored flow is complete or interrupted.

Every Selene parent fails activation when its required payload classes are unassigned, preventing a paid no-op. Treat every non-optional slot in the sections below as required before testing. The current native parents still validate their legacy Chill/Freeze effect-class slots as compatibility scaffolds; keep those slots assigned until each ability payload is migrated to submit centralized status requests. Do not use those compatibility effects as a second status-application path. Optional accents such as Stillpoint detonation damage, Dispatch's Frozen shatter, Axiom's device disable, and their presentation assets may remain empty.

## Identity and granting

Selene's player Blueprint must derive from `ASovSeleneCharacter`. That concrete class merges `Sov.Character.Player.Selene` into the definition-owned ASC tag contribution and removes a conflicting Tarrik identity. Keep the same Selene tag on her Player Definition so the asset remains self-describing. Tarrik should likewise derive from `ASovTarrikCharacter`. Legacy Blueprints still deriving directly from `ASovPlayerCharacterBase` retain the temporary untagged migration fallback.

Grant Stillpoint Grenade and Dispatch once through Selene's default `UAbilityConfiguration`:

- Stillpoint's `AllowedWeaponClasses` must include the Verity, Staccato, and Axiom item classes. It accepts any currently wielded allowed class.
- Dispatch deliberately requires no currently wielded source weapon. It is Selene's signature and may summon/throw Verity while a firearm presentation is active.

Grant only the matching Ability2 variant from each weapon asset:

- Verity grants Verity's Wake;
- Staccato grants Staccato Zero;
- Axiom grants Axiom Null Pulse.

Set each Blueprint child's `AllowedWeaponClasses` to its exact weapon item class. These variants fail closed when the array is empty and verify that the granting source is still wielded. Narrative activates every spec sharing the pressed input tag, so do not grant two Ability2 variants at once.

## Animation keys

Author paired `UNarrativeAnimSet` entries in the active linked weapon layers:

- `Narrative.Anim.AnimSets.Ability.Selene.StillpointGrenade`
- `Narrative.Anim.AnimSets.Ability.Selene.Dispatch`
- `Narrative.Anim.AnimSets.Ability.Selene.StaccatoZero`
- `Narrative.Anim.AnimSets.Ability.Selene.AxiomNullPulse`
- `Narrative.Anim.AnimSets.Ability.Selene.VeritysWake`

Resolve through `Get Anim Set` with `Search Linked Layers = true`. Gameplay-significant projectiles, traces, overlaps, status rolls, and effects stay authority-owned; 1P and camera polish stays local.

## Status effects required

The centralized `USovStatusComponent` consumes these native contracts:

- `Sov.Status.Apply.Chill`
- `Sov.Status.Apply.Freeze`
- `Sov.Status.Apply.DeviceDisabled`
- `Sov.State.Status.Chilled`
- `Sov.State.Status.Frozen`
- `Sov.State.Status.DeviceDisabled`
- `Sov.Status.Immunity.Freeze`
- `Sov.Status.Immunity.DeviceDisable`

For status coupled to damage, add the exact `Sov.Status.Apply.*` tag and `Sov.SetByCaller.Status.Magnitude` / `Sov.SetByCaller.Status.Duration` to the same authoritative damage spec. The unified damage transaction submits a typed request only after positive, nonfatal Health, Shield, or Poise damage. For a non-damaging payload, call `Apply Status By Tag` on the target's status component from authority. Do not directly apply a second duration GE in the ability Blueprint.

Native definitions provide functional Burn, Chill, Freeze, Device Disabled, and Exposed behavior. Optional `USovStatusDefinition` assets can replace them by exact request tag. Relevant rules are:

- Chill lasts 4 seconds, adds up to three stacks, refreshes on a valid added stack, and grants the prototype movement-slow constraint.
- Freeze lasts 1.25 seconds, grants Busy, Movement Lock, and Weapon Block Firing, stops current controller movement, and applies 1.5 seconds of Freeze immunity after removal.
- `GE_Damage_FrostDOT`: periodic damage through `UNarrativeDamageExecCalc`, never a direct Health or Damage-meta modifier.
- Device Disabled lasts 4 seconds and succeeds only on a status component explicitly marked device-eligible. `ASovDroneNPCBase` opts in by default.
- `GE_Status_ShieldRechargeBlocked`: duration effect granting `Sov.State.Shield.RechargeBlocked`.

CC-resistant elites and bosses should reject hard Freeze through immunity tags and receive the authored Chill/exposure fallback. Branch presentation from the returned status result or `On Status Changed`; do not play a Frozen pose when the authoritative target rejected the request. See `Docs/StatusAndCorruptionPrototype.md` for definitions, cleanses, checkpoint semantics, and PIE coverage.

## Payload contracts

### Stillpoint Grenade

- Spawn one replicated grenade on authority. Prediction may spawn a separate non-gameplay throw/trail cosmetic.
- Authority owns the fuse, overlap, faction filter, target deduplication, and refreeze lockout.
- Standard enemies in the detonation field receive Freeze plus Frost DOT. Freeze-immune targets receive Chill plus the authored reduced DOT.
- Author the Frozen DOT so it requires `Sov.State.Status.Frozen` and ends or inhibits when Freeze ends. Use the separate resistant-target DOT slot for the reduced fallback; do not reuse the Frozen-only effect.
- Treat `StasisDuration` as the target duration for Freeze and its DOT, then copy that value into the projectile/effect payload instead of maintaining unrelated timers.
- The field is the reliable area lockdown tool; it should not also become Selene's highest burst-damage option.
- The ability may end once the initialized authority grenade exists. Copy all ASC/effect/tuning data into the grenade and never retain the Gameplay Ability instance.

### Verity's Wake

- Spawn a server-owned advancing wave or run an authoritative swept volume with one hit per actor.
- Apply immediate Echo/Thermal damage, Chill, and Frost DOT.
- Replace invisible random chance with an authorable deterministic rule: targets struck near the centerline or already carrying `Sov.State.Status.Chilled` receive Freeze when not immune.
- Wake is fast lane pressure, not Stillpoint's persistent guaranteed area lock.

### Staccato Zero

- Make this one dedicated empowered shot, not a loose timed buff consumed by a separate ordinary-fire ability.
- Use `Get Authority Aim Target Data` or an equivalent server trace. Do not trust Narrative's current client target-data path for a paid precision payload.
- Apply the bonus damage and Freeze only to the confirmed target. Freeze-immune targets receive maximum Chill/exposure instead.
- Pass `DamageMultiplier` through `Sov.SetByCaller.Damage.AbilityScalar` on the empowered damage spec. Do not also bake the same multiplier into the Gameplay Effect magnitude.
- Keep it single target, with no Frost DOT or explosion, so it remains the clean execution tool.

### Axiom Null Pulse

- Clamp charge alpha on authority and interpolate the authored minimum/maximum pulse range, half-angle, and suppression duration. Charge never accepts an unvalidated client damage value.
- On authority, send a shield-only Disruption packet through the normal SetByCaller damage Gameplay Effect:
  - `Sov.Damage.Channel.Disruption`
  - `Sov.Damage.Policy.AlreadyResolved` when submitting an exact Shield-collapse magnitude
  - `Sov.Damage.BypassGuard` if the EMP is not meant to be intercepted by a physical guard plane
  - Shield coefficient `1`
  - Health coefficient `0`
  - Poise damage `0` unless separately authored
- Then apply the duration recharge-block effect. `Sov.State.Shield.RechargeBlocked` only pauses recharge; the Disruption packet is what removes existing Shield while preserving Shield-break events, material state, and Niagara.
- Apply Device Disabled only to eligible targets that do not carry `Sov.Status.Immunity.DeviceDisable`.
- Exact full-Shield collapse for 30 Echo is intentionally aggressive. Bosses or shield-critical elites should carry an authored EMP immunity/cap policy if they must retain phase mechanics.
- The command-link payload is separate. Add `USovCommandLinkComponent` to the actual hostile command node, give it a stable `LinkId`, and assign/register its encounter participants. Do not award Sever from the Shield or Device Disabled result.
- Inside the existing `Echo Ability Authority Committed` pulse target loop, call `Try Sever Axiom Command Link` once for each unique command-node actor selected by the authoritative pulse. Do not call it from local presentation, an animation notify, a Gameplay Cue, or client-authored target data.
- The helper revalidates active ability state, authority, world, maximum Axiom range, and component presence. The component then validates hostility against the link's actual participants. Only `NewlySevered` changes link state. The helper automatically submits that transaction to Selene's Echo generator; Blueprint must not add the `+12` itself.
- `Inactive`, `NotHostile`, `Immune`, `AlreadySevered`, and `Invalid` are deliberate no-award outcomes. A repeated call returns the existing Sever identity and cannot restart the reveal or repay Echo.
- A successful Sever removes the component's `Sov.State.CommandLink.Active` and optional active-link Gameplay Effect contributions, adds `Sov.State.CommandLink.Severed`, interrupts watched specialist actions, and begins the configured weak-point reveal on affected actors.

This implementation is a Shield collapse plus timed suppression. If the design later needs an intact but temporarily bypassed Shield, add a separate routing contract rather than pretending RechargeBlocked disables it.

Full command-node and red decal authoring instructions are in `Docs/SeleneCommandLinkAndWeakPointReveal.md`.

### Dispatch

**Required content:** `ReturningVerityClass` must be a project-owned replicated actor derived from `ANarrativeProjectile` that implements the state machine, steering validation, hit ledgers, recall, forced return, and cleanup below. The native Gameplay Ability supplies the paid lifecycle contract but does not fabricate that actor behavior.

- Spawn one authority-owned replicated Verity actor with explicit `Outbound`, `Recalling`, and `Returned/Expired` states.
- The server accepts control rotation/aim intent, never client projectile positions. Clamp steering rate, speed, range, and lifetime.
- In the active ability, use GAS `Wait Input Press` for the second press. That request recalls the existing Verity actor and never spends Echo again.
- Auto-recall at the outbound timer/range limit. Death, interruption, owner loss, or timeout must also force return/cleanup.
- Maintain separate server hit ledgers so each actor can be damaged at most once outbound and once inbound.
- A return pass through a Frozen or Chilled target may consume/shatter that state for bonus Poise pressure, but must not duplicate the base leg hit.
- Keep the ability active until Verity returns or the failsafe resolves. The native Dispatch parent raises the general maximum duration to seven seconds.

Narrative's stock projectile task is not suitable for Dispatch's authoritative state machine and can create unreconciled predicted/authority copies for replicated projectiles. Use a project-owned returning-weapon actor.

## Minimum PIE matrix

- Selene identity tag present/absent; Tarrik cannot activate Selene classes and vice versa
- each Ability2 variant granted only by its active weapon source
- Stillpoint works with all three authored Selene weapon classes and no others
- authority Echo spend occurs once under prediction and lag
- Stillpoint standard, Freeze-immune, friendly, dead, and invulnerable targets
- Wake centerline, edge, already-Chilled, and repeated-wave target cases
- Staccato Zero weak-point/ordinary miss, immune fallback, and server/client aim divergence
- Axiom target with full, partial, zero, and no MaxShield; no Health overflow at any charge
- Axiom recharge remains blocked for the authored duration, then resumes normally
- Axiom first active hostile link Sever returns `NewlySevered`, grants exactly `+12`, and exposes only unbroken authored weak points
- Axiom repeated, inactive, immune, non-hostile, missing-link, and out-of-range Sever attempts grant no Echo or reveal
- Axiom spends `30` before the eligible Sever reward: verify `100 -> 82` and `30 -> 12`, with no replay after later meter changes
- Dispatch early recall, timed recall, range recall, owner death, interruption, obstruction, and owner disconnect
- Dispatch one hit per actor per leg, including repeated overlaps at low speed
- first-person owner and third-person simulated-proxy montage/cue presentation

These native classes are intentionally abstract scaffolds. Blueprint children, projectiles, effects, AnimSets, Gameplay Cues, Niagara, audio, reveal decal materials, command-node membership, and final balance must still be authored in the Unreal project. Physical projectile reflection/retargeting remains a separate deferred system; this Sever integration does not change projectile ownership or trajectory.
