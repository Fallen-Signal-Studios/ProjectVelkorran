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

The native base provides:

- Local Predicted GAS activation;
- server-owned termination, so a client cannot clear the committed Busy/Echo-action state before authority finishes or cancels the payload;
- replicated Echo threshold and affordability checks;
- authority-only, atomic Echo spending through `USovEchoComponent`;
- one active Echo action at a time, with Narrative's Busy state owned for the committed cast so ordinary combat cannot stomp its montage;
- common death, guard, guard-break, Poise-break, busy, interaction, sequencer, and ragdoll activation blocks, with active cancellation when terminal/interrupt states arrive during the cast;
- fail-closed `AllowedWeaponClasses` validation against the currently wielded item;
- display text, weapon family, animation-set key, and payload asset/tuning slots;
- `Echo Ability Started`, `Echo Ability Authority Committed`, `Echo Ability Local Presentation`, and `Echo Ability Ended` Blueprint hooks;
- `Get Authority Aim Target Data` for server-owned hitscan validation;
- a five-second stuck-ability failsafe, adjustable per child;
- `Finish Echo Ability` for montage/task completion.

Do not author a second Echo Cost Gameplay Effect on these abilities. A normal GAS cooldown or a separate Stamina/ammo cost may still be used; Echo itself is already committed by the native base.

Do not implement the generic `Event ActivateAbility` in these children. The native parent owns activation and commits Echo before it calls the custom hooks. Use `Echo Ability Started` for the predicted/authority montage and task setup, and use `Echo Ability Authority Committed` as the only authorization point for server gameplay. Do not execute the same payload from both hooks.

## Granting

Grant the Sticky Grenade once through Tarrik's default `UAbilityConfiguration`. Do not add it to both weapon assets or dual-wield/source-object variants can create duplicate specs.

Add the sword pair to Velkorran's `WeaponAbilities` and the rifle pair to Cinderline's `WeaponAbilities`. Set each sword Blueprint child's `AllowedWeaponClasses` array to the Velkorran item class and each rifle child's array to the Cinderline item class. Set Sticky Grenade's array to both weapon classes. These abilities intentionally fail closed when that array is empty.

Narrative activates every granted spec whose input tag matches. Never grant both weapon-context variants of Ability2 or Ability3 at the same time.

The native Tarrik Guard parent now blocks while Narrative Busy or an Echo ability is active. Echo abilities already block while Guarding, so the two committed lanes cannot enter simultaneously and fight over their montages/state.

## Animation flow

Each parent exposes a native animation key:

- `Narrative.Anim.AnimSets.Ability.Tarrik.CinderStickyGrenade`
- `Narrative.Anim.AnimSets.Ability.Tarrik.VelkorransHunger`
- `Narrative.Anim.AnimSets.Ability.Tarrik.CinderSlam`
- `Narrative.Anim.AnimSets.Ability.Tarrik.CinderJudgement`
- `Narrative.Anim.AnimSets.Ability.Tarrik.CinderlineRequiem`

Add the matching `UNarrativeAnimSet` to the active weapon linked layer's `Tagged Anim Sets`. Resolve it with `Get Anim Set` and `Search Linked Layers = true`, then play its paired 1P/3P montages through the same Narrative flow used by attacks.

In `Echo Ability Started`:

1. start the cast montage and its wait task;
2. use a montage notify/gameplay event for the release frame;
3. arm the authority release task from `Echo Ability Authority Committed`, then execute target, projectile, and damage payload only from that authoritative path;
4. call `Finish Echo Ability(false)` on completed/blend-out and `Finish Echo Ability(true)` on interruption.

Use `Echo Ability Local Presentation` only for owning-player camera, rumble, audio accents, and cosmetic hit-stop. Do not apply damage, spawn replicated actors, or change global time dilation there.

## Payload contracts

### Cinder Sticky Grenade

- Server-spawn one replicated grenade from the configured `GrenadeClass`.
- Do not use Narrative's stock `Spawn Projectile` task for this Local Predicted ability: its replicated-projectile policy can spawn an unreconciled client copy as well as the authoritative copy. Spawn on authority and use prediction only for the throw presentation.
- Ignore the owner during launch, attach to the hit component/bone, run the fuse on authority, then apply the configured radial effect once.
- Copy the configured effects, fuse, radius, source ASC/context, and ability tag into exposed-on-spawn projectile data. The projectile must never retain the Gameplay Ability instance.
- Recommended channels: Kinetic + Thermal. Add `Sov.Status.Apply.Burn` and `Sov.SetByCaller.Status.Magnitude` if the explosion requests Burn.

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
