# Aurelion selective Chaos destruction

Status: required follow-on phase, queued after the relevant custom meshes and placements are established. No Geometry Collections, destructible placements or combat destruction hooks are implemented by this document.

The user requested Chaos destruction for appropriate cover and nonstructural walls. Finalize each intact asset and placement first, then author and qualify its destructible version before declaring that area complete. Begin with one freestanding cover object and one optional nonstructural panel in a controlled test scene, then expand to campaign placements after live qualification.

## Placement policy

| Role | Intended treatment |
| --- | --- |
| Selected freestanding cover, stone barriers and cargo shells | Clustered destruction with accumulated damage; retire the corresponding intact obstruction when its section breaks. |
| Optional nonstructural partitions and infill | Anchored edges/base, local breakage and an authored opening where the surrounding level supports traversal. |
| Cladding on an essential wall | Breakable surface over a deliberately retained structural core. |
| Structural piers, arches, floors, ceilings and traversal supports | Retain structural geometry and collision; optional cosmetic chips or mounted trim may break. |
| Survivor-protection baffles, mission gates and required route boundaries | Protected core or cosmetic damage until mission-specific acceptance qualifies functional destruction. |

## Repository findings

- The runtime module already depends on Chaos, but neither project module definition currently declares Geometry Collection dependencies. The source search found no dedicated environmental fracture integration.
- Much of the custom art sits over separate native collision actors. Breaking only the visible mesh would leave invisible cover. Every selected placement needs an explicit mapping between visible assembly, intact obstruction and broken-state collision.
- The nineteen custom Z08 cargo instances use a shared noncolliding HISM, recorded in `Art/Source/Aurelion/Z08CargoKit/cargo-baseline.json`. Selected destructible instances need separation without removing unrelated instances or duplicating geometry.
- Refuge shells now own both presentation and original collision envelopes. Their protection, survivor-clearance and access checks remain requirements; non-load-bearing appearance alone does not make them safe to destroy during the rescue sequence.
- The August TDD requires destruction in checkpoint snapshots and specifies checkpointable state groups. This phase must integrate with that save design.

## Implementation requirements

1. Author Geometry Collections from finalized custom meshes, including clustered pieces, believable fracture interiors, material-specific breakage and anchored sections. Keep tiny hardware attached to larger fragments rather than simulating every pin. Intact pieces must be stable at spawn.
2. Inspect existing native combat paths and connect actual weapon impacts, melee and explosions to one admitted destruction response. Add the smallest required integration; do not duplicate character GAS payloads in Blueprint or assume character damage receipts automatically reach scenery.
3. Tune local damage and strain for stone, ceramic and metal. Light impacts should not demolish every object. Add matching dust, chips, impacts and break sounds.
4. Retire the correct intact collision as sections break. Update affected navigation and cover/line-of-sight state. Small debris must not trap players, companions or survivors.
5. Bound active fragments, sleeping, lifetime and cleanup. Profile simultaneous combat destruction, including the Geometry Collection representation of detailed Nanite source meshes.
6. Save authored intact/broken state through checkpointable groups with stable placement identities. Restore visuals, obstruction and navigation consistently without relying on replaying the previous physics simulation.

## Acceptance before rollout

- Actual protagonist and enemy attacks break eligible assets through the real combat path, with proportionate local response and no duplicate damage response.
- Intact cover blocks the intended shots and movement. Traces and live player/companion traversal confirm the broken opening has no invisible original collider.
- Protected structures and refuge/mission geometry survive the same test attacks as authored.
- Fractured interiors, fragments, dust and sound look appropriate; intact and broken versions do not overlap or flicker.
- Checkpoint reload before and after destruction restores the correct state without resetting unrelated mission progress.
- Worst-case nearby destruction meets measured campaign performance and debris budgets before broad rollout.

The existing 68 architecture checks establish static fit only. They do not qualify Chaos destruction, live combat or save/load behavior for this phase.
