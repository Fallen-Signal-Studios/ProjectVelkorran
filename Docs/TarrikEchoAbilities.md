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

Tarrik's player Blueprint must derive from `ASovTarrikCharacter`. That concrete class merges `Sov.Character.Player.Tarrik` into the definition-owned ASC tag contribution and removes a conflicting Selene identity. Keep the same Tarrik tag on the Player Definition so the asset remains self-describing. Native Tarrik abilities now require the Tarrik identity at activation and release. Legacy Blueprints deriving directly from `ASovPlayerCharacterBase` must supply the correct definition tag or migrate to `ASovTarrikCharacter`; untagged content is rejected before Echo is spent.

Grant the Sticky Grenade once through Tarrik's default `UAbilityConfiguration`. Do not add it to both weapon assets or dual-wield/source-object variants can create duplicate specs. It is character-granted and works with either Tarrik weapon, so it deliberately ignores `AllowedWeaponClasses`.

Add the sword pair to Velkorran's `WeaponAbilities` and the rifle pair to Cinderline's `WeaponAbilities`. Set each sword Blueprint child's `AllowedWeaponClasses` array to the Velkorran item class and each rifle child's array to the Cinderline item class. Those weapon-specific abilities intentionally fail closed when that array is empty.

Narrative activates every granted spec whose input tag matches. Never grant both weapon-context variants of Ability2 or Ability3 at the same time.

The native Tarrik Guard parent now blocks while Narrative Busy or an Echo ability is active. Echo abilities already block while Guarding, so the two committed lanes cannot enter simultaneously and fight over their montages/state.

## Cinderline Echo generation

`ASovTarrikCharacter` owns the replicated `USovTarrikEchoGenerationComponent`; the shared player base and Selene do not. Do not add a second generator in the Blueprint Components panel: duplicates are rejected at runtime so one hit cannot award twice. The generator listens to Narrative's post-resolution `OnDealtDamage` callback on authority and awards through the existing Echo component; it never writes the GAS Echo attribute directly.

The default **Cinderline Cadence** loop is:

| Confirmed primary-fire result | Cadence / Echo |
|---|---:|
| Ordinary damaging hit | +1 Cadence |
| Precision damaging hit | +2 Cadence |
| Reach 6 Cadence | +4 Echo, then reset |
| Fatal precision hit | +3 Echo independently |

Unfinished Cadence expires 1.5 seconds after the most recent qualifying hit. This is a confirmed-hit streak with a maximum inter-hit gap, not a beat detector or an authored minimum/ideal burst interval. Ordinary Cadence payouts are limited to one per second with no reward backlog. A completed sequence waits out the short payout cooldown instead of being deleted by the unfinished-sequence timer. Bosses tagged `Sov.Character.Enemy.Boss` contribute and pay at 75% strength by default; the component weights every accepted point so building on a boss and finishing on a grunt cannot bypass that reduction.

Shield damage counts. Misses, zero-damage immunity, attacks into the air, friendly targets, successful Guard interception, Burn ticks, grenades, explosions, Judgement, Requiem, other weapons, and all `Sov.Ability.Echo` abilities do not count. The sequence clears on timeout, unwielding its source Cinderline instance, a damage callback from another ranged weapon, death/fatal state, Echo-action start, ASC replacement, or reaching full Echo. At full Echo the loop pauses and discards partial Cadence; campaign Echo is capped at 100 with no overflow resource.

### Primary-fire setup

On the ordinary Cinderline fire Gameplay Ability, add this native tag to **Asset Tags**:

`Sov.Ability.Weapon.Cinderline.PrimaryFire`

This is the preferred stable identity. The component also recognizes Narrative's stock Attack-input ranged `Ability.WeaponFire` path when the source weapon instance grants Cinder Judgement or Cinderline Requiem, so an already-complete Cinderline loadout normally works without a second weapon field. As an explicit content fallback, add the Cinderline item Blueprint class to the inherited component's `Allowed Cinderline Weapon Classes` array on Tarrik's character Blueprint.

The damage Gameplay Effect must be `Instant`, and the fire ability must preserve its normal GAS context. A qualifying spec needs both:

- Cinderline as `SourceObject` (Narrative supplies this for a weapon-granted ability spec); and
- the originating Narrative Gameplay Ability plus the authoritative blocking `FHitResult`, including the hit bone and component.

Do not apply the primary-fire marker to Cinder Judgement, Requiem, Burn, or a shared damage effect used by another weapon. Putting it on the ordinary fire ability is safest because `Context.GetAbility()` retains that exact classification while the damage shell can remain reusable.

The generator trusts Narrative's already-applied authoritative damage result; it does not re-trace the bullet. Any custom primary-fire ability that accepts client target data must still validate or re-trace origin, direction, range, target, bone, and physical material on authority before applying its damage effect.

### Precision and presentation

`head` is the default SK Mannequin precision root. Exact `head` hits and child bones on a skinned hit component qualify; Narrative physical materials with a damage multiplier of at least 1.01 also qualify by default. Add creature-specific roots to `Precision Bone Names`, or disable the physical-material fallback if bone mapping should be authoritative.

The component exposes Blueprint presentation hooks without granting cosmetic authority:

- `On Cinderline Hit Confirmed`: current progress, threshold, precision flag, hit bone, and boss-reduction flag;
- `On Cinderline Cadence Changed`: replicated owner-only progress for the HUD;
- `On Cinderline Echo Awarded`: actual Echo received, new total, cadence/precision-kill source, and boss-reduction flag;
- normalized and integer Cadence getters; and
- `Reset Cinderline Cadence` for an authority-owned encounter or weapon-flow reset.

Use those hooks for reticle ticks, rising pitch, Cinderline emissive pulses, hit Niagara, payout flashes, rumble, and precision-kill accents. They are owning-client cosmetic notifications; damage, Echo, and cadence eligibility remain server-owned.

For a smoke test, start below maximum Echo, wield Cinderline, and land each qualifying hit within 1.5 seconds of the previous one. Six body hits award 4 Echo; three precision hits do the same; a fatal precision hit also awards 3 Echo. Confirm separately that Shield damage advances Cadence, while Guard, a friendly, a miss, a Burn tick, and Cinder Judgement do not. A boss should pay 3 Echo for six body points and 2.25 Echo for a precision kill at the default multiplier. Repeat on a listen server and owning client: the owner-only HUD hooks should fire, and authority should award each payout only once.

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

All five Tarrik abilities now own native authority release and recovery timing. An empty gameplay Event Graph works after weapon allowlists and the Tarrik identity are configured. A server montage event can call the corresponding release function earlier; that event and the native fallback share one release gate. Tune `Payload Release Delay` and `Post Release Recovery` to the montage. See `TarrikNativeCompletion.md` for defaults, migration and validation.

## Payload contracts

### Cinder Sticky Grenade

- Native defaults provide `Sovereign Cinder Grenade Explosion Damage`, the canonical Burn status shell, and a functional native projectile fallback. Existing GA Blueprint children that serialized the old empty values still resolve safely. Blueprint subclasses remain optional for damage Gameplay Cues, a visible projectile mesh, or projectile tuning; status tuning belongs to the Burn definition.
- Create `BP_CinderStickyGrenadeProjectile` from `ASovCinderStickyGrenadeProjectile`. Assign its inherited `Grenade Mesh` and `Explosion Niagara System`; `Explosion Niagara Scale` is available for per-projectile sizing. The native actor spawns that system once at the replicated detonation location on every non-dedicated-server instance. `Cinder Grenade Launched`, `Cinder Grenade Stuck`, and `Cinder Grenade Detonated` remain available for audio, decals, and additional presentation; do not spawn the same explosion system again from the event. Native code hides the mesh on detonation and owns all gameplay.
- The projectile also owns an inherited `Explosion Radial Force` component. Its one-shot impulse fires on the server at detonation and affects simulated props, destructibles, and ragdoll bodies without altering GAS damage. Select the component in `BP_CinderStickyGrenadeProjectile` to tune `Impulse Strength`, `Falloff`, `Impulse Vel Change`, and `Object Types to Affect`. The actor-level `Explosion Physics Radius Scale` multiplies the gameplay blast radius, while `Explosion Physics Upward Bias` shifts the force origin downward so floor-bound bodies lift instead of only sliding. Disable `Apply Explosion Physics Impulse` if a projectile variant should deal damage without physical force.
- Assign a Deferred Decal material to `Explosion Decal Material` for a post-blast scorch mark. On each non-dedicated client, the projectile searches the surface it struck plus the nearest ground, ceiling, and surrounding walls, then attaches the decal to the closest world-static or world-dynamic component. The defaults keep it fully visible for five seconds, fade it for one second, and destroy the transient decal afterward. Use Unreal's `Decal Lifetime Opacity` material expression in the decal opacity path so `SetFadeOut` is visually represented; otherwise the component still cleans itself up but the material may pop off at the end. `Explosion Decal Size`, search distance, surface offset, visible duration, and fade duration are all editable on the projectile Blueprint.
- In `GA_Tarrik_CinderStickyGrenade`, assign that Blueprint to `GrenadeClass`. Leave the damage effect on its inherited native default or replace it with a Blueprint child of the matching native GameplayEffect class. The serialized Burn effect slot is retained for asset compatibility, but runtime Burn policy/effect selection is owned by `USovStatusComponent`; use a Burn `USovStatusDefinition` override for custom status behavior. Invalid non-positive tuning still fails before Echo is spent.
- The explosion GE must be `Instant`, use `UNarrativeDamageExecCalc`, and must not directly modify Health, Shield, Poise, or the Damage meta attribute. Native code supplies `SetByCaller.Damage`, radial falloff, Poise pressure, Kinetic + Thermal channels, Standard guard class, and the grenade as effect causer.
- The explosion damage spec publishes `Sov.Status.Apply.Burn` with `Sov.SetByCaller.Status.Magnitude` and `.Duration`. The status component applies the tracked Burn only after that transaction actually reduces Shield, Health, or Poise and the target survives. Native Burn ticks once per second without an immediate tick and routes through `UNarrativeDamageExecCalc` with Thermal, Bypass Guard, and Bypass Deflection. Equal strength refreshes, stronger replaces, and weaker is rejected. Global/Burn immunity and perfect defense therefore cannot receive a detached Burn.
- In the GA child, resolve and play the tagged Narrative anim set from `Echo Ability Started`. At the authoritative throw-frame notify, call `Release Cinder Sticky Grenade From Aim`. The native ability resolves `GrenadeThrowSocketName` (falling back to `GrenadeFallbackSpawnOffset`), traces the server controller's aim, solves a fixed-speed ballistic arc, and applies the same `GrenadeGravityScale` to the projectile. `Release Cinder Sticky Grenade(SpawnTransform, InitialVelocity)` remains available only for unusual server-validated custom throws.
- The release function is not a client-to-server RPC. The montage/release notify must run on the server ability instance as well as the predicting owner. Native authority and once-per-activation guards ensure that only the server creates one gameplay grenade.
- Native fallback releases the grenade at 0.35 seconds, then recovers for 0.4 seconds. A server release-frame notify may release earlier. Do not create a second Blueprint gameplay timer or spawn path.
- End the ability from montage completed/blend-out and cancel it on interruption. Successful release schedules native recovery, and the replicated grenade safely outlives the ability instance.
- Native behavior starts the fuse at launch, sticks once to a hit component/bone, damages unique hostile ASCs in radius, optionally requires line of sight, replicates stuck/detonated presentation state, and cleans itself up after detonation.
- Do not use Narrative's stock `Spawn Projectile` task for this Local Predicted ability: its replicated-projectile policy can spawn an unreconciled client copy as well as the authoritative copy. Spawn on authority and use prediction only for the throw presentation.

All five Tarrik abilities now own native gameplay payloads. Blueprint children supply weapon allowlists, presentation and optional tuning. Remove legacy Blueprint damage, ward and projectile spawning before using the native release functions.

### Velkorran's Hunger

- Native defaults provide `Sovereign Velkorran's Hunger Damage`, the canonical Burn status shell, and a functional `ASovVelkorransHungerProjectile` fallback. Existing GA Blueprint children that serialized the original empty payload fields resolve safely.
- Create `BP_VelkorransHungerProjectile` from `ASovVelkorransHungerProjectile`. Assign its inherited `Projectile Mesh`, `Impact Niagara System`, and `Dissipation Niagara System`. The actor automatically spawns the configured resolution system once on every non-dedicated-server instance. `Hunger Projectile Launched`, `Hunger Projectile Impacted`, and `Hunger Projectile Dissipated` remain available for audio, trails, camera accents, and additional presentation; do not spawn the same native impact system again from those events.
- In `GA_Tarrik_VelkorransHunger`, assign that Blueprint to `ProjectileClass`. Leave the direct effect on its inherited native default, or replace it with a Blueprint child for damage-specific Gameplay Cues. The serialized Burn effect slot remains only for asset compatibility; Hunger and Cinder Sticky Grenade now request the same status definition, so one canonical Burn refreshes rather than creating parallel effects.
- Add a socket named `HungerRelease` to Velkorran's active weapon visual mesh, or change `Hunger Release Socket Name` in the GA child to the asset's existing socket. The native release resolves Narrative's wielded main-hand `AWeaponVisual` and prefers its replicated world mesh so listen and dedicated servers produce the same launch point. If that mesh, visual, or socket is unavailable, it uses `Hunger Fallback Spawn Offset` on Tarrik rather than failing the paid action.
- Resolve and play the tagged Narrative anim set from `Echo Ability Started`. At the authoritative release-frame notify, call `Release Velkorrans Hunger From Aim`. The native function traces the server controller's aim, launches from the weapon socket, and creates exactly one replicated gameplay projectile. It is not a client-to-server RPC, so the montage/release path must execute on the server ability instance as well as the predicting owner.
- For a first smoke test, call `Release Velkorrans Hunger From Aim` directly from `Echo Ability Authority Committed`. Once launch and damage are verified, move the call to the server's authored release frame. If a dedicated server does not evaluate the montage notify, start a server timer from the authority-committed hook using the montage's release offset.
- Native defaults launch a straight `4200 cm/s` blade wave for `1.25 s` with a `35 cm` collision radius. The projectile overlaps Pawn collision so allies do not absorb the shot, resolves on the first valid hostile or blocking world surface, and dissipates harmlessly at its flight limit. All values remain editable on the GA child.
- A valid hostile impact applies `65` direct damage and `30` Poise pressure through `UNarrativeDamageExecCalc` with Edge + Thermal and Standard guard classification. The same damage spec requests Burn magnitude/duration; centralized status validation accepts it only after positive nonfatal Shield, Health, or Poise damage. Perfectly defended, fatal, or immune hits do not receive a detached Burn.
- The projectile copies the source ASC, source object, effect classes, ability tag, effect level, movement, and damage values before `FinishSpawning`. It never retains the Gameplay Ability instance, so unwielding Velkorran after launch cannot invalidate the paid projectile.
- End the ability from montage completed/blend-out and cancel it on interruption. Successful release schedules native recovery, and the replicated projectile safely outlives the ability instance.
- Do not use Narrative's stock `Spawn Projectile` task for this Local Predicted ability. The predicting owner may create a separate non-gameplay trail or release flash, but only authority may spawn the gameplay projectile.

### Cinder Slam

- Native authority release at 0.55 seconds applies a 450 cm radial heavy Kinetic/Thermal blast, 90 base damage and 80 Poise pressure, with linear falloff to 50%. It deduplicates ASCs and requires line of sight.
- Knockback is allowed only for the typed damage receipt's new Poise break on a surviving non-boss target without super armor.
- The native Cinder Ward adds 50 Damage Resistance for 2.5 seconds and refreshes without stacking from the same source. It never changes current or maximum Shield.
- `Release Cinder Slam` and `Receive Cinder Slam Released` expose the notify and authority cosmetic hooks; native recovery lasts 0.65 seconds.

### Cinder Judgement

- Native defaults provide `Sovereign Cinder Judgement Damage` for both stages and a functional `ASovCinderJudgementPresentation` fallback. Legacy GA children that serialized empty effect fields resolve the native damage class automatically. Any replacement effect must be a Blueprint child of that native damage shell; an unrelated or malformed effect class is rejected in favor of the safe fallback.
- The authority performs an eye/control-rotation aim trace, then re-traces from Cinderline's muzzle. This preserves camera aiming while preventing a third-person camera from firing through nearby cover. Client target data is never trusted.
- The direct stage deals 60 base damage, 30 Poise, and uses a 1.5 Shield coefficient. It retains the full authoritative `FHitResult`, bone, and physical material.
- The separate 325 cm blast deals 45 base damage and 25 Poise at center, falls linearly to 35% at the edge, and uses a 1.25 Shield coefficient. Its context deliberately has no direct-hit result, so a headshot multiplier is not applied to the explosion.
- Both stages are Standard Guard-class Kinetic + Thermal damage. A surviving primary target receives both stages. Burn is intentionally not applied; Sticky Grenade and Hunger own Tarrik's Burn space.
- The blast deduplicates Narrative ASCs, resolves runtime CharacterVisual/attachment ownership, excludes non-hostiles and dead targets, requires line of sight for non-primary targets, and routes all damage through `UNarrativeDamageExecCalc`.
- Native authority timing releases at 0.28 seconds and ends after 0.45 seconds of recovery. Tune those fields to the montage. The base three-second watchdog still cancels any authored path that never releases or finishes.
- Create `BP_CinderJudgementPresentation` from `ASovCinderJudgementPresentation`, then assign it to the GA's `Presentation Class`. The native class is gameplay-safe with no art, while the Blueprint child exposes slots for muzzle Niagara, beam endpoint/length parameters, direct impact, radial explosion and radius parameter, miss dissipation, fire/impact/explosion/dissipation audio with volume and pitch tuning, camera shakes, and a searched scorch decal.
- Set the Cinderline weapon mesh socket used by the GA's `Muzzle Socket Name` (default `Muzzle`). If the socket or runtime weapon visual is unavailable, the authority uses `Fallback Muzzle Offset` on Tarrik.
- The presentation actor is an immutable, short-lived replicated packet. It emits each one-shot once, suppresses stale late delivery, and is also used as the blast Effect Causer so Guard direction comes from the detonation point. Do not duplicate the central explosion as a Gameplay Cue on the radial effect, because that would play once per damaged target.
- Simulated props and ragdolls receive the optional native radial impulse, using the same line-of-sight policy as gameplay damage. Character reactions remain owned by Poise/Guard outcomes; the ability never unconditionally launches living targets.
- Remove legacy Blueprint traces, damage application, radial loops, and central explosion multicasts from `GA_Tarrik_CinderJudgement` before enabling the native path, or the shot will resolve twice.

### Cinderline Requiem

- Native eye/muzzle aiming uses Narrative's configured weapon trace channel, penetrates resolved character ASCs and stops at the first world blocker. Each hostile receives at most one 100-damage/80-Poise direct hit. Friendly characters are penetrated without damage.
- `ASovCinderRequiemLine` owns the paid chain after release: 220 cm blasts spaced by 350 cm at 0.035-second intervals, bounded to 64 points, with one 60-damage/100-Poise line packet per hostile across the entire sequence. Endpoints remain on the confirmed clear line and every radial hit requires line of sight.
- Confirmed line damage applies the existing source-owned Cinder Burn, 5 damage per second for 4 seconds, with existing immunity policy. The paid lane survives ordinary ability recovery and weapon changes; encounter cleanup owns it through its source-pawn owner/instigator.
- `Release Cinderline Requiem From Aim` supports an earlier server notify. Native release is 0.45 seconds and recovery 0.65 seconds. Optional `Line Class` Blueprint children receive replicated `Receive Line Detonation` cosmetic events.

## Damage and status requirements

All damage must enter the configured SetByCaller damage Gameplay Effect with `UNarrativeDamageExecCalc`; direct Health/Shield writes bypass Guard, mitigation, Shield routing, Poise, and result events.

Use authored asset tags for damage channel and guard class. Explosions and projectiles must set an Effect Causer at the actual impact source so Guard evaluates direction from the blast/projectile rather than Tarrik's pawn.

`USovStatusComponent` owns generic Burn, Chill, Freeze, Device Disabled, and Exposed requests, including Cinder Sticky Grenade and Velkorran's Hunger Burn. Other native Tarrik payloads that consume a damage receipt and apply their own Burn mark the spec `Sov.Status.Application.NativeOwned`; that receipt does not also invoke the generic listener. Damage-coupled generic statuses travel on the unified damage spec; non-damage generic abilities call `Apply Status By Tag` on authority. Do not add a second duration effect to either path. See `Docs/StatusAndCorruptionPrototype.md` for reapplication, immunity, cleanse, presentation, and checkpoint contracts.

For Cinder Judgement and Requiem, do not trust an unvalidated client hit result. Re-trace or clamp origin, aim, and range on authority before applying a 50/90-Echo payload.
