# Reformation Drone Weapon Abilities

The Reformation drone package provides three server-authoritative Narrative/GAS attacks:

- `USovGameplayAbility_ReformationDroneGunfire`: a configurable hitscan burst on `Narrative.Input.Attack`.
- `USovGameplayAbility_ReformationDroneRocketLauncher`: a replicated radial-damage rocket on `Narrative.Input.AltAttack`.
- `USovGameplayAbility_ReformationDroneSelfDestruct`: an AI-driven pursuit, warning fuse, radial blast, and Narrative-native self death on `Narrative.Input.Attack`.

All three abilities are intended to be granted by the drone's Narrative **Ability Configuration**. They are integral NPC attacks, not weapon-item abilities, and do not require ammo or an equipped `UWeaponItem`.

## One-time editor setup

Close the editor and perform a full `ProjectVelkorranEditor` build after pulling the implementation. The change adds reflected native classes and native Gameplay Tags; Live Coding is not suitable for the first load.

### Drone pawn base and death safety

Reparent the mechanical gun, rocket, and explosive drone character Blueprints to `ASovDroneNPCBase`. It remains inside `ASovNPCCharacterBase`, so Narrative initialization, the ASC, teams, death, loot, and the project combat-sustain drop component continue to work. It replaces the inherited humanoid dismemberment implementation with a disabled drone-safe subobject and never enters the mannequin ragdoll path. Do not add a second Dismemberment or combat-sustain component in the Blueprint.

The capsule is still the grounded navigation/authority proxy; only the skeletal mesh receives the optional hover sine offset. Tune hover amplitude/frequency on the inherited component defaults, and do not raise the actor/capsule or convert CharacterMovement to flying merely to obtain the visual hover. On death, the base stops AI and movement, disables mesh collision, and can retain a query-only capsule for Narrative interaction without blocking pawns. Narrative revive/recycle restores collision and movement and restarts the AI brain.

`Enable Death Explosion On Death` defaults off. Keep it off on the Self Destruct variant unless an encounter has a separately approved ordinary-death blast. Native Self Destruct explicitly suppresses the base death explosion before its fatal self-hit and also detects a committed detonation as a fallback; do not add a Blueprint `OnDeath`, `AnyDamage`, or montage-notify explosion that can produce a second payload. If an ordinary drone variant opts in, assign the Narrative damage effect and presentation assets, verify hostile-team and wall-LOS filtering, and test that each ASC receives exactly one radial hit. Self Destruct must still produce one warning, one blast, one fatal self-hit, and no second death explosion on listen and dedicated servers.

To make a drone a valid Selene precision target, add one `USovWeakPointComponent` to its Blueprint and author zones against real drone bones or physical materials. The drone base does not create a weak point automatically.

Create these Blueprint children:

| Suggested asset | Native parent | Purpose |
|---|---|---|
| `GA_ReformationDrone_Gunfire` | `Reformation Drone: Gunfire` | Damage, burst, targeting, timing, montage, and muzzle configuration |
| `BP_ReformationDrone_GunshotPresentation` | `Reformation Drone Gunshot Presentation` | Muzzle flash, tracer, impacts, audio, decals, and camera shakes |
| `GA_ReformationDrone_RocketLauncher` | `Reformation Drone: Rocket Launcher` | Explosion, launch, homing, timing, montage, and muzzle configuration |
| `BP_ReformationDrone_Rocket` | `Reformation Drone Rocket Projectile` | Rocket mesh, trail, flight audio, explosion presentation, decal, and physics impulse |
| `GA_ReformationDrone_SelfDestruct` | `Reformation Drone: Self Destruct` | Pursuit, warning duration, blast damage/falloff, LOS, and AI movement tuning |
| `BP_ReformationDrone_SelfDestructPresentation` | `Reformation Drone Self Destruct Presentation` | Runtime material charge, travel/warning audio and Niagara, explosion art, decal, shake, and impulse |

Set the gun ability's **Gunshot Presentation Class** to the presentation Blueprint. Set the rocket ability's **Rocket Class** to the rocket Blueprint. The native presentation and projectile classes are valid art-free defaults. Gun gameplay also remains functional if its optional presentation class is deliberately cleared.

The shared native **Damage Effect Class** is also Blueprintable. A child may add Gameplay Cues or project metadata, but should retain the instant Narrative damage execution and leave damage magnitude to the ability/projectile SetByCaller payload.

Add the appropriate Gameplay Ability Blueprints to each drone's Narrative **Ability Configuration → Default Abilities**. Do not put them in a weapon item's granted ability array. The native defaults already use:

| Ability | Input tag | Identity tag |
|---|---|---|
| Gunfire | `Narrative.Input.Attack` | `Sov.Ability.NPC.ReformationDrone.Gunfire` |
| Rocket launcher | `Narrative.Input.AltAttack` | `Sov.Ability.NPC.ReformationDrone.RocketLauncher` |
| Self destruct | `Narrative.Input.Attack` | `Sov.Ability.NPC.ReformationDrone.SelfDestruct` |

The explosive drone should receive **Self Destruct as its only Attack ability**. Do not also grant Gunfire or an inherited placeholder on `Narrative.Input.Attack`; Narrative's input lookup does not provide a useful override guarantee when multiple active specs claim the same slot. The gun/rocket drone may continue using Gunfire plus Rocket Launcher.

Narrative's normal AI attack query can use their authored bot frequency/range values. A custom StateTree can also activate the same input-tagged specs explicitly. Make sure the drone is on a team that regards the player as hostile; friendly and neutral actors block shots but are not damaged.

## Muzzle sockets and aim

Create these sockets on the drone skeleton, ideally on the weapon-barrel bones:

- `Muzzle_Gun`
- `Muzzle_Rocket_L`
- `Muzzle_Rocket_R`

The gun cycles its configured socket list across burst shots. The rocket launcher alternates left and right. Missing sockets use **Fallback Muzzle Offset**, so the attacks remain functional while art is being integrated, but authored sockets are strongly recommended.

Sockets must exist on the animation-driving skeletal mesh. Narrative's runtime appearance may be a separate visual actor, but the character mesh remains the authoritative pose and targeting source.

Keep the authoritative drone mesh's **Visibility Based Anim Tick Option** at **Always Tick Pose and Refresh Bones** (Narrative's normal character setup does this). If a drone Blueprint overrides that setting, dedicated-server muzzle sockets can remain in reference pose even though clients animate correctly.

AI aim uses the controller focus location. Non-AI authority uses control rotation. A trace resolves the first world blocker before the weapon trace begins at the muzzle, preventing fire through nearby cover.

## Animation

Assign an optional drone-skeleton montage to each ability's **Attack Montage** slot. The montage is played through GAS, so its authoritative playback is replicated. Narrative now refreshes GAS actor info whenever its runtime appearance installs or replaces the root AnimInstance; this prevents montages from targeting a stale AnimInstance after appearance construction.

The montage must use the drone's own Skeleton, and the drone AnimBP must contain a **Slot** node matching the montage slot (for example `DefaultGroup.DefaultSlot`) between its locomotion pose and the final output pose. A full-body slot is usually simplest for this non-biped drone; use a layered blend only if its rig has a meaningful weapon/body partition. Keep gameplay root motion disabled unless the StateTree deliberately coordinates it.

Gameplay release never depends on an animation notify:

- Gunfire defaults to a short native payload delay, then owns its burst with authority timers.
- The rocket defaults to a longer native payload delay aligned to a launcher wind-up.
- Self Destruct uses native pursuit and fuse timers; it does not require a montage or animation notify.
- Tune **Payload Release Delay** to the visual fire frame.
- Keep **Auto Release Payload** enabled for normal use.

For an exceptional server-authored sequence, disable automatic release and call `Fire Gun Burst From Aim`, `Launch Rocket From Aim`, or `Start Self Destruct Run` from the Gameplay Ability Blueprint. Do not call those functions from a cosmetic AnimBP notify; simulated-client notifies are not gameplay authority.

Cosmetic montage notifies are still appropriate for local charge glows, servo sounds, barrel movement, or warning lights. Keep only damage, tracing, and projectile release on the native authority path.

## Gun presentation slots

Configure these on `BP_ReformationDrone_GunshotPresentation`:

- Muzzle Niagara, scale, rotation offset, fire sound, and fire camera shake
- Tracer Niagara, scale, endpoint parameter, and length parameter
- Impact Niagara, rotation/scale, sound, camera shake, and fading attached decal
- `Gunshot Presented` Blueprint cosmetic event (including physical surface type) for project-specific lights, shells, material flashes, or surface routing

The default tracer parameter names are `User.TracerEnd` (world-space vector) and `User.TracerLength` (centimeters). Either parameter may be set to `None` if the Niagara system does not expose it.

Each server shot creates one short-lived immutable presentation packet. All clients render the same quantized muzzle, endpoint, hit normal, actor, and bone once; no damage logic runs in the presentation actor.

## Rocket presentation slots

Configure these on `BP_ReformationDrone_Rocket`:

- **Rocket Mesh** component for model/material art
- Launch/backblast Niagara, trail Niagara, and their relative transforms
- Launch sound and spatial flight-loop sound
- Explosion Niagara, sound, camera shake, and fading surface decal
- Dissipation Niagara and sound for rockets that time out without impact
- Tunable radial-force component and physics radius/upward-bias controls
- `Drone Rocket Launched`, `Drone Rocket Impacted`, and `Drone Rocket Dissipated` Blueprint cosmetic events

Projectile forward is local **+X**. Rotate the inherited Rocket Mesh, trail transform, or launch transform on the Blueprint child when the purchased model uses a different forward axis; do not rotate the collision/movement root to compensate.

The server owns projectile collision, optional homing, expiry, and radial gameplay. The replicated actor owns flight and impact presentation for clients. Explosion damage uses Kinetic + Thermal channels, Heavy guard classification, distance falloff, optional world line of sight, and the shared Narrative damage execution. Only unique living hostile ASCs are damaged.

The checked-in collision configuration now registers Narrative's channel 6 as the `NarrativeProjectile` object channel. Ordinary characters, world geometry, physics bodies, and other projectiles block it; overlap/sensor profiles ignore it. This registration also corrects the existing Velkorran's Hunger projectile path. Preserve that channel assignment if collision settings are later regenerated in Project Settings.

## Explosive drone setup and presentation

Set `GA_ReformationDrone_SelfDestruct`'s **Presentation Class** to `BP_ReformationDrone_SelfDestructPresentation`. The native presentation class is an art-free fallback, so pursuit, damage, and self death remain functional before assets are assigned.

The ability first uses the AIController focus actor. If focus is missing, it performs one authority-only nearest-hostile-player fallback within **Target Acquisition Range**. **Only Acquire Player Controlled Targets** defaults on, so an explosive drone does not kamikaze an allied summon or a third faction by accident; disable it for encounters that deliberately target other hostile NPCs. It sends one `MoveToActor` request that follows the moving target, monitors range on a lightweight timer, stops at **Detonation Trigger Radius**, and then runs the native **Detonation Warning Duration**. A lost/dead target or failed move cancels before arming. Once armed, target loss does not cancel the blast, but death, Poise break, Freeze, or Device Disable can still interrupt the fuse. A pursuit timeout can either arm in place or cancel through **Detonate When Pursuit Times Out**.

The drone must have an AIController and a compatible NavMovement/CharacterMovement component. Leave **Use Pathfinding** enabled for a ground drone on Recast NavMesh. Disable it for an authored flying/direct-movement setup whose path-following component supports direct movement. The ability retains its path-following request ID and aborts only that request, so ending the attack cannot cancel an unrelated Narrative move. The StateTree/Behavior Tree should still treat `State.Weapon.IsFiring` as an exclusive attack lane and avoid issuing a competing move; if another task replaces the pursuit or starts moving during the fuse, Self Destruct safely cancels without aborting the new task. A failed move request logs a focused setup warning and causes no damage or self death.

Configure these presentation slots on the presentation Blueprint:

- **Material:** `SelfDestructCharge` parameter name, pursuit value/pulse, warning ramp/pulse, and inactive reset value
- **Travel:** attached Niagara, relative transform, spatial looping travel sound, volume, and pitch
- **Warning:** attached Niagara plus the non-looping detonation-indication sound
- **Explosion:** world-space Niagara, `User.ExplosionRadius` float, true explosion sound, camera shake, fading surface decal, and radial-force component
- Blueprint cosmetic events for pursuit, warning, detonation, and cancellation

Add the scalar parameter named `SelfDestructCharge` to every material that should respond. The presentation creates/reuses dynamic material instances on both the authoritative drone actor and Narrative's runtime `CharacterVisual`, including static modules, and rescans while active so asynchronously loaded appearance pieces receive the current synchronized value. Cancellation resets only this scalar; it does not replace materials or disturb other dynamic parameters.

The travel sound asset should loop and use spatial attenuation. The detonation indication and true explosion sounds should be non-looping. Travel/warning Niagara generally works best in local space; explosion Niagara should normally simulate in world space. Warning progress is derived from the replicated phase timestamp and GameState server clock rather than per-frame replication. The presentation freezes at the blast location and outlives the drone long enough for explosion audio and effects to finish.

Gameplay order is fixed: immutable detonation state is prepared first so it survives any synchronous callback, unique living hostile ASCs receive the Kinetic + Thermal Heavy blast through `UNarrativeDamageExecCalc`, presentation and physics finalize at that location, then the drone receives a separate fatal self-hit. This preserves Narrative damage credit, Guard/Shield/Health/Poise routing, death, loot, ragdoll, and replication. Ordinary guard is punished by the Heavy classification; a perfect guard, evade, interruption, or cover during the readable warning remains valid counterplay.

## Default gameplay tuning

| Setting | Gunfire | Rocket launcher | Self destruct |
|---|---:|---:|---:|
| Base damage | 12 per shot | 55 at center | 80 at center |
| Poise pressure | 4 per shot | 35 at center | 50 at center |
| Burst | 3 shots, 0.1 s apart | 1 rocket | 1 blast |
| Maximum range | 5,000 cm | 6,000 cm AI attack range; 8,000 cm aim trace | 4,000 cm acquisition |
| Ability cooldown | 0.75 s | 4.0 s | 0.5 s (normally dies) |
| Bot attack frequency | 0.9 s | 5.0 s | 10.0 s |
| Explosion radius | — | 450 cm | 425 cm |
| Minimum damage at radius edge | — | 30% | 30% |
| Pursuit/fuse | — | — | 8.0 s / 0.85 s |
| Rocket speed | — | 2,600 cm/s | — |
| Flight limit | — | 5.0 s | — |

These are native-safe starting values, not balance locks. Override them on the ability Blueprints.

## Verification checklist

1. Start with a fresh PIE or Standalone session after granting the intended abilities.
2. Confirm the expected specs appear in `showdebug abilitysystem` on the drone, with only one spec claiming each input.
3. Confirm the drone and player have hostile Narrative team attitudes.
4. Test gunfire with an empty presentation Blueprint first: Health/Shield/Poise should resolve even without art.
5. Confirm a wall stops both a tracer and its damage.
6. Confirm gunfire hits carry the skeletal bone in `FSovDamageResult`.
7. Test rocket center, edge falloff, cover line of sight, direct hits, homing, expiry, and world collision.
8. Run a two-player listen-server test: each client should see one gun presentation per shot and one rocket impact, with no duplicate audio or decals.
9. Test dedicated server plus clients if available; dedicated authority should run gameplay without constructing cosmetic systems.
10. Swap or rebuild the Narrative appearance, then fire again and verify both montages still play on the current drone AnimInstance.
11. For the explosive variant, verify pursuit, trigger radius, the full warning window, wall LOS, falloff, one damage application per ASC, and normal Narrative death/loot/ragdoll.
12. Interrupt Self Destruct during pursuit and warning with death, Poise break, Freeze, and Device Disable. Movement, loop audio, warning FX, and the material scalar should stop/reset without a blast.
13. On a listen server with two observers, verify each client hears one warning and one true explosion, including when the drone dies immediately after detonation. Join or become relevant mid-pursuit/mid-warning and verify current loop/scalar reconstruction without duplicate one-shots.
14. Swap or asynchronously load the explosive drone's Narrative appearance during pursuit; every newly visible runtime mesh should inherit the current `SelfDestructCharge` value.

If activation never begins, first check that the abilities are in the NPC Ability Configuration, the drone has been fully initialized by Narrative, no firing/death/ragdoll/Poise-break/Freeze/device-disable tag is active, and the StateTree is requesting the matching Attack or Alt Attack input.
