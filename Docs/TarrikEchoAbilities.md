# Tarrik Echo Ability Kit

This kit replaces the older placeholder Tarrik actives for the current weapon-context prototype. Echo remains the replicated 0–100 resource owned by `USovEchoComponent`.

## Loadout

| Input | Velkorran | Cinderline | Minimum Echo | Default Spend |
|---|---|---|---:|---:|
| `Narrative.Input.Ability1` | Cinder Sticky Grenade | Cinder Sticky Grenade | 35 | 35 |
| `Narrative.Input.Ability2` | Velkorran's Hunger | Cinder Judgement | 50 | 50 |
| `Narrative.Input.Ability3` | Cinder Slam | Cinderline Requiem | 90 | 90 |

`MinimumEchoRequired` and `EchoCost` are intentionally separate on every class. The defaults above interpret the requested values as both readiness and full spend. If the 90-point actions should behave as Resonant techniques rather than near-signatures, retain the 90-point readiness gate and lower their spend independently. The v2 TDD otherwise reserves the full 100-point meter for Tarrik's signature release.

## Native parents

Create these Blueprint children:

- `GA_Tarrik_CinderStickyGrenade` from `USovGameplayAbility_TarrikCinderStickyGrenade`
- `GA_Tarrik_VelkorransHunger` from `USovGameplayAbility_TarrikVelkorransHunger`
- `GA_Tarrik_CinderSlam` from `USovGameplayAbility_TarrikCinderSlam`
- `GA_Tarrik_CinderJudgement` from `USovGameplayAbility_TarrikCinderJudgement`
- `GA_Tarrik_CinderlineRequiem` from `USovGameplayAbility_TarrikCinderlineRequiem`

The shared `USovGameplayAbility_EchoBase`, reached through Tarrik's compatibility parent, provides:

- Local Predicted GAS activation;
- server-owned termination, so a client cannot clear the committed Busy/Echo-action state before authority finishes or cancels the payload;
- replicated Echo threshold and affordability checks;
- authority-only, atomic Echo spending through `USovEchoComponent`;
- one active Echo action at a time, with Narrative's Busy state owned for the committed cast so ordinary combat cannot stomp its montage;
- common death, guard, guard-break, Poise-break, busy, interaction, sequencer, and ragdoll activation blocks, with active cancellation when terminal/interrupt states arrive during the cast;
- fail-closed `AllowedWeaponClasses` validation for weapon-specific abilities, with the character-granted Sticky Grenade exempt as a universal action;
- display text, weapon family, animation-set key, and payload asset/tuning slots;
- fail-closed native payload validation, so an incomplete child cannot commit or spend Echo;
- `Echo Ability Started`, `Echo Ability Authority Committed`, `Echo Ability Local Presentation`, and `Echo Ability Ended` Blueprint hooks;
- `Get Authority Aim Target Data` for server-owned hitscan validation;
- a five-second stuck-ability failsafe, adjustable per child;
- `Finish Echo Ability` for montage/task completion.

Do not author a second Echo Cost Gameplay Effect on these abilities. A normal GAS cooldown or a separate Stamina/ammo cost may still be used; Echo itself is already committed by the native base.

Do not implement the generic `Event ActivateAbility` in these children. The native parent owns activation and commits Echo before it calls the custom hooks. Use `Echo Ability Started` for the predicted/authority montage and task setup, and use `Echo Ability Authority Committed` as the only authorization point for server gameplay. Do not execute the same payload from both hooks.

## Granting

Add `Sov.Character.Player.Tarrik` to Tarrik's Player Definition/default owned tags. During migration the common base permits an avatar with no player-identity tag, preserving existing content; once any `Sov.Character.Player` identity is present, the Tarrik adapter rejects Selene and other mismatches.

Grant the Sticky Grenade once through Tarrik's default `UAbilityConfiguration`. Do not add it to both weapon assets or dual-wield/source-object variants can create duplicate specs. It is character-granted and works with either Tarrik weapon, so it deliberately ignores `AllowedWeaponClasses`.

Add the sword pair to Velkorran's `WeaponAbilities` and the rifle pair to Cinderline's `WeaponAbilities`. Set each sword Blueprint child's `AllowedWeaponClasses` array to the Velkorran item class and each rifle child's array to the Cinderline item class. Those weapon-specific abilities intentionally fail closed when that array is empty.

Narrative activates every granted spec whose input tag matches. Never grant both weapon-context variants of Ability2 or Ability3 at the same time.

The native Tarrik Guard parent now blocks while Narrative Busy or an Echo ability is active. Echo abilities already block while Guarding, so the two committed lanes cannot enter simultaneously and fight over their montages/state.

## Animation flow

Each parent exposes a native animation key:

- `Narrative.Anim.AnimSets.Ability.Tarrik.CinderStickyGrenade`
- `Narrative.Anim.AnimSets.Ability.Tarrik.VelkorransHunger`
- `Narrative.Anim.AnimSets.Ability.Tarrik.CinderSlam`
- `Narrative.Anim.AnimSets.Ability.Tarrik.CinderJudgement`
- `Narrative.Anim.AnimSets.Ability.Tarrik.CinderlineRequiem`

These keys are intentionally `VisibleDefaultsOnly`: they are stable ability identities, not montage asset slots. Do not recreate or edit the tag in the child. Read it with `Get Echo Ability Anim Set Tag`.

Add the matching `UNarrativeAnimSet` to the active weapon linked layer's `Tagged Anim Sets`. Resolve it with `Get Anim Set` and `Search Linked Layers = true`, then play its paired 1P/3P montages through the same Narrative flow used by attacks.

In `Echo Ability Started`:

1. start the cast montage and its wait task;
2. use a montage notify/gameplay event for the release frame;
3. use `Echo Ability Authority Committed` as the server authorization/arming signal, then invoke the payload's authority-only release function at the release frame;
4. call `Finish Echo Ability(false)` on completed/blend-out and `Finish Echo Ability(true)` on interruption.

Use `Echo Ability Local Presentation` only for owning-player camera, rumble, audio accents, and cosmetic hit-stop. Do not apply damage, spawn replicated actors, or change global time dilation there.

## Payload contracts

### Cinder Sticky Grenade

- Native defaults provide `Sovereign Cinder Grenade Explosion Damage`, `Sovereign Cinder Grenade Burn`, and a functional native projectile fallback. Existing GA Blueprint children that serialized the old empty values now resolve those native classes automatically. Blueprint subclasses remain optional if you want effect-specific Gameplay Cues, a visible projectile mesh, or data-only tuning.
- Create `BP_CinderStickyGrenadeProjectile` from `ASovCinderStickyGrenadeProjectile`. Assign its inherited `Grenade Mesh` and `Explosion Niagara System`; `Explosion Niagara Scale` is available for per-projectile sizing. The native actor spawns that system once at the replicated detonation location on every non-dedicated-server instance. `Cinder Grenade Launched`, `Cinder Grenade Stuck`, and `Cinder Grenade Detonated` remain available for audio, decals, and additional presentation; do not spawn the same explosion system again from the event. Native code hides the mesh on detonation and owns all gameplay.
- The projectile also owns an inherited `Explosion Radial Force` component. Its one-shot impulse fires on the server at detonation and affects simulated props, destructibles, and ragdoll bodies without altering GAS damage. Select the component in `BP_CinderStickyGrenadeProjectile` to tune `Impulse Strength`, `Falloff`, `Impulse Vel Change`, and `Object Types to Affect`. The actor-level `Explosion Physics Radius Scale` multiplies the gameplay blast radius, while `Explosion Physics Upward Bias` shifts the force origin downward so floor-bound bodies lift instead of only sliding. Disable `Apply Explosion Physics Impulse` if a projectile variant should deal damage without physical force.
- In `GA_Tarrik_CinderStickyGrenade`, assign that Blueprint to `GrenadeClass`. Leave the two effect fields on their inherited native defaults, or replace them with Blueprint children of the matching native GameplayEffect classes. Empty class fields fall back to the native implementations; invalid non-positive tuning still fails before Echo is spent.
- The explosion GE must be `Instant`, use `UNarrativeDamageExecCalc`, and must not directly modify Health, Shield, Poise, or the Damage meta attribute. Native code supplies `SetByCaller.Damage`, radial falloff, Poise pressure, Kinetic + Thermal channels, Standard guard class, and the grenade as effect causer.
- The native Burn GE uses `SetByCaller.Duration`, ticks once per second without an immediate application tick, and refreshes instead of stacking when Tarrik reapplies it. Each tick routes through `UNarrativeDamageExecCalc`; the projectile supplies `SetByCaller.Damage`, Thermal, and Bypass Guard. Burn is applied only after the explosion actually reduces Shield, Health, or Poise, and is rejected by either exact `Sov.Status.Immunity` or `Sov.Status.Immunity.Burn`, so immunity and perfect defense do not receive a detached status effect.
- In the GA child, resolve and play the tagged Narrative anim set from `Echo Ability Started`. At the authoritative throw-frame notify, call `Release Cinder Sticky Grenade From Aim`. The native ability resolves `GrenadeThrowSocketName` (falling back to `GrenadeFallbackSpawnOffset`), traces the server controller's aim, solves a fixed-speed ballistic arc, and applies the same `GrenadeGravityScale` to the projectile. `Release Cinder Sticky Grenade(SpawnTransform, InitialVelocity)` remains available only for unusual server-validated custom throws.
- The release function is not a client-to-server RPC. The montage/release notify must run on the server ability instance as well as the predicting owner. Native authority and once-per-activation guards ensure that only the server creates one gameplay grenade.
- For a first smoke test, call `Release Cinder Sticky Grenade From Aim` directly from `Echo Ability Authority Committed`. Once that works, move it to the server's release-frame path. If your dedicated-server mesh does not evaluate animation notifies, start a server timer from `Echo Ability Authority Committed` using the montage's authored release offset; never depend on an owner-only AnimBP notify for gameplay.
- End the ability from montage completed/blend-out and cancel it on interruption. Successful release does not end the ability automatically, and the replicated grenade safely outlives the ability instance.
- Native behavior starts the fuse at launch, sticks once to a hit component/bone, damages unique hostile ASCs in radius, optionally requires line of sight, replicates stuck/detonated presentation state, and cleans itself up after detonation.
- Do not use Narrative's stock `Spawn Projectile` task for this Local Predicted ability: its replicated-projectile policy can spawn an unreconciled client copy as well as the authoritative copy. Spawn on authority and use prediction only for the throw presentation.

The other Tarrik parents also validate their declared payload assets now. They remain Blueprint-authored payload contracts until their own native vertical slices are implemented; an incomplete child will no longer consume Echo and do nothing.

### Velkorran's Hunger

- Server-spawn a physical blade-wave projectile from the sword/weapon visual socket.
- As with the grenade, spawn the replicated gameplay projectile only on authority; the predicting client may spawn a separate non-gameplay trail/muzzle cosmetic.
- Apply Edge + Thermal direct damage, then Burn on a confirmed valid target.
- Copy all ASC/effect data into the projectile; do not retain the ability instance after launch because unwield can remove the granted ability while the projectile is still alive.

### Cinder Slam

- Perform radial damage on authority, with high Poise damage through the normal damage execution.
- Knockback is a consequence of a valid Poise break, not an unconditional launch; this preserves boss and crowd-control immunity rules.
- Apply `ProtectiveWardEffectClass` to Tarrik for the brief orb. Prefer a short Damage Resistance/barrier policy plus Gameplay Cue over temporarily changing `Shield` or `MaxShield`.

### Cinder Judgement

- Perform an authority-validated Cinderline trace or server-owned very-fast projectile.
- Apply direct impact damage once and a separate radial explosion at the confirmed impact point.
- Recommended channels: Kinetic + Thermal, elevated Shield coefficient, and meaningful Poise pressure.

### Cinderline Requiem

- Perform an authority-validated penetrating line trace and deduplicate hit actors.
- Apply the penetrating hit once per target, then detonate a short sequence along the confirmed line or leave a brief burning lane.
- It is intentionally different from Judgement: Judgement is one explosive impact; Requiem controls an entire firing lane and applies extreme Poise pressure.

## Damage and status requirements

All damage must enter the configured SetByCaller damage Gameplay Effect with `UNarrativeDamageExecCalc`; direct Health/Shield writes bypass Guard, mitigation, Shield routing, Poise, and result events.

Use authored asset tags for damage channel and guard class. Explosions and projectiles must set an Effect Causer at the actual impact source so Guard evaluates direction from the blast/projectile rather than Tarrik's pawn.

`Sov.Status.Apply.Burn` is now a native request tag, but the tracked source still has no status listener that turns `Sov.Event.Status.ApplicationRequested` into a duration effect. Until that handler exists, apply the configured Burn duration GE directly after a confirmed damaging hit.

For Cinder Judgement and Requiem, do not trust an unvalidated client hit result. Re-trace or clamp origin, aim, and range on authority before applying a 50/90-Echo payload.
