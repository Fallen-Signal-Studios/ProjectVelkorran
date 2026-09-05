# Narrative state engineering

This pass completes the native narrative-state gaps in the August 2026 TDD, sections 9.3, 9.7–9.10 and 9.14, within the existing Narrative and campaign architecture. The December attachment is superseded by that revision. Dialogue delivery, speech priority and interruption are covered separately by `Narrative/SovNarrativeCue*` and Narrative's playback suspension.

## Findings and decisions

| Gap | Existing implementation | Native change and reason | Dependencies and risk |
|---|---|---|---|
| Evidence had acquisition IDs and hero knowledge but no provenance progression | `USovCampaignStateComponent::Evidence`, the journal sequence and physical source component already survived saves | Extend those records with canonical definition, location, custodian, stage, support, witnesses, copies and publicity. Keep one acquisition history. | Authored source identities and evidence definitions must remain stable; changed definitions are checked during replay and can invalidate incompatible saves. |
| Optional searches could become an undeclared critical-path dependency | Physical acquisition had range, visibility, mission and hero checks | Mandatory beats acquire configured critical records atomically with their journal event. Optional physical sources cannot be the first acquisition of a critical record. | Required records must be assigned to mandatory beats; there is no invented story content or automatic interpretation. |
| Typed consequences and asymmetrical memories were missing | Mission beats already committed semantic state writes | Add bounded consequence definitions and records to the same beat entry, with separate holder/subject memories. No approval score or second relationship ledger. | Each consequence needs a downstream consumer or explicit archival purpose. Cross-mission memory dependencies must be declared and present before mission entry. |
| Viewmaker did not have a bounded native investigation entry | Weak points, command links, evidence sources and companion commands already existed | A native inspection library calls those systems after verifying controlled, ready, living Selene, angle, distance and opaque-world visibility. | Input action, scan presentation and authored traces remain content work. A trace through cover never reveals linked actors. |
| Narrative graphs could not query the campaign through a stable native interface | Narrative already provides condition/event objects and completed-data-task history | Add native query and end-event adapters. Physical proof and campaign transaction checks remain authoritative. | Existing graph assets must choose the adapters and reference valid beats, evidence sources and participants. |
| No combined graph preflight for the campaign contract | Narrative exposes the real graph nodes and replies | Validate the real graph, native adapters, speaker roles, line fallbacks, IDs, exits and subtitle timing. | Arbitrary Blueprint conditions, sequence blocking and performance references still need their own content review. Validation reports these limits. |

## Evidence contract

`USovEvidenceDefinition` contains the record's stable ID, canonical content ID, accessible summary, optional full text/audio, original and allowed custodians, independent supporting-record IDs, authentication authorities, copy destinations, claims, contradictions, known alterations and relevant missions.

`USovEvidenceSourceComponent` retains physical ownership of acquisition. A raw evidence name cannot write a record. It requires a stable source GUID, an active mission, an allowed protagonist, finite range, actual visibility, configured prerequisites and compatible provenance metadata. Duplicate live source GUIDs fail closed. Legacy acquisition-only sources remain supported for one private `Observed` entry; they cannot fabricate higher stages, provenance or copies without a canonical definition.

Progression is sequential:

1. **Observed:** first valid acquisition, or guaranteed observation by a mandatory critical-path beat.
2. **Questioned:** a configured source recognizes inconsistency in an already observed record.
3. **Corroborated:** a distinct source and custodian present a configured supporting record that this protagonist already knows.
4. **Authenticated:** a configured authentication authority confirms the corroborated record.
5. **Distributed:** a configured recipient receives a copy after authentication. The recipient must have an explicit institution/control domain different from the original custodian. Another desk inside the original institution does not count.

Additional valid distribution destinations can be recorded after the first. Repeated copies are idempotent. Each protagonist's attained stage is queried independently; a named witness or copy recipient gains awareness without silently inheriting another protagonist's complete interpretation. Knowledge grants reach only the acquiring protagonist and explicitly named protagonist witnesses/recipients.

The read APIs are `GetEvidenceStage`, `KnowsEvidence`, `ObserverKnowsEvidence`, `HasEvidenceCopy` and `GetEvidence`. UI and dialogue should use the protagonist/observer queries before exposing record details. `GetEvidence` is the complete persisted history, not a filtered presentation list. Canonical claim/alteration text is authored data; the system does not infer truth from a scan.

## Consequences and memories

`FSovConsequenceRecord` contains its authored definition, mission, beat, actual resolved instigator, journal order and campaign play time. The definition holds subject and witness IDs, semantic choice/outcome tags, publicity, bounded typed payload, canon class, persistence classification and consumer IDs. `bArchivalOnly` defaults false so lack of a consumer is an explicit authoring decision.

A memory identifies its own stable ID, holder, subject, consequence, kind and how its holder learned about the action. A witnessed memory requires the holder to be the actual instigator or a recorded witness. Learned, inferred and told memories are authored disclosures at a validated beat; they do not rewrite the original witness list. `FindConsequence` returns a record only to its instigator, recorded witnesses or an observer with an authored disclosure memory. Passing no observer is the internal historical query. Publicity records scope of publication; it does not grant omniscient knowledge to every character.

`RequiredPriorConsequenceIds` declares inherited mission facts. Entry and replay require them to exist before the mission; the shipping manifest verifies mandatory predecessor writers and detects bypass paths. Within a mission, memories may reference the current beat, guaranteed prerequisite beats or these inherited IDs. Duplicate consequence or memory identities fail validation/commit rather than creating an unsavable history.

Persistence is a record classification for downstream consumers and export. The factual history is retained for audit; records are not silently deleted when an encounter ends. There is no universal morality, faction-affinity or approval calculation.

## Existing Narrative integration

Use `USovCampaignNarrativeCondition` for beat/mission completion, acquired knowledge, evidence stage and observer knowledge, consequence existence or exact typed payload, asymmetric memory, companion availability, owned technique, and existing Narrative completed-data-task history. Queries use the actual current protagonist, including authored M12/M13 handoffs.

Use `USovCampaignNarrativeEvent` as an end-only event with save refiring disabled. It can request an ordinary native beat commit or acquisition through an exact unique physical evidence-source GUID. It cannot manufacture a handoff or co-action receipt. Consequently, relationship/evidence writes stay in the existing mission transaction and native source paths rather than arbitrary dialogue flag setters.

`USovNarrativeValidationLibrary::ValidateDialogue` accepts the existing `UDialogue` asset/CDO, an optional mission context, a shipping-validation flag, and error/warning arrays. It checks:

- Stable, unique node IDs; bounded graph size and reply count; no dangling edges or unreachable nodes.
- Every reachable path can reach a structural exit. Loops with an actual exit remain legal.
- Unconditional fallback line variants and a legal unconditional outgoing reply where a node has successors.
- Valid native condition/event configuration, existing local beat references and prohibited forged handoff/co-action writes.
- Declared speaker/listener roles; complete subtitle text for voiced lines; finite positive explicit durations; fixed subtitles lasting through known finite audio; no infinite unskippable lines.
- Stable string-table IDs for shipping spoken text.

The validator does not execute arbitrary Blueprint conditions, prove every possible authored performance binding, compare paraphrase intent with delivered dialogue, or judge localization readability. It reports those dependencies rather than claiming they passed. Shipping commandlet aggregation is documented with the settings/diagnostics validation work.

## Viewmaker integration

`USovViewmakerLibrary::ScanTarget` is the presentation entry. The native world query is limited to 1,000 cm and a 30-degree camera half-cone; an evidence source can impose a tighter range/angle and known-query requirement. The source default is 300 cm and 20 degrees. A failed query clears its output instead of leaving a previously scanned target visible.

A valid scan can return live ASC health status, acquire a configured evidence stage, return authored recent-use trace IDs and route hints, reveal the existing weak-point component for five seconds, and return only command-link members independently visible inside the same scan bounds. It does not select all interactables, scan through walls or translate an unknown signal into an invented answer.

`RequestCompanionAnalysis` routes the visible target into the existing companion `Interact` command. Companion identity, presence, mission permission and command availability are still checked by that component. Input mapping, reconstruction effects, record widgets and actual device traces are editor/content tasks.

## Save and transaction validation

Campaign records restore in Narrative's `World` phase before presentation components resume. Saved state replays the one journal and interleaved evidence history, checking stages, source identity, mission/hero order, guaranteed critical acquisition, typed consequence contents, witness identities, memory dependencies and each authored handoff. Rehydration emits no acquisition or beat-completion rewards.

This pass also fixes inherited implementation defects:

- A previously witnessed critical record no longer gets skipped during commit and then rejected during replay merely because the active protagonist was its witness rather than its first observer.
- Duplicate memory IDs cannot commit successfully and subsequently make the save invalid.
- Failed consequence/memory validation no longer consumes a physical co-action receipt before the transaction is ready to commit.
- Failed consequence queries and failed scans clear stale output.
- Unready/dead Selene cannot use the Viewmaker acquisition path.

## Validation performed and remaining gate

`Tests/Portable/SovEvidenceNarrativePolicyTests.cpp` compiles the production-used progression, scan and graph policies with C++17, warnings as errors and UndefinedBehaviorSanitizer. It exhaustively checks all 65,536 four-node directed graphs against an independent all-pairs reachability oracle, plus every progression/authorization combination: **525,072 checks**, with additional malformed graph, nonfinite value, range and angle cases. This suite passed.

`Private/Tests/SovNarrativeStateRuntimeTests.cpp` adds five UE automation scenarios: full evidence provenance/distribution, atomic consequence/memory/critical evidence, inherited cross-mission knowledge, physical Viewmaker bounds, and validation over actual Narrative graph objects. They cover replay tampering, independent institutions, witnessed knowledge, opaque cover and stale outputs. Existing campaign/co-action/handoff tests must also remain green.

Unreal 5.7 and its toolchain are unavailable in this workspace. UHT, module compilation and UE automation were **not run**. These authored runtime tests must pass in the configured UE environment before treating the slice as integrated gameplay. Content assignment, actual dialogue graph preflight, authored scan geometry and save/load playthroughs remain the editor acceptance gate.

## Managed cinematic follow-up

Native M01/M02 cinematic beats now require `USovCampaignCinematicComponent` playback receipts. Dialogue events, raw beat completion and the legacy viewed setter cannot create those receipts. Saved managed viewing history is rebuilt from full-view journal entries in order. See [CinematicEngineering.md](CinematicEngineering.md) for exact participant/sequence preflight, pause/skip/postcondition contracts and the remaining World Partition/editor validation limits.
