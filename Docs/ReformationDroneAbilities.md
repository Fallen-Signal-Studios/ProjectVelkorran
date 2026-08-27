# Reformation Drone Weapon Abilities

The Reformation drone weapon package provides two server-authoritative Narrative/GAS attacks:

- `USovGameplayAbility_ReformationDroneGunfire`: a configurable hitscan burst on `Narrative.Input.Attack`.
- `USovGameplayAbility_ReformationDroneRocketLauncher`: a replicated radial-damage rocket on `Narrative.Input.AltAttack`.

Both abilities are intended to be granted by the drone's Narrative **Ability Configuration**. They are integral NPC attacks, not weapon-item abilities, and do not require ammo or an equipped `UWeaponItem`.

## One-time editor setup

Close the editor and perform a full `ProjectVelkorranEditor` build after pulling the implementation. The change adds reflected native classes and native Gameplay Tags; Live Coding is not suitable for the first load.

Create these Blueprint children:

| Suggested asset | Native parent | Purpose |
|---|---|---|
| `GA_ReformationDrone_Gunfire` | `Reformation Drone: Gunfire` | Damage, burst, targeting, timing, montage, and muzzle configuration |
| `BP_ReformationDrone_GunshotPresentation` | `Reformation Drone Gunshot Presentation` | Muzzle flash, tracer, impacts, audio, decals, and camera shakes |
| `GA_ReformationDrone_RocketLauncher` | `Reformation Drone: Rocket Launcher` | Explosion, launch, homing, timing, montage, and muzzle configuration |
| `BP_ReformationDrone_Rocket` | `Reformation Drone Rocket Projectile` | Rocket mesh, trail, flight audio, explosion presentation, decal, and physics impulse |

Set the gun ability's **Gunshot Presentation Class** to the presentation Blueprint. Set the rocket ability's **Rocket Class** to the rocket Blueprint. The native presentation and projectile classes are valid art-free defaults. Gun gameplay also remains functional if its optional presentation class is deliberately cleared.

The shared native **Damage Effect Class** is also Blueprintable. A child may add Gameplay Cues or project metadata, but should retain the instant Narrative damage execution and leave damage magnitude to the ability/projectile SetByCaller payload.

Add both Gameplay Ability Blueprints to the drone's Narrative **Ability Configuration → Default Abilities**. Do not put them in a weapon item's granted ability array. The native defaults already use:

| Ability | Input tag | Identity tag |
|---|---|---|
| Gunfire | `Narrative.Input.Attack` | `Sov.Ability.NPC.ReformationDrone.Gunfire` |
| Rocket launcher | `Narrative.Input.AltAttack` | `Sov.Ability.NPC.ReformationDrone.RocketLauncher` |

Remove inherited placeholder abilities that also use `Narrative.Input.Attack` or `Narrative.Input.AltAttack`. Narrative's input lookup does not provide a useful override guarantee when multiple active specs claim the same slot.

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
- Tune **Payload Release Delay** to the visual fire frame.
- Keep **Auto Release Payload** enabled for normal use.

For an exceptional server-authored sequence, disable automatic release and call `Fire Gun Burst From Aim` or `Launch Rocket From Aim` from the Gameplay Ability Blueprint. Do not call those functions from a cosmetic AnimBP notify; simulated-client notifies are not gameplay authority.

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

## Default gameplay tuning

| Setting | Gunfire | Rocket launcher |
|---|---:|---:|
| Base damage | 12 per shot | 55 at center |
| Poise pressure | 4 per shot | 35 at center |
| Burst | 3 shots, 0.1 s apart | 1 rocket |
| Maximum range | 5,000 cm | 6,000 cm AI attack range; 8,000 cm aim trace |
| Ability cooldown | 0.75 s | 4.0 s |
| Bot attack frequency | 0.9 s | 5.0 s |
| Explosion radius | — | 450 cm |
| Minimum damage at radius edge | — | 30% |
| Rocket speed | — | 2,600 cm/s |
| Flight limit | — | 5.0 s |

These are native-safe starting values, not balance locks. Override them on the ability Blueprints.

## Verification checklist

1. Start with a fresh PIE or Standalone session after granting the two abilities.
2. Confirm both specs appear in `showdebug abilitysystem` on the drone.
3. Confirm the drone and player have hostile Narrative team attitudes.
4. Test gunfire with an empty presentation Blueprint first: Health/Shield/Poise should resolve even without art.
5. Confirm a wall stops both a tracer and its damage.
6. Confirm gunfire hits carry the skeletal bone in `FSovDamageResult`.
7. Test rocket center, edge falloff, cover line of sight, direct hits, homing, expiry, and world collision.
8. Run a two-player listen-server test: each client should see one gun presentation per shot and one rocket impact, with no duplicate audio or decals.
9. Test dedicated server plus clients if available; dedicated authority should run gameplay without constructing cosmetic systems.
10. Swap or rebuild the Narrative appearance, then fire again and verify both montages still play on the current drone AnimInstance.

If activation never begins, first check that the abilities are in the NPC Ability Configuration, the drone has been fully initialized by Narrative, no firing/death/ragdoll/Poise-break/Freeze/device-disable tag is active, and the StateTree is requesting the matching Attack or Alt Attack input.
