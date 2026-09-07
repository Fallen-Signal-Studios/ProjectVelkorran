# Aurelion E4 runtime binding

This implements two native encounter segments in the same physical arena. It does
not replace the elite at the protagonist switch. It does not give an active combat
encounter a general handoff or save exception.

| Segment | Mission beat | Native director | Receipt |
| --- | --- | --- | --- |
| E4A | `SeverCrucibleLinks` | `ASovAurelionLinkPhaseDirector`, ID `M12_E4_QuarantineCrucibleA` | Two real, distinct Selene command-link sever transactions |
| Switch | `HandoffToTarrikCrucible` | Existing `ASovCampaignHandoffAnchor`, ID `M12_TarrikCrucible` | Existing native controller handoff journal receipt |
| E4B | `ThermalFracture` | `ASovAurelionThermalPhaseDirector`, ID `M12_E4_QuarantineCrucibleB` | Actual local frost/heat/Poise fracture receipt, then conventional required-enemy victory |

Both encounter beats use `ASovCampaignEncounterObjective`; their mission contracts
select the corresponding `RequiredEncounterProof`. Ordinary kill-all directors and
generic terminals cannot manufacture either specialized proof.

## Phase A and the preserved boundary

Author the initial NPC roster only on the A director. Register the elite under a
stable `EliteParticipantId`, both survivor groups as protected non-victory actors,
and the remaining enemies normally. E4 uses actors throughout both phases. Assign
exactly two `RequiredLinks`, each naming a registered owner participant, its command
link component name, and its unique link ID. Both native links must be active when
the phase starts. A source death that merely deactivates a link does not count as
Selene severing it.

Set the source's `MissionId`, `CompletionBeat`, `HandoffBeat`, `HandoffAnchor`, and
`PhaseBObjective`. The B objective references the initially empty B director, whose
`EliteParticipantId` must match A. Do not put another copy of the elite on B. If
using automatic boundary transitions, enable `bAutoRequestHandoff`; the existing
handoff anchor still applies its exact mission, readiness, position, visibility,
navigation and durable-checkpoint checks. `bAutoStartPhaseB` defaults to true.

After both genuine sever receipts, the A director freezes its remaining live roster
and waits for active abilities, temporary effects and unsafe resource states to
settle. It then records phase success. Only this completed, frozen, quiescent
boundary may ignore its own registered threats during save admission. Arbitrary
active/restoring encounters remain blocked by the original handoff path.

`CompletePhaseHandoff(Tarrik)` requires the actual committed handoff receipt and
current ready Tarrik. It transfers the existing living actors and their suspension
ownership into B without changing their health, shield, other resources, links or
transforms. The A director persists the transferred IDs in its existing save record;
loading A cannot reclaim those actors or create parallel encounter ownership.

B captures the new Tarrik entry and the live carried roster through the existing
checkpoint system. Its verified ArenaEntry boundary is CP5b. Only after that write
succeeds does B release combat. A failed write leaves B inactive and frozen, so the
same entry request can be retried. Do not add an ordinary checkpoint terminal after
B becomes active and call it CP5b.

## Thermal Fracture

The elite's authored Blueprint needs `USovAurelionThermalFractureComponent` and its
native weak-point component. Assign `FrostAnchor` and give that single actor the
unique tag in `FrostAnchorId` (default `Aurelion.Crucible.CleanFrost`). Input calls
`RequestFrostSetup(Tarrik, Error)` and `RequestConfirmFracture(Tarrik, Error)`. The
mark reach defaults to 150 cm, setup reach to 1500 cm, confirm reach to 350 cm, and
the recoverable window to 3 seconds. The contextual frost setup uses the actual owned
inactive Selene and a named clean frost anchor; heat confirmation uses the current
Tarrik and the living target. The component supplies a repeatable contextual path
without requiring a full Echo meter, ammunition, or a paid signature ability. It
also observes appropriate ordinary real thermal hits.

Use `GetFractureWindowRemainingSeconds()` for the available confirm window and
`OnThermalFractureCompleted` for the successful native payoff signal. Interrupting
either hero with a stagger, movement lock, ragdoll, sequencing, interaction or busy
reservation immediately cancels the current window and any pending payoff. Recovery
requires a fresh frost setup. Author Selene's repositioning to the named anchor and
the action prompts; the component checks the actual position and visibility but
does not teleport or automatically move the companion.

The component emits its completion event only after actual native control, thermal
damage and the Poise fracture payoff. It applies no automatic elite kill or campaign
completion. The B director retains the native receipt before corpse cleanup and
requires the normal required-enemy deaths afterward. Missing a window while the
elite lives allows another setup. Killing the elite/finishing required combat before
fracture explicitly fails the phase and restores through the ordinary B entry retry.

The scene's active cap, Wallrunner arrival after its anchor, geometry, visual and
sound cues, authored animations, and action-input wiring remain content qualification
work. The runtime classes do not claim those staged content events happened.

## Verification

Native tests under `ProjectVelkorran.Campaign.Aurelion` and the thermal component's
suite exercise real link severs, frozen phase completion, retained elite resources,
wrong-handoff refusal, premature-death recovery, and actual frost/heat fracture
transactions. Source inspection and host checks cannot substitute for running these
tests in UE. The full physical handoff, phase ownership save/load, CP5b disk boundary,
content-driven actions and reload/retry must be qualified on the work PC.
