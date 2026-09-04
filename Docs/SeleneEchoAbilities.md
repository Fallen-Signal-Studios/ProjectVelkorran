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
- `Echo Ability Authority Committed` for authority-owned payload work in Stillpoint, Dispatch, Staccato Zero, and Verity's Wake; Axiom already owns its payload natively, so its Blueprint hook is presentation-only;
- `Echo Ability Ended` for presentation cleanup;
- `Finish Echo Ability` when a Blueprint-owned payload flow is complete or interrupted. Axiom owns its charge, release, recovery, and normal end; do not finish it from a charge-start montage or its authority-committed hook.

Stillpoint, Dispatch, Staccato Zero, and Verity's Wake remain payload scaffolds: assign their required classes and implement the authoritative projectile/effect behavior below before testing. Their configuration checks reject missing required classes but do not implement those payloads. Optional accents such as Stillpoint detonation damage, Dispatch's Frozen shatter, and presentation assets may remain empty. Axiom now supplies its complete native pulse and safe damage/suppression/device-disable Gameplay Effect defaults; follow [AxiomNullPulse.md](AxiomNullPulse.md) to migrate its Blueprint to presentation only.

## Identity and granting

Selene's player Blueprint must derive from `ASovSeleneCharacter`. That concrete class merges `Sov.Character.Player.Selene` into the definition-owned ASC tag contribution and removes a conflicting Tarrik identity. Keep the same Selene tag on her Player Definition so the asset remains self-describing. Tarrik should likewise derive from `ASovTarrikCharacter`. Other Selene payload scaffolds retain the temporary untagged migration fallback for legacy Blueprints derived directly from `ASovPlayerCharacterBase`. Axiom fails closed without the explicit Selene identity and a valid currently wielded granting weapon; migrate its player Blueprint before testing.

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

The source publishes these native contracts:

- `Sov.Status.Apply.Chill`
- `Sov.Status.Apply.Freeze`
- `Sov.Status.Apply.DeviceDisabled`
- `Sov.State.Status.Chilled`
- `Sov.State.Status.Frozen`
- `Sov.State.Status.DeviceDisabled`
- `Sov.Status.Immunity.Freeze`
- `Sov.Status.Immunity.DeviceDisable`

There is not yet a tracked status manager that consumes `Sov.Event.Status.ApplicationRequested`. Stillpoint, Dispatch, Staccato Zero, and Verity's Wake must apply their configured duration Gameplay Effects from their authority-owned payload after faction, life-state, invulnerability, and immunity validation. Axiom applies its own bounded native suppression and device-disable effects; do not duplicate them in Blueprint.

Recommended authored effects for the remaining payloads (Axiom uses its native defaults):

- `GE_Status_Chilled`: duration effect granting `Sov.State.Status.Chilled`, with the tuned slow/control-vulnerability policy.
- `GE_Status_Frozen`: hard-CC duration effect granting `Sov.State.Status.Frozen` and the appropriate Narrative movement/action lock tags.
- `GE_Status_FreezeImmunity`: short post-thaw immunity/refreeze lockout for targets that should not be chain-frozen.
- `GE_Damage_FrostDOT`: periodic damage through `UNarrativeDamageExecCalc`, never a direct Health or Damage-meta modifier.
- `GE_Status_DeviceDisabled`: duration effect granting `Sov.State.Status.DeviceDisabled` only to eligible targets.
- `GE_Status_ShieldRechargeBlocked`: duration effect granting `Sov.State.Shield.RechargeBlocked`.

CC-resistant elites and bosses should reject hard Freeze through immunity tags and receive the authored Chill/exposure fallback. Do not play a Frozen pose when the authoritative target rejected the effect.

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

Axiom has a complete native gameplay payload. See [AxiomNullPulse.md](AxiomNullPulse.md) for the setup, migration, tuning, and validation details.

- Grant `GA_Selene_AxiomNullPulse` only from the actual Axiom `UWeaponItem`, and set `AllowedWeaponClasses` to that item class. Native activation and release require Selene's identity, the original granting weapon still wielded, and no weapon transition.
- Native code spends Echo once, starts its authoritative charge clock, releases on input release or the `0.65`-second full-charge timer, and ends after native recovery. It interpolates range, cone half-angle, and suppression duration from that clock. No client supplies target or charge magnitude.
- The release deduplicates candidates and applies charge-derived range/cone and visibility checks. The target and source are revalidated as the transaction progresses.
- Exact Shield collapse uses the existing Narrative damage execution with Disruption, guard/deflection bypass, Shield coefficient `1`, Health coefficient `0`, and zero Poise. Native duration effects pause Shield recharge and expire normally; this preserves Shield-break feedback without Health overflow.
- Device Disabled defaults to eligible `ASovDroneNPCBase` targets and explicitly opted-in additional classes. Immunity and boss policy still apply. Organic Handlers and Hounds are not globally disabled; their command-dependent action stops through genuine link Sever while independent Bite/Pounce remain available.
- Native safe Gameplay Effect shells are supplied. Legacy Blueprint effect overrides are retained as serialized references but safely replaced by native shells during application. Reset those fields to the native defaults when migrating; leaving an old slot empty does not disable the native payload.
- Author `USovCommandLinkComponent` on the actual hostile command node, with its stable `LinkId` and encounter participants. Native release authorizes each eligible node and calls `TrySeverAxiomCommandLink` internally. Direct Blueprint calls to that helper now return `Invalid`; do not build a second target loop or call Sever from `Echo Ability Authority Committed`.
- Only a new valid link transaction awards the existing `+12` Echo and reveals weak points. Shield collapse, Device Disabled, link deactivation, and repeated attempts cannot substitute for Sever or repay a consumed transaction.
- Use `Echo Ability Started`, local presentation, `Receive Axiom Pulse Released`, and `Echo Ability Ended` for animation/audio/VFX/UI only. The release-result event runs on authority after gameplay; route observer cosmetics through the existing replicated presentation mechanisms when needed. Remove old Blueprint charge timers, input-release tasks, damage/status applications, direct Sever/Echo writes, and normal-end calls.

Exact Shield collapse for 30 Echo is an aggressive prototype. Author immunity on phase-critical targets when needed; any future capped-collapse behavior needs its own explicit native policy. RechargeBlocked pauses regeneration and does not make an intact Shield bypassable.

Command-node and red decal authoring instructions are in [SeleneCommandLinkAndWeakPointReveal.md](SeleneCommandLinkAndWeakPointReveal.md).

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
- Axiom repeated, inactive, immune, non-hostile, missing-link, outside-charge-cone/range, and occluded targets grant no Sever Echo or reveal
- Direct Blueprint Sever calls fail; input release plus full-charge timer releases at most one pulse
- Death, cinematic interruption, or weapon replacement during charge prevents a late payload
- Axiom spends `30` before the eligible Sever reward: verify `100 -> 82` and `30 -> 12`, with no replay after later meter changes
- Dispatch early recall, timed recall, range recall, owner death, interruption, obstruction, and owner disconnect
- Dispatch one hit per actor per leg, including repeated overlaps at low speed
- first-person owner and third-person simulated-proxy montage/cue presentation

Stillpoint, Dispatch, Staccato Zero, and Verity's Wake remain native payload scaffolds. Axiom supplies native gameplay, but its Blueprint grant, exact weapon allowlist, AnimSets, Gameplay Cues, Niagara, audio, reveal decal materials, command-node membership, and final balance still need Unreal content setup. The other four abilities additionally require their authored projectile/effect payloads. Physical projectile reflection/retargeting remains a separate deferred system; this Sever integration does not change projectile ownership or trajectory.
