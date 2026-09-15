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

The existing 73 architecture checks establish static fit only. They do not qualify Chaos destruction, live combat or save/load behavior for this phase.

## Collision ownership survey

`Scripts/Editor/audit_aurelion_destruction_candidates.py` surveys all nineteen finalized cargo visuals against collision-enabled primitive components in the saved M12 map. It records component paths, transforms, world bounds, Pawn/Visibility responses and two horizontal simple-collision probes for each overlapping component. It first runs the existing cargo fit check, makes no map changes, and asserts a clean map before and after execution.

An overlapping bounding box does not prove that a collider belongs to the cargo: floors, shared barriers, triggers and unrelated geometry can overlap. These rows are review evidence, not automatic removal instructions. HISM instance indices are source references, not stable checkpoint identities. None of these candidates is approved for campaign destruction until its actual obstruction ownership, stable identity and gameplay role are qualified.

The inspected drone point-damage path (`USovGameplayAbility_ReformationDroneWeaponBase::ApplyPointDamage`) rejects targets without a valid hostile ability system. A Geometry Collection alone therefore does not supply the required damage integration for this attack. This source finding is not a live combat qualification or evidence that every weapon shares that path.

Verified run: `DestructionOwnershipVerified-20260914-202435-08e54112`, terminal exit 0, no Python errors, 3140 actors and clean map. Full measured output is `Docs/AurelionDestructionCandidates-2026-09-14.json`.

The nineteen visuals overlap ten native cover components: six `Z08_LC_*` and four `Z08_HC_*` actors. Nine cover actors span two cargo visuals each; `Z08_HC_NorthMid` spans one. Both horizontal component probes report contact for each cover overlap. Independent per-crate destruction must therefore split the shared obstruction or intentionally fracture both visuals as one authored state group. The sky sphere also overlaps the broad-phase bounds, and instance 0 overlaps the E4 entry trigger; neither is cargo collision ownership. Preserve those unrelated components.
