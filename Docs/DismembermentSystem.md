# Sovereign Dismemberment System

Project Velkorran now supports server-authoritative, bone-defined dismemberment on Narrative modular characters. The system resolves a sever from the same ordered damage result used by Guard, Shield, Health, Poise, and death, then replicates a compact permanent region mask. Detached limbs, blood effects, decals, and stump actors are local cosmetics driven by one reliable multicast.

## First setup

1. Reparent an enemy Blueprint to `SovNPCCharacterBase`, or add `SovDismembermentComponent` directly to an existing Narrative character Blueprint.
2. The component includes working Epic-human mappings for `head`, `lowerarm_l`, `lowerarm_r`, `calf_l`, and `calf_r`.
3. Create a `SovDismembermentProfile` Data Asset when a character uses different bone names or needs its own art.
4. Assign that profile on the component.
5. Ensure damaging Gameplay Effects include `Sov.Damage.Channel.Edge` and carry a valid `FHitResult` in their effect context. Narrative already copies `HitResult.BoneName` into the resolved damage packet.

Without custom rules, the safe fallback policy requires all of the following:

- A fatal hit
- Edge damage
- At least 25 points of applied Health damage
- No successful Guard or Perfect Defense

The thresholds are editable on the component.

## Authoring a profile

Each region defines:

- `Hit Bone Roots`: a hit on that bone or any descendant maps to this region.
- `Bone To Hide`: the complete skeletal branch collapsed by Unreal.
- `Stump Attach Bone`: the surviving parent bone or an authored stump socket.
- `Physics Body Operation`: normally `Terminate` for lethal dismemberment.
- `Presentation Slots To Hide`: rigid Narrative armor pieces that cannot collapse with a skeletal branch.
- `Detached Limb Class`: a Blueprint derived from `SovDetachedLimbActor`.
- `Stump Actor Class`: a local cosmetic actor containing cap geometry and wound materials.
- `Sever System` and `Blood Decal Material`: Niagara and Deferred Decal gore presentation.

The blood material must use the Deferred Decal domain. Multiply opacity by `Decal Lifetime Opacity` if the authored fade duration should be visible instead of ending with a pop.

`Disable Collision` keeps branch bodies allocated but turns off their collision. `Terminate Permanently` removes them and is the default for this lethal-only first slice.

For decapitation, enable `Hide Head Presentation`. Narrative will hide the face, helmet, facial meshes, all grooms, and supported static head pieces.

The stump actor is spawned at the sever transform before the bone is hidden, then attached to the surviving parent with `Keep World Transform`. This lets an authored cap stay aligned during animation and ragdoll. Use the region spawn offsets to correct an asset whose pivot does not match the skeleton.

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

The component applies every severed branch to Narrative's hidden leader mesh, visible modular skeletal meshes, and local first-person meshes. It also listens for:

- A replacement `NarrativeCharacterVisual`
- Base appearance completion
- Every later asynchronous modular mesh change

The replicated state is therefore reapplied after clothing swaps, armor changes, appearance replacement, and late joining. This state belongs to the real character component, not the transient appearance actor.

## Blueprint hooks

- `On Limb Severed`: fires on the server and participating clients with region, hit bone, and sever location.
- `On Dismemberment State Changed`: fires when the replicated permanent mask changes.
- `Force Sever Region`: authority-only path for executions and scripted deaths.
- `Refresh Dismemberment Visuals`: manual recovery hook after any custom presentation change outside Narrative's normal mesh pipeline.
- `Is Region Severed` and `Get Severed Regions`: query permanent state.

The initial scope intentionally uses authored sever points. It does not perform arbitrary runtime mesh slicing.
