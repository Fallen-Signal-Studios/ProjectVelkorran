# Encounter composition and escalation

TDD §§8.4–8.7 required encounter composition, decision budgets, attacker slots, readable offscreen attacks and a recoverable low-pressure state. Before this pass, the encounter director already owned registered NPC snapshots, victory/failure, retry and reward ledgers. Narrative already selected attacks from the whole ability repertoire and owned its existing attack-token leases. The missing layer was composition admission across those actors.

## Native implementation

`ASovEncounterDirector` now owns `USovEncounterCoordinationComponent`. Composition is validated before entry capture, at encounter start and during saved-entry validation. Every authored composition entry refers to an existing registered participant; there is no second NPC spawning or checkpoint system.

| Requirement | Implementation and ownership |
|---|---|
| Tier A/B budgets | Per-wave authoring validation caps full combatants at 16 and supporting actors at 24. Actual wave admission also counts surviving actors from earlier waves. Supporting actors use reduced existing brain/perception component tick cadence and a minimum attack admission interval. |
| Controlled promotion | `SetDecisionTier` changes runtime tier only. It preserves the same actor, GUID, health, position, faction, animation and mission ownership; active or reserved attacks cannot change tier. Retry returns to the authored tiers. |
| Reinforcement waves | Future registered actors remain hidden, noncolliding, movement-disabled and protected by owned Busy/invulnerability counts. Their existing brain/perception and mesh ticks are suspended. Defeating the current wave's required opponents admits the next wave once the live population fits its budgets. Actor identity/resources are not recreated or reset during promotion. |
| Melee slots and role quotas | `ISovBotAttackCoordinator` extends Narrative's existing selection path. A selection reserves one composition ticket before GAS activation and releases it on failed activation, ability end or attempt invalidation. The ticket is checked again after token callbacks and during attack execution. Narrative's own token accounting is preserved. |
| Attack pressure classification | `UNarrativeCombatAbility::BotAttackPressure` supports Melee, Ranged, Support and Automatic. Automatic uses the existing maximum attack range, classifying ranges through 600 cm as melee. Long lunges and hybrid attacks must explicitly declare their pressure class. This is admission metadata, not a second behavior graph. |
| Offscreen lethal attack rule | Player-directed ranged attacks require an onscreen source or a source/attempt-scoped warning receipt. `OnOffscreenAttackWarning` asks presentation for a readable cue. After presentation acknowledges it, the full configured lead time must elapse. Unacknowledged, expired and replayed receipts fail closed. Each admitted offscreen shot consumes its receipt. |
| Protection priorities | The coordinator does not replace target selection. Existing AI may attack living hostile companions and objective actors; all targets still share melee and role quotas. Offscreen presentation admission applies to attacks directed at the encounter player. |
| Resource relief | At 25% health or jointly depleted shield/stamina, a finite relief period reduces simultaneous melee slots to one and spaces new attacks. It grants no health, stamina, shield or Echo. A cooldown prevents continuous renewal. Authored challenge encounters can disable it. |
| Escalation/intensity feedback | `OnWaveChanged` and `OnPressureChanged` expose committed wave and relief state for music/intensity presentation. Encounter state/attempt identity remain owned by the existing director. |

All timers and composition reservations are attempt-local. Future-wave staging owns only the state counts it added; existing checkpoint/failure suspensions and other immunity sources remain intact. Failed encounters keep future actors staged until retry replaces/restores the registered roster. No midfight composition snapshot is represented as an arbitrary rollback save.

## Integration and limits

- An empty composition preserves an existing single-wave encounter, subject to the same population budgets. Once any composition rows are authored, every participant needs exactly one row. Waves start at zero, remain contiguous, and multiwave encounters need a required defeat gate in every wave.
- If optional survivors would exceed the next wave's budget, the next wave waits. Authored retreat/removal or defeating those survivors must make room. Do not disguise a required objective as an immortal required opponent in a defeat-gated wave.
- All future waves must be part of the existing entry snapshot. These are staged registered actors, not an unbounded spawn API. Their spawn positions still require level geometry, collision and navigation validation in the engine.
- Readable warning widgets/audio, music rules, concrete role compositions, StateTree assets, animations, Tier C Mass background populations and Tier D effects are content/performance integration. This pass does not claim that an absent warning callback is a readable cue or that reducing a brain tick interval proves console frame time.
- Native selection and attack payloads must use the existing Narrative selection/execution path. A Blueprint that bypasses that path and directly activates arbitrary attacks also bypasses its token and composition admission; remove that bypass during integration.
- Existing Narrative perception, faction sharing and individual activity selection are preserved. Narrative already supplies `AI/Mass/NarrativeMassAgentComponent`, ped spawning, representation and LOD. A full source/confidence/expiry threat-memory contract and a campaign combatant-state bridge into those C/D representations are separate remaining engineering; this component does not implement or validate that bridge. Preserve the existing Mass foundation.

## Validation

Executed: `Tests/Portable/SovEncounterCoordinationPolicyTests.cpp`, 7,819 budget, warning lead-time and low-resource boundary checks, compiled under C++17 with warnings as errors and undefined-behavior sanitization. The combined native-policy runner passed all 23 suites available at the time of this implementation.

Authored UE automation, not executed in this workspace:

1. `ProjectVelkorran.Campaign.Encounter.Coordination.WavesAndIdentity`: duplicate identity rejection, owned staging, wave promotion, exact actor/GUID/location/health preservation, preservation of another Busy contribution, runtime tier changes and active-attack rejection.
2. `ProjectVelkorran.Campaign.Encounter.Coordination.AttackLeasesWarningsAndRelief`: real Narrative selector and GAS activation, coordination-only execution, shared melee exclusion/release, acknowledgement and lead time, one-use warning receipt, no free resource grant, relief cooldown and attempt invalidation.

Engine acceptance must exercise authored two-wave encounters for both protagonists, optional survivor capacity, retry during a staged wave, an offscreen ranged unit with/without acknowledged presentation, real perception/StateTree throttling and the simultaneous 16A/24B budget under a console profiler. Unreal Engine, UHT and assets are unavailable here; portable results do not replace those gates.
