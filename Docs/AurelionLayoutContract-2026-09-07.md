# Aurelion full-layout authoring contract

The creator authorized implementing the **7 September 2026 Aurelion Level Layout Plan** against the August 14 TDD and Origins chapters 23–26. This adopts the plan's route and chronology for source preparation. Dimensions, enemy counts and timings remain blockout targets until measured in Unreal. The PDF's original proposed status does not constitute an earlier playtest or completed content pass.

The machine-readable reference is [AurelionFullLayout-2026-09-07.json](../Scripts/Manifests/AurelionFullLayout-2026-09-07.json). Run:

```powershell
python Scripts/Validate-AurelionLayout.py --check-native-bindings
python -m unittest discover -s Scripts/Tests -p TestAurelionLayout.py
```

The validator checks connected topology, dimensional sanity, timing arithmetic, reinforcement gates and active caps, named mission/encounter references, checkpoint progression, mutually exclusive support and shared-first build order. Its optional C++ scan checks literal name drift only. Neither the validator nor its tests execute Unreal, create assets, simulate the encounters or qualify gameplay.

## Route and geometry

The two entrances converge physically at Z05. The player experiences them sequentially through the isolated perspective cut from Z02 to Z03. That cut is not a physical corridor and cannot bring Tarrik or a protagonist companion into Selene's approach before their meeting. Canonical companion activation begins only at the shared-breach handoff.

All dimensions are meters, at 100 Unreal units per meter. Unknown absolute elevations remain unspecified in the manifest.

| Zone | Playable envelope | Elevation / local detail | Chunk |
|---|---|---|---|
| Z00 Broken landing | 28 × 22 | Tarrik entry datum 0 | A |
| Z01 Pressure hall | 56 × 26 | 0 to +2; three cover bands | A |
| Z02 Survivor bend | 30 × 18 | Returns to entry datum 0 | A |
| Z03 Frozen sensor access | 48 × 22 | Selene entry datum 0 to +3 | B |
| Z04 Relay overlook | 62 × 38 | Main 0; flank balcony +3 | B |
| Z05 Meeting atrium | 72 diameter | 6-wide bridges; 18-wide shaft opening; approximately 300-diameter scenic room | C |
| Z06 Breach rescue | 56 × 34 | Shared-route datum 0; refuge +1 | D |
| Z07 Capture gallery | 34 × 22 | Shared-route -3 | D |
| Z08 Quarantine crucible | 70 × 48 | Shared-route -6; east balcony +3 locally; 18 × 14 central court | D |
| Z09 Wound gallery | 55 × 16 | Shallow descent; no invented gravity reversal | E |
| Z10 Fifth chamber | 54 diameter | Lower core; 10-diameter human interaction dais | E |
| Z11 Observation gallery | 24 × 16 | Several levels above core; short authored lift | F |
| Z12 Departure concourse | 42 × 24 | Shared concourse; scenic opposite berths | F |

Keep the primary combat lanes 4–6 m clear, flanks at least 3 m, ordinary doors 3 × 3.5 m and elite entrances 5 × 4.5 m. Cover targets are 1.2 m low and 2.2 m high, with 2.5 m of camera clearance. Ramps rise 3 m over 10–12 m with flat landings and require movement/camera validation. The manuscript's vast spaces are scenic envelopes, not justification for long empty playable crossings.

## Clock and encounter budgets

| Route segment | Main target |
|---|---:|
| Z00–Z02 Tarrik approach | 10 min |
| Z03–Z04 Selene approach | 10 min |
| Z05 meeting and carrier rescue | 5 min |
| Z06 shared breach and rescue | 5 min |
| Z07 cage destruction, isolated threat exchange and local priority | 5 min |
| Z08 final encounter and quarantine | 5 min |
| Z09–Z10 recognition, assent, Witness and unavoidable consequence | 5 min |
| Z11–Z12 voluntary conversation and separate departure | 5 min |
| **Mandatory total** | **50 min** |

O1 in Z02 adds 2 minutes, O2 in Z04 adds 2, O3 in Z07 adds 1, and O4 in Z11 adds 3. All optional pockets rejoin their own zone and cannot gate progression. **All content totals 58 minutes.** The three-minute priority decision is already inside Z07's five minutes and the 15-minute Eclipse block. No mission-wide countdown is added.

| Encounter | Committed roster | Active cap | Required staged conditions |
|---|---|---:|---|
| E1, Z01 | 6 Reformation security drones | 4 | Four initial. The last two require the first formation broken **and** at most two alive. |
| E2, Z04 | 4 Dominion Enforcers + 2 contaminated Reformation drones | 6 | Both physical receivers disabled and local hostile pressure cleared. Scanner failure alerts the existing two drones and creates no extra enemies. |
| E3, Z06 | 5 Linkbound + 1 Wall-runner + 1 Weaver | 6 | Three Linkbound and one Wall-runner initially. The final two Linkbound and Weaver require the rescue approach open **and** at most three alive. The trapped-marine rescue remains a separate confirmed action. |
| E4, Z08 | 2 Linkbound + 1 Weaver + 1 Wall-runner + 1 elite | 5 | Selene isolates the links; the Wall-runner enters after the first anchor falls. A controlled handoff gives Tarrik the same isolated elite and Selene a valid frost anchor. |

`ASovAurelionLinkPhaseDirector` owns phase A's actual native link-sever receipts and frozen handoff boundary. Its `CompletePhaseHandoff` transfers the living roster to phase B after the real Tarrik handoff, then requires the verified phase-B entry checkpoint before release. `ASovAurelionThermalPhaseDirector` combines `USovAurelionThermalFractureComponent`'s actual frost/heat/payoff receipt with conventional victory. Author phase A with exactly two `RequiredLinks` bindings (participant, component and link identity), its real `HandoffAnchor` and `PhaseBObjective`. Phase B starts with an empty participant array and the same `EliteParticipantId`; do not pre-register a second copy of the roster or elite. The elite needs `USovAurelionThermalFractureComponent` and `USovWeakPointComponent`. These source contracts still need real link components, partner anchors, animation, effects and staged content. Reinforcement arrivals and the Wall-runner aperture remain authored staging; these director classes do not automatically create or activate waves.

This is **24 committed hostiles across four physical encounters**. Contaminated drones are a state variant of the opening drone role. Two native E4 phase directors do not create a fifth encounter, a second elite or a second hostile budget.

The two E2 receivers are `M12_E2_ReceiverWest` and `M12_E2_ReceiverEast`, using `ASovCampaignRelayReceiver` and the objective bridge's `RequiredReceivers`. Place both where the player can reach a close, visible interaction even with no ammunition. Their exact world positions are content assignments, not fabricated coordinates in the manifest. Runtime proof requires both native interactions; an editor checkbox or visual state is insufficient.

E3's contextual ceiling assist is not the full joint Resonance. The single full payoff is E4's **Thermal Fracture**: Selene arrests the exposed joint and Tarrik's Cinder strike fractures it. A missed or interrupted window must recover valid partner positions without spending a scarce signature resource. After the demonstration, ordinary exposed-core damage may finish the elite. Assistance widens or automates confirmation, never requires rapid tapping.

## Native phase and checkpoint bindings

| Physical encounter | Native binding | Completion beat |
|---|---|---|
| E1 | `M12_E1_PressureHall` | `PressureHall` |
| E2 | `M12_E2_RelayOverlook` | `RelayOverlook` |
| E3 | `M12_E3_SharedBreach` | `BreachSharedJunction`, followed by authored `FreeTrappedMarine` |
| E4 phase A | `M12_E4_QuarantineCrucibleA` | `SeverCrucibleLinks`, then `HandoffToTarrikCrucible` |
| E4 phase B | `M12_E4_QuarantineCrucibleB` | `ThermalFracture` |

| Checkpoint | Boundary | Restored lead |
|---|---|---|
| CP0 | Z00 before context / `TarrikArrival` | Tarrik |
| CP1 | Z01 before E1 | Tarrik |
| CP2 | Z03 after the isolated approach handoff | Selene |
| CP2b | Z04 after sensor traversal / terminal read, before E2 | Selene |
| CP3 | Z05 before meeting; both approaches complete | Selene |
| CP4 | Z06 after carrier rescue and shared-breach handoff | Tarrik |
| CP4b | Z07 after cages and isolated threat exchange, before priority acknowledgment | Selene |
| CP5 | Z08 after `LocalPriorityCommitted`, before E4 | Selene |
| CP5b | Z08 after `HandoffToTarrikCrucible`, before Thermal Fracture | Tarrik |
| CP6 | Z09 after survivor clearance and quarantine, before recognition | Tarrik |
| CP7 | Z10 after `GrammarPropagation` and the full core consequence | Selene |
| CP8 | Z11 after voluntary reunion, before evidence/pact conversation | Selene |
| CP9 | Z12 after `SeparateDepartures` | Tarrik |

The placed `ASovAurelionCheckpoint` supplies CP0, CP2, CP3, CP6, CP7, CP8 and CP9 through exact `Aurelion.CP*` keys and native progress guards. It captures existing state and never completes a beat. CP1, CP2b and CP4 use the encounter objective's verified `ArenaEntry` transaction; CP5b is the verified phase-B entry after the live-elite transfer. Do not add a second writer at those arena boundaries. The priority terminals own CP4b and CP5. The JSON does not create checkpoints. Handoff/recovery source, placed actors, streaming availability and save/load tests must agree with the manifest before the route is accepted.

## One local priority, two observable outcomes

`Aurelion.RescuePriority` is pending until a support order is acknowledged in Z07. It commits one `ImmediateProtection` choice, with CP4b before the prompt and CP5 after acknowledgment. There is no second choice in E4.

| Outcome | Immediate support | Later acknowledgment |
|---|---|---|
| `WestStretchers` | Lyessa protects the medics; the west recovery cache is accessible. | Protected carry and making room for those who could not run. |
| `EastWalkers` | Mobile survivors open the east flank shutter before phase B. | A survivor helps the last stretcher; trusting others with the route. |

Both groups are mixed faction and both reach safety. Malik, Lyric, Tharne and Lyessa are not killable choice stakes. West-cache access and the early east-flank benefit are alternatives. The east flank opens normally for both outcomes after `HandoffToTarrikCrucible`; East buys earlier access, not a permanently different map.

`ASovAurelionPriorityTerminal` supplies normal held, nearby line-of-sight interaction for an explicitly placed option. Place both real active option terminals and exactly one support actor; one missing option cannot become a silent default choice. `ASovAurelionPrioritySupport` derives west-cache/east-shutter access and the aftermath selection from the same saved choice. It does not spawn or grant consumables. Real one-time pickups behind the west barrier remain the inventory/recovery owners. Survivor poses, cache contents, barrier geometry, audio and actual dialogue lines still require content authoring. Retrying or skipping cannot select the other benefit or duplicate the choice record.

## Build and acceptance order

1. **Shared skeleton first: Z05–Z12.** Block collision, gates, story entry/exit anchors and all recovery boundaries. Prove every checkpoint can reach the next with temporary content.
2. Add the separate Z00–Z04 entrances after shared flow is stable.
3. Tune E1/E2 identity loops with production movement and camera, including zero Echo and low ammunition.
4. Integrate E3 rescue, both Z07 support outcomes, E4 phases and recoverable Thermal Fracture.
5. Lock the five-minute core storyboard before final environment work; then author the quiet conversation and separate departures.

Chunks A–F preserve shared mission state through unload. Prestream the destination before transition and retain the previous chunk until the checkpoint commits. Do not use quarantine barriers to disguise blocking loads as seamless gameplay.

The core and aftermath contain no new present-day combat. Carrier rescue is fixed success for 7,184 people. Both cages are destroyed, command authority stays isolated, quarantine precedes contrary recognition, and both protagonists assent independently. Boundary closed, release withheld, unavoidable grammar propagation, Crownmark Five integrated, Lyric alive/not cured, evidence exchanged, pact and separate destinations remain fixed.

Test both priority outcomes, E4 phases, every handoff, saves before and after assent, scene skip, evidence custody and final departure. Record actual timings, production cost and representative platform performance. The TDD's 80% protagonist identification, 70% Echo understanding, 80% corruption-remedy understanding, checkpoint/reload success **above 99.5%** and zero surviving canon contradictions remain acceptance targets, not results from this source pass.
