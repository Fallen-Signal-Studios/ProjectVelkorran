# Aurelion selective Chaos destruction

Status: the isolated cargo Geometry Collection now has a native damageable cover owner, a drone gunfire connection, saved intact/broken state, and bounded Chaos debris. Campaign placements, protagonist attack connections and nonstructural panels remain unfinished.

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

- The initial survey found a Chaos dependency but no Geometry Collection dependency or dedicated environmental fracture integration. The native cover implementation below adds these dependencies.
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

Before the native cover change, the inspected drone point-damage path (`USovGameplayAbility_ReformationDroneWeaponBase::ApplyPointDamage`) rejected targets without a valid hostile ability system. A Geometry Collection alone still does not supply damage admission; the new typed cover owner is the explicit exception. This does not imply that every weapon shares that path.

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

## Explicit cover groups, 2026-09-15

`Scripts/Validation/Aurelion/build_destruction_groups.py` now validates the ten reviewed obstruction groups against the collision survey. `Docs/AurelionDestructionGroups-2026-09-15.json` records all nineteen visual members, exact obstruction component paths, transforms and bounds, excluded unrelated overlaps, and reserved deterministic placement GUIDs. These GUIDs are not yet assigned to Narrative savable actors. Campaign destruction remains disabled.

Nine groups must break both cargo visuals and retire their shared obstruction together. `Z08_HC_NorthMid` (source instance 16) has one visual and one obstruction, so it is the first integration candidate. This grouping avoids changing a shared collider while another member still looks intact. Sky and trigger overlaps are explicitly excluded. Nonstructural panels still require a separate ownership and mission-role survey.

## Native cover owner, 2026-09-15

`ASovDestructibleCover` owns an intact visual and box obstruction for one authored group. Destruction requires explicit opt-in, an authored placement GUID and a fractured collection. Finite positive damage accumulates against 120 health by default. The break retires the intact visual, shot/movement collision and navigation relevance together. Further hits do not repeat the break. Structural actors do not opt in automatically.

The existing drone gunfire point-damage path now admits this exact owner type before character ASC targeting. An intercepted shot follows either scenery damage or character GAS damage, never both. Other attacks, including protagonist weapons, melee and explosions, are not connected by this change.

The actor uses Narrative's existing stable actor/save interfaces and Structure restore phase. Remaining health and broken state are SaveGame properties. Loading reconciles collision and presentation and removes transient debris without replaying break effects. The automated serializer round-trip uses the same `ArIsSaveGame` and `ArNoDelta` flags as Narrative. This is not yet a full campaign checkpoint reload qualification.

Debris is created only on a break, uses an impact-directed impulse, ignores characters and queries, and is removed after six seconds (hard maximum ten). A collection with more than 65 transforms is excluded from cosmetic debris spawning; the authored prototype has one root and twelve fragments. This is a per-object bound, not a profiled simultaneous-destruction budget. Optional Niagara and sound fields are exposed but have no authored assignments yet. Client break presentation and network qualification remain open.

Runtime component registration must precede explicit simulation startup. The initial implementation created its solver proxy too early; merely checking `IsRootBroken` missed that defect. The corrected validator requires at least eight fragments to move more than 10 cm as well as testing damage, collision retirement, control preservation and cleanup.

`NativeCoverRendered-20260915-112203-601c1fd2` completed with exit 0 and no Python errors. All twelve fragments moved 12.1-211.5 cm, the control stayed intact, and debris cleared within the authored lifetime. Intact/fractured/cleared captures were taken from the piloted simulation view. Earlier captures without a piloted viewport were stale and are excluded from visual evidence. The three cargo materials now persist Geometry Collection usage flags. The fractured object visibly collapses into large chunks; flat interior faces and material-specific fracture detail still need art work.

The two new native automation tests cover protected defaults, invalid damage, accumulation, repeated hits, both saved states, collision/navigation flags, and the real drone muzzle trace with a character behind the cover. Build and test reports are recorded alongside the isolated simulation evidence. No M12/M13 map or cargo HISM was changed. Campaign enablement still requires replacing the entire reviewed obstruction group, real protagonist attacks, traversal/navigation checks, full checkpoint reload, effects/audio and performance qualification.

After merging `origin/main` at `ac3cc446` into this content branch (`4e30965a`), the Windows editor build and all ten `ProjectVelkorran.World` tests passed in `20260915-112522-6e163d77`. All sixteen drone continuation tests passed in `20260915-112808-d7e0f73f`, with source integrity unchanged during each validation. Existing test warnings concerned Busy-tag replication, a crowd-manager/Recast fixture, and ordinary drone-death logging; there were no failed tests. Packaged/game-target builds were not run.

Fresh merged-build simulation `NativeCoverMerged-20260915-112919-bcf7c07d` exited 0 with no Python errors or missing Geometry Collection material-usage warnings. All twelve fragments moved 12.1-211.6 cm; control preservation and cleanup passed again. Its inspected images and reports replace the earlier evidence in `Docs/Validation/AurelionNativeCover-2026-09-15`. The overall 90% TDD goal is not qualified by these checks.

Fresh editor run `DestructionGroups-20260915-103551-a06a2876` completed with exit 0, no Python errors, 3140 actors and a clean map. The grouping validator passed against that fresh survey: member bounds tile each obstruction envelope within 0.1 cm and all reviewed components block Pawn and Visibility. Fault injection correctly rejected a missing collider, a collider spanning an extra visual, a gap in the visual envelope, and duplicated instance identities. This is static ownership validation, not combat, traversal, checkpoint or destruction-performance qualification.

## Protagonist and explosive damage admission, 2026-09-15

Only drone gunfire could damage `ASovDestructibleCover` before this change. The customized Narrative plugin cannot see game-module classes, so the admission point lives there: `ISovEnvironmentDamageable` marks an authored scenery owner, and `SovEnvironmentDamage::ApplyPoint`/`ApplyRadial` are the single path from character combat into scenery. The helpers require an authoritative source, reject anything carrying an ability system, and still leave opt-in, placement identity and health to the owner. The cover owner implements the marker; drone gunfire keeps its existing explicit admission.

Connected paths:

| Path | Admission | Ordering |
| --- | --- | --- |
| Narrative target data (`ApplyGameplayEffectSpecToTargetData`), used by ordinary hitscan fire | Point, spec `SetByCaller.Damage` | Scenery hit results only; character specs unchanged |
| Native melee sweep environment contact | Point, node damage x charge scalar | Deferred to the end of the sweep step so a break cannot open later samples of the same blade motion |
| Selene Wake/Dispatch projectile wall hit | Point, base damage, once per projectile | After character hits in the segment; the segment still stops at the wall |
| Tarrik Cinder Judgement | Direct point when the blocking hit has no ASC, then radial with the shot's falloff and sightline rule | After all character packets |
| Cinder sticky grenade | Radial with explosion damage, falloff and sightline rule | After character packets |
| Cinderline Requiem line | Radial per detonation, one packet per owner per line | After character packets |

Radial admission resolves every candidate's sightline before damaging any of them and measures falloff to the nearest bounds point. Damage magnitudes are authored base values; character mitigation, hit-zone and difficulty modifiers are not applied to scenery. Narrative's generic curve-based `ArsenalStatics` explosion and Velkorran's Hunger are not connected. Firearm admission depends on the weapon's effect spec carrying `SetByCaller.Damage`; the authored Cinderline weapon asset was not inspected in a live session.

Validation `20260915-121111-ad9371e0` rebuilt the Win64 Development Editor and Game targets (both exit 0) and passed all 665 matching `ProjectVelkorran` automation tests with source integrity unchanged. The six new suites under `ProjectVelkorran.World.Destruction` exercise the real paths: point and radial admission (owner opt-in, ability-system rejection, falloff, blocked sightline, range, retired collision), Narrative hit-result target data, a native melee sweep that breaks cover without reaching the target behind it in the same step, a Selene Wake that breaks cover without passing it, and a paid Cinder Judgement direct hit that breaks cover while the character behind it receives nothing. Grenade and Requiem admission are compile-verified and share the radial helper's tested behaviour, but have no dedicated runtime test. None of this is live campaign qualification: no M12 placement is enabled, and traversal, navigation rebuild, checkpoint reload, effects, audio and performance remain the rollout gates listed above.
