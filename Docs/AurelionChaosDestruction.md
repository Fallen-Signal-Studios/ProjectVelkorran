# Aurelion selective Chaos destruction

Status: an isolated cargo Geometry Collection and solver test are implemented under `/Game/Aurelion/ArtReview/Chaos`. Campaign destructible placements and combat destruction hooks are not implemented yet.

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

The architecture checks establish static fit only. They do not qualify Chaos destruction, live combat or save/load behavior for this phase.

## Collision ownership survey

`Scripts/Editor/audit_aurelion_destruction_candidates.py` surveys all nineteen finalized cargo visuals against collision-enabled primitive components in the saved M12 map. It records component paths, transforms, world bounds, Pawn/Visibility responses and two horizontal simple-collision probes for each overlapping component. It first runs the existing cargo fit check, makes no map changes, and asserts a clean map before and after execution.

An overlapping bounding box does not prove that a collider belongs to the cargo: floors, shared barriers, triggers and unrelated geometry can overlap. These rows are review evidence, not automatic removal instructions. HISM instance indices are source references, not stable checkpoint identities. None of these candidates is approved for campaign destruction until its actual obstruction ownership, stable identity and gameplay role are qualified.

The inspected drone point-damage path (`USovGameplayAbility_ReformationDroneWeaponBase::ApplyPointDamage`) rejects targets without a valid hostile ability system. A Geometry Collection alone therefore does not supply the required damage integration for this attack. This source finding is not a live combat qualification or evidence that every weapon shares that path.

Verified run: `DestructionOwnershipVerified-20260914-202435-08e54112`, terminal exit 0, no Python errors, 3140 actors and clean map. Full measured output is `Docs/AurelionDestructionCandidates-2026-09-14.json`.

The nineteen visuals overlap ten native cover components: six `Z08_LC_*` and four `Z08_HC_*` actors. Nine cover actors span two cargo visuals each; `Z08_HC_NorthMid` spans one. Both horizontal component probes report contact for each cover overlap. Independent per-crate destruction must therefore split the shared obstruction or intentionally fracture both visuals as one authored state group. The sky sphere also overlaps the broad-phase bounds, and instance 0 overlaps the E4 entry trigger; neither is cargo collision ownership. Preserve those unrelated components.

## Cargo prototype, 2026-09-14

`DF_Aurelion_CargoPrototype` converts the finalized Z08 stores mesh, applies a deterministic twelve-region Voronoi fracture (seed 914), and writes `GC_Aurelion_CargoPrototype`. Disconnected islands are not split into individual simulated fasteners. The generated collection has thirteen transforms (root plus twelve pieces), three source materials and two UV layers. The stored geometry includes 87,547 faces including fracture geometry and the parent representation; this is not a measured runtime triangle or performance budget.

`L_Aurelion_ChaosPrototype` contains two independent collections and a floor, with a base engine game mode. No M12 actor or collider is replaced. `validate_aurelion_chaos_prototype.py` exercises settling, low strain and high strain in Simulate In Editor. Deliberate strain is an isolated physics test, not an actual weapon damage receipt.

The first trial used a 5,000 threshold and both objects broke while settling from 5 cm. Its cleanup also used an unsupported Python method; that owned editor process was stopped and cleanup corrected. Raising the threshold to 500,000 at 120 kg total mass passed the next trial (`ChaosPrototypeStrength-20260914-231445-5e54464a`, exit 0, no Python errors): both roots stayed intact for ten seconds, 100 strain did not break the target, and 100,000,000 strain broke only the target. These deliberately separated values establish solver response; they are not balanced weapon tuning.

The Dataflow and Geometry Collection are authored through existing engine Python APIs, without runtime C++ changes. Source inspection found no `ISovCheckpointable` or `USovWorldStateSubsystem` implementation under `Source`, although the August TDD describes those interfaces. Campaign rollout therefore still needs a concrete save-state integration through the existing save architecture, plus real scenery-damage admission, collision retirement, navigation, debris cleanup, matching effects and audio. Do not treat the prototype as completed campaign destruction.

Fresh saved-scene run `ChaosPrototypeMotion-20260914-232645-e6bee05f` completed with exit 0 and no Python errors. It regenerated the saved recipe into a transient collection and repeated the solver checks using the saved collection and placements. After the break, the test applied a radial impulse and explicit alternating per-piece velocities; all twelve fragment sockets moved 58.4-612.8 cm while the control root stayed intact. Earlier strain-only and radial-impulse captures left the crate largely assembled, so a convincing local impact response still needs authoring and qualification. The final displacement is evidence of simulated fragments, not balanced explosion behavior.

SceneCapture2D images were inspected for the final run. The target visibly separates into large pieces while the control remains assembled. Fracture interiors are currently flat and large; material-specific ceramic and metal breakage, smaller cosmetic chips, dust and audio are unfinished. The initial high-resolution viewport captures were black and are excluded from visual evidence.

Review evidence is retained in `Docs/Validation/AurelionChaosPrototype-2026-09-14/` (intact/fractured images, recipe, solver and fragment-motion reports). Run `Scripts/Editor/verify_aurelion_chaos_saved.py` through `Scripts/Validation/Aurelion/run-editor-script.ps1` with the existing architecture review startup map to repeat the isolated test. The validator loads the dedicated Chaos map without rebuilding it, creates temporary capture actors, enters simulation and exits without saving those temporary actors or simulated state.

## Existing save integration, 2026-09-15

The absence of the interface names proposed in the TDD does not mean the project lacks world saving. `USovSaveSubsystem` wraps the configured Narrative serializer; `USovCampaignSaveGame` explicitly stores its payload rather than maintaining a second actor snapshot. The installed Narrative save system exposes `INarrativeSavableActor` and `INarrativeSavableComponent`, stable actor GUIDs, `PrepareForSave` and `Load` events, and ordered restoration. Its serializer sets `ArIsSaveGame` for actors and eligible components. Component serialization requires a savable owner.

Destruction should use that existing authority: a stable placed owner with authored intact/broken state marked for saving, restoring the corresponding presentation and obstruction in its load event. Do not add a parallel world-state subsystem merely to reproduce the TDD's interface names. A collection's transient solver positions are not a suitable replacement for that state. This is a source-verified integration direction, not an implemented or tested destruction save record.
