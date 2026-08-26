# Sovereign Dismemberment System

Project Velkorran now supports server-authoritative, bone-defined dismemberment on Narrative modular characters. The system resolves a sever from the same ordered damage result used by Guard, Shield, Health, Poise, and death, then replicates a compact permanent region mask. Narrative Save serializes that same mask for savable NPCs. Detached limbs, blood effects, decals, and stump actors are local cosmetics driven by one reliable multicast.

## First setup

1. Reparent an enemy Blueprint to `SovNPCCharacterBase`, or add `SovDismembermentComponent` directly to an existing Narrative character Blueprint.
2. Leave `Use SK Mannequin Bone Map` enabled. No dismemberment profile is required for the standard Epic mannequin skeleton.
3. Create and assign a `SovDismembermentProfile` only when the character needs custom detached limbs, stumps, Niagara effects, decals, rules, or explicit mapping overrides.
4. Ensure damaging Gameplay Effects include `Sov.Damage.Channel.Edge` and carry a valid `FHitResult` in their effect context. Narrative already copies `HitResult.BoneName` into the resolved damage packet.

Without custom rules, the safe fallback policy requires all of the following:

- A fatal hit
- Edge damage
- At least 25 points of applied Health damage
- No successful Guard or Perfect Defense

The thresholds are editable on the component.

## SK Mannequin bone map

`Use SK Mannequin Bone Map` is enabled by default and supplies all standard biped sever regions. It also repairs missing or invalid core bone fields in an existing profile and adds any standard regions that the profile omitted. Region-specific art and gameplay settings are copied from the profile unchanged.

New `SovDismembermentProfile` assets start with the complete map already populated. For an existing profile, use its `Apply SK Mannequin Bone Map` editor button to write the canonical mapping into the asset while preserving its authored cosmetic payloads and sever rules. The component's runtime auto-fill still protects older assets that have not been updated manually.

| Region | Hit bone root | Hidden branch | Surviving stump bone |
| --- | --- | --- | --- |
| Head | `head`, `neck_01` | `head` | `neck_01` |
| Left upper arm | `upperarm_l`, `clavicle_l` | `upperarm_l` | `clavicle_l` |
| Right upper arm | `upperarm_r`, `clavicle_r` | `upperarm_r` | `clavicle_r` |
| Left forearm | `lowerarm_l` | `lowerarm_l` | `upperarm_l` |
| Right forearm | `lowerarm_r` | `lowerarm_r` | `upperarm_r` |
| Left hand | `hand_l` | `hand_l` | `lowerarm_l` |
| Right hand | `hand_r` | `hand_r` | `lowerarm_r` |
| Left upper leg | `thigh_l` | `thigh_l` | `pelvis` |
| Right upper leg | `thigh_r` | `thigh_r` | `pelvis` |
| Left lower leg | `calf_l` | `calf_l` | `thigh_l` |
| Right lower leg | `calf_r` | `calf_r` | `thigh_r` |
| Left foot | `foot_l` | `foot_l` | `calf_l` |
| Right foot | `foot_r` | `foot_r` | `calf_r` |

Finger, toe, twist, and other child-bone hits present on the driver skeleton resolve through its hierarchy to the closest configured region. For example, a finger hit resolves to the hand instead of the forearm, while a calf twist-bone hit resolves to the lower leg.

Disable `Use SK Mannequin Bone Map` only for a genuinely different skeleton whose regions are fully authored in its profile or in `Fallback Regions`. `Custom A` and `Custom B` are never auto-filled.

## Authoring a profile

Each region defines:

- `Hit Bone Roots`: a hit on that bone or any descendant maps to this region.
- `Bone To Hide`: the complete skeletal branch collapsed by Unreal.
- `Stump Attach Bone`: the surviving parent bone or an authored stump socket.
- `Physics Body Operation`: normally `Terminate` for lethal dismemberment.
- `Presentation Slots To Hide`: rigid Narrative armor pieces that cannot collapse with a skeletal branch.
- `Detached Limb Class`: a Blueprint derived from `SovDetachedLimbActor`.
- `Stump Actor Class`: a local cosmetic actor containing cap geometry and wound materials.
- `Stump Niagara Slots`: stateful Niagara effects attached to the region's stump bone or an optional per-slot bone/socket override.
- `Sever System` and `Blood Decal Material`: Niagara and Deferred Decal gore presentation.
- `Blood Decal Surface Search Distance`: how far the cosmetic searches for nearby ground, walls, ceilings, or movable world geometry instead of projecting onto Narrative's decal-disabled modular meshes.

The blood material must use the Deferred Decal domain. Multiply opacity by `Decal Lifetime Opacity` if the authored fade duration should be visible instead of ending with a pop.

`Disable Collision` keeps branch bodies allocated but turns off their collision. `Terminate Permanently` removes them and is the default for this lethal-only first slice. Physics operations are applied once per local mesh physics state, even if Narrative emits several asynchronous appearance callbacks. A complete base-appearance replacement clears that local guard so the operation is applied to the replacement physics asset.

For decapitation, enable `Hide Head Presentation`. Narrative will hide the face, helmet, facial meshes, all grooms, and supported static head pieces.

The stump actor is spawned at the sever transform before the bone is hidden, then attached to the surviving parent with `Keep World Transform`. This lets an authored cap stay aligned during animation and ragdoll. Use the region spawn offsets to correct an asset whose pivot does not match the skeleton.

## Stump Niagara slots

Use `Sever System` for the transient burst that plays when the hit first severs the limb. Use `Stump Niagara Slots` for effects that belong to the persistent wound, such as a looping blood mist, embers, smoke, or leaking energy. Each region can contain multiple independently authored slots:

- `Slot Name`: editor-facing label for the array element.
- `Niagara System`: the effect to spawn.
- `Attach Bone Override`: optional bone or socket; empty uses the region's `Stump Attach Bone`.
- `Spawn Offset`: an offset from the exact sever transform before attachment.
- `Auto Destroy`: enable for a finite effect; leave disabled for a persistent looping effect.

Stump effects spawn once per region on each local game instance and attach with their world cut transform preserved. They are reconstructed from the permanent sever mask for late joiners and save loads, but Narrative appearance refreshes do not duplicate them. A missing override falls back to the region stump bone; a missing region stump bone leaves the effect attached to the driver mesh at the resolved sever position.

## Detached limb Blueprints

Create one child Blueprint of `SovDetachedLimbActor` per required limb/body form. Assign a skeletal mesh and physics asset to its inherited `Limb Mesh`. The base actor:

- Simulates locally instead of replicating continuous ragdoll physics.
- Uses the `Ragdoll` collision profile by default.
- Receives an impulse based on resolved damage.
- Destroys itself after 12 seconds by default.
- Exposes `Detached Limb Initialized` for blood trails, sound, material changes, or additional effects.

## Additional sever rules

Profile rules are OR conditions. Any matching rule permits a sever. Examples:

- Sword execution: Edge, fatal, 25 applied Health damage.
- Heavy kinetic overkill: Kinetic, Heavy classification, fatal, 35 overkill damage.
- Explosive hit: Kinetic plus Thermal with `Require All Damage Channels`, fatal, 20 overkill damage.

An empty required-channel container means the rule accepts any damage channel. Guarded and perfect-defense hits are rejected before rules are evaluated.

## Narrative appearance behavior

The component applies every severed branch to Narrative's hidden leader mesh and local first-person leader mesh. Leader-pose body and armor pieces inherit that transform from their leader; independently animated modular meshes are updated directly. This avoids corrupting follower skinning while keeping every presentation layer synchronized. It also listens for:

- A replacement `NarrativeCharacterVisual`
- Base appearance completion
- Every later asynchronous modular mesh change

The replicated state is therefore reapplied after clothing swaps, armor changes, appearance replacement, Narrative death/ragdoll transitions, save loading, and late joining. This state belongs to the real character component, not the transient appearance actor. The server applies each new sever once through the same multicast path used by clients, avoiding a second irreversible `PBO_Term` pass on listen servers.

## Narrative Save and revive behavior

`SovDismembermentComponent` implements `NarrativeSavableComponent`, and `Severed Region Mask` is marked `SaveGame`. Narrative will therefore preserve severed regions whenever the owning actor implements `NarrativeSavableActor`, as `NarrativeNPCCharacter` already does.

Dismemberment is intentionally permanent for the lifetime and save record of that actor. Narrative's `Revive` restores attributes and exits ragdoll, but it does not grow limbs back. If an encounter needs resurrection with restored anatomy, respawn a fresh actor or provide a project-specific regeneration flow rather than using `Terminate Permanently`.

## Runtime validation

At startup the component warns about duplicate or invalid regions, bones or stump sockets missing from the driver mesh, empty bone mappings, duplicate hit roots, empty stump Niagara slots, inverted impulse limits, and non-fatal rules paired with permanent physics termination. A sever is rejected instead of committing replicated state when its configured hide bone does not exist on Narrative's driver mesh.

## Blueprint hooks

- `On Limb Severed`: fires on the server and participating clients with region, hit bone, and sever location.
- `On Dismemberment State Changed`: fires when the replicated permanent mask changes.
- `Force Sever Region`: authority-only path for executions and scripted deaths.
- `Refresh Dismemberment Visuals`: manual recovery hook after any custom presentation change outside Narrative's normal mesh pipeline.
- `Is Region Severed` and `Get Severed Regions`: query permanent state.

The initial scope intentionally uses authored sever points. It does not perform arbitrary runtime mesh slicing.

## PIE verification matrix

Before shipping a new character profile, verify:

1. A fatal Edge hit with a valid bone hides the correct branch, creates one stump and one detached limb, attaches every configured stump Niagara slot, and leaves no stretched leader-pose geometry.
2. Guarded and Perfect Defense hits never sever. Shield-only hits sever only when an authored custom rule explicitly permits them.
3. A listen server and remote client each play one cosmetic event. The server must not terminate the same body twice.
4. Equipping or asynchronously replacing armor after a sever keeps the branch and configured rigid slots hidden.
5. A late-joining client reconstructs hidden branches and stumps from the replicated mask without replaying old transient blood or limb cosmetics.
6. Saving and loading a savable NPC restores the permanent sever mask.
7. Narrative death, ragdoll, and revive transitions preserve the sever. `Disable Collision` regions remain disabled after Narrative's mesh-wide collision changes.
8. Stump Niagara effects follow the configured stump bone during animation and ragdoll, and outfit or appearance refreshes do not create duplicates.
9. Hits on upper arms, forearms, hands/fingers, thighs, calves, feet/toes, neck, and head resolve to the closest matching SK Mannequin region on both sides.
