# Combat Sustain pickups and Cinderline damage

This integration adds short-lived, automatic ammo and Echo rewards to player-caused hostile kills, plus deterministic distance-based damage variation for ordinary Cinderline fire. Gameplay transactions run on authority; Blueprint children own meshes, Niagara, audio, and tuning. Pickups use a small replicated physics body so they fall from their spawn offsets and settle on the ground.

## Design decision

Combat Sustain is an explicit carve-out from the TDD's **no loot drops** rule. These actors are combat feedback, not a loot economy:

- collection is an automatic pawn overlap, with no interact prompt or inventory choice;
- the reward is a fixed authored combat resource, with no rarity, equipment roll, vendor value, or loot table;
- the pickup actor is transient and implements no Narrative save interface;
- an uncollected pickup expires after its authored lifetime; and
- encounter/checkpoint reset owns whether the kill and its reward opportunity are recreated.

The collected ammo becomes part of Narrative's normal authoritative inventory state, and collected Echo uses `USovEchoComponent`. The temporary world actor itself is never saved.

This pass also resolves the TDD's open Cinderline pacing decision: **Cinderline uses a magazine plus carried reserve, not heat**. `UWeaponItem::ClipSize` is the magazine and the quantity of its configured `UAmmoItem` is the total carried ammunition pool.

Cinderline damage remains mastery-readable. Variation is based on confirmed hit distance, not a random critical roll. The weapon's existing physical-material hit-zone multiplier still determines precision damage.

## Create the pickup presentation Blueprints

Create two Blueprint actor classes:

1. `BP_CinderlineAmmoPickup`, parented to `ASovAmmoCombatSustainPickup`.
2. `BP_EchoCombatSustainPickup`, parented to `ASovEchoCombatSustainPickup`.

In each Blueprint:

- select the inherited **Pickup Mesh** component and assign the desired static mesh;
- optionally configure the inherited **Idle Niagara** and **Idle Audio** components;
- assign **Collection Niagara System** and **Collection Sound** for the successful pickup burst;
- tune **Pickup Radius** (`70 cm` by default), **Ground Collision Radius** (`12 cm`), damping, **Pickup Lifetime Seconds** (`20 s`), and rotation under `Sovereign | Combat Sustain`; and
- use `On Pickup Collected` only for extra presentation. Do not add ammo, Echo, collision, replication, or destruction logic in Blueprint.

The native actor's small physics sphere blocks world geometry while its larger trigger overlaps only pawns. It accepts `ASovPlayerCharacterBase`, grants once on the server, replicates its movement and claimed state, and cleans itself up after the collection presentation. Vertical hover is intentionally disabled so the mesh remains grounded after settling. If the player cannot accept the reward, the pickup remains available until its lifetime expires. This includes a full ammo stack or a full Echo meter.

## Configure Cinderline ammunition

Create `BP_Ammo_Cinderline` from `UAmmoItem` and set:

- **Stackable**: enabled
- **Max Stack Size**: the maximum total carried rounds, including rounds currently represented in the magazine

Narrative calculates displayed spare ammo as `Ammo item quantity - Ammo in clip`. For example, a `30`-round magazine and `Max Stack Size = 240` produce `210` spare rounds when completely full. Combat Sustain treats that maximum as a total cap for the matching ammo class, and Narrative inventory capacity now allows a partial existing stack to be topped up even when every inventory slot is occupied.

On the Cinderline `URangedWeaponItem` asset set:

- **Required Ammo**: `BP_Ammo_Cinderline`
- **Clip Size**: the intended magazine size, such as `30`
- **Allow Manual Reload**: enabled

The ordinary Cinderline fire ability must require and consume ammo through Narrative's existing weapon path. Give the player's starting loadout at least one `BP_Ammo_Cinderline` stack so the weapon can initialize its ammo source.

## Configure enemy drops

Every Blueprint derived from `ASovNPCCharacterBase` inherits **Sov Combat Sustain Drop Component**. Select it and configure the archetype:

| Setting | Recommended starting value |
|---|---:|
| Combat Sustain Drops Enabled | `true` for combat enemies |
| Drop Ammo | `true` |
| Ammo Pickup Class | `BP_CinderlineAmmoPickup` |
| Ammo Item Class | `BP_Ammo_Cinderline` |
| Ammo Amount | `5` |
| Ammo Spawn Offset | `(0, 30, 35)` |
| Drop Echo | `true` for intended Echo-reward enemies |
| Echo Pickup Class | `BP_EchoCombatSustainPickup` |
| Echo Amount | `1` ordinary, `2` elite |
| Echo Spawn Offset | `(0, -30, 35)` |

Class fields intentionally default to empty, so source integration cannot begin spawning artless pickups before content is assigned. Disable either reward independently for archetypes that should not provide it. The current policy is deterministic: each configured reward spawns once after an authoritative, player-caused fatal hit against a hostile NPC. Revival or encounter recycling opens one new reward opportunity; repeated fatal callbacks cannot duplicate a drop.

Echo pickup grants are attributed with the native tag `Sov.Echo.Source.CombatSustainPickup`.

This reward is independent of Tarrik's existing Cinderline Cadence and fatal-precision Echo awards. A configured precision kill can therefore grant both the native precision payout and a `1`-Echo mote. Disable or retune the per-NPC Echo drop if that combined payout is too generous for an encounter.

## Configure Cinderline damage

On the Cinderline `URangedWeaponItem` asset, open `Item - Weapon | Attack Settings | Damage Variation` and set:

| Setting | Value |
|---|---:|
| Damage Variation Mode | `Distance Based` |
| Minimum Attack Damage | `10` |
| Maximum Attack Damage | `15` |
| Damage Variation Near Distance | close-range full-damage boundary, in cm |
| Damage Variation Far Distance | long-range minimum-damage boundary, in cm |

A useful first pass is `1000 cm` near and `5000 cm` far. Hits at or inside the near boundary deal `15`; hits at or beyond the far boundary deal `10`; hits between them interpolate linearly. Tune those distances to actual encounter scale rather than changing the endpoint damage first.

Keep the Cinderline weapon's fixed **Attack Damage** at its normal fallback value. `Fixed` remains the backwards-compatible default for every other weapon.

Add `Sov.Ability.Weapon.Cinderline.PrimaryFire` to the ordinary fire ability's **Asset Tags**. The implementation also recognizes Narrative's standard ranged `Attack + WeaponFire` route, but the native Cinderline tag is the stable explicit identity. Do not put it on Cinder Judgement, Requiem, grenades, Burn, or shared effects.

The distance result is selected before the normal execution modifiers. With the precision physical material set to `Damage Multiplier = 2.0`:

- ordinary hits resolve from `10` to `15`; and
- precision hits resolve from `20` to `30`.

The variation applies only to a blocking ordinary ranged hit. A damage spec with an explicit `SetByCaller.Damage` magnitude or an `Sov.Ability.Echo` classification keeps its authored fixed damage. Shield, Guard, Health, Poise, difficulty, armor, resistance, and other execution behavior remain unchanged.

Telemetry caveat: for captured `AttackDamage` attacks without `SetByCaller.Damage`, the current `FSovDamageResult.BaseDamage` fallback reports the resolved incoming magnitude, including range, hit-zone, and mitigation modifiers. It is not a true pre-formula base value. Gameplay damage and routing are correct; do not use that field to chart raw Cinderline falloff until a separate pre-formula telemetry value is added.

## Validation pass

1. Kill a hostile NPC with the player and confirm each configured pickup appears exactly once, falls to the floor, and settles at the same location on server and clients.
2. Walk over both actors. Confirm ammo enters only the matching reserve and Echo reports the `CombatSustainPickup` source.
3. Fill the matching ammo stack and Echo meter, then confirm the corresponding pickup remains unclaimed.
4. Partially fill the ammo stack, collect a pack larger than the remaining capacity, and confirm only the unaccepted remainder stays in the world.
5. Let each pickup sit for `20` seconds and confirm it expires without a save record.
6. Revive or recycle an NPC and confirm its next valid death can drop once again.
7. Fire Cinderline at, inside, between, and beyond the two distance boundaries. Confirm body damage is `15`, interpolated, and `10` before mitigation.
8. Repeat against the `2.0` precision physical material and confirm `30`, interpolated, and `20`.
9. Confirm ordinary non-Cinderline weapons remain fixed, and Cinder Judgement, Requiem, Burn, and other explicit `SetByCaller.Damage` effects do not inherit primary-fire variation.
10. Repeat in listen-server and client PIE, then run a Development Editor build.
