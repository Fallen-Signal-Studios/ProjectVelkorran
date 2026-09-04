# Native campaign state and evidence integration

`USovCampaignStateComponent` is the durable boundary on `ASovPlayerController`. Narrative remains responsible for quest graphs, dialogue, data tasks and disk save records. `USovCampaignDefinition` supplies a validated beat DAG and references the real pawn, player definition, map and entry marker. Optional completion tasks bridge committed native beats into existing Narrative graphs; this is not a second quest runner.

The native Mantle schema protects the facts that Tarrik is named heir, Caelus is wounded and alive, and Tarrik withholds the market shot. One Degree's precision-shot beat commits the same wounded/alive fact, preserving consistency when played after Mantle or initialized as an authored standalone mission. Other world state, objectives, evidence grants and presentation references remain authored content.

## Content contract

Create assets derived from the native M01/M02 definitions and supply the actual PlayerDefinition, pawn subclass, map, entry PlayerStart, task references and presentation content. Keep mission IDs, beat IDs, cinematic IDs and evidence source GUIDs stable across saves. Definition validation rejects cycles, duplicate IDs/edges, missing prerequisites, mandatory dependencies on optional beats, all-optional missions, duplicate/invalid state keys, contradictory protected prerequisite facts and malformed Echo reserves. The controller separately verifies concrete protagonist class, default attribute configuration and appearance before spawning.

Call `CompleteBeat` from authoritative content conditions. Native admission checks the owned protagonist, prerequisite beats, exact hero knowledge, required facts, immutable facts and skip policy. All native changes commit before Tale/delegate callbacks, and the mutation guard remains active through those notifications. A callback attempting another mutation receives `Busy`; schedule a later command to continue after notification completes. Duplicate completion emits no second journal/task/reward event.

`RecordCinematicViewed` is a content completion signal. Call it only after successful full playback; an aborted sequence does not count. The component verifies current prerequisites, facts and knowledge. Skip requires that stable cinematic ID to have been viewed previously and the beat to be noninteractive. Interactive decisions remain unskippable even after viewing. This boundary does not pretend to verify Sequence playback from a raw Blueprint call; the actual Sequence/controller integration supplies the signal.

`USovEvidenceSourceComponent` is the only evidence acquisition entry. Author EvidenceId, mission, allowed protagonist(s), required completed beat, granted knowledge and interaction range. Newly placed editor components receive stable GUIDs; normal duplication/import receives a new identity and PIE retains it. Existing placed sources with a missing GUID must be assigned one and saved; runtime-spawned sources must supply a stable identity from their spawning content. GUIDs are not randomly regenerated during game loading.

Acquisition verifies source identity, world, mission, protagonist, prerequisite, finite range and line of sight. The actor's own attached/character presentation is ignored by the visibility trace, while world occluders remain effective. Source GUID reuse for a different artifact or mission is rejected. Each protagonist acquires an evidence identity once; grants stay in that protagonist's knowledge record. Recorded provenance includes source GUID, mission/beat, the exact knowledge granted and the journal position at acquisition.

## Save behavior

Save fields hold mission definition references, completed beats, ordered GUID journal events, protected facts, exact per-protagonist knowledge, viewed cinematic IDs and evidence provenance. `Load` validates these together by reconstructing ordered committed facts and knowledge, checking prerequisite and mission succession order and comparing the reconstruction to the saved current state. It does not replay Narrative tasks or commit notifications. Unknown definitions, duplicate/out-of-order events, fabricated completion flags, invalid evidence provenance or inconsistent facts/knowledge fail closed via `IsStateValid` and `OnCampaignStateRestored`.

This validates structural and gameplay consistency; it is not cryptographic save authentication. A saved evidence grant is its durable acquisition payload because its source actor may be in an unloaded map. Asset schema/identity changes require an explicit migration. Earlier partial campaign records without the new definition/provenance fields are rejected rather than silently resetting progress.

## Validation status

The production portable policy compiled under C++17 with `-Wall -Wextra -Werror` and passed 24 assertions covering prerequisites, idempotence, knowledge, canon writes, skip rules, handoff eligibility and stale async epochs.

Four new transient-world runtime tests are authored under `ProjectVelkorran.Campaign.Story`:

- `DefinitionValidation`: DAG, optional prerequisite, protected ancestor and duplicate-edge failures.
- `CommitAndHeroKnowledge`: real possessed-pawn ownership boundary, callback reentry, duplicate completion, cinematic skipping, successor admission, per-hero evidence and valid journal restore.
- `EvidenceRangeAndVisibility`: physical occluder, owned attached presentation, range, prerequisite, deduplication and GUID reuse.
- `SavedStateValidation`: malformed journal sequence, missing provenance and contradicted protected facts fail closed without replay.

Unreal/UHT compilation and runtime automation are unrun in this workspace because the engine is unavailable. Sequence playback, real map transition, Narrative disk persistence, authored evidence placement and playable M01/M02 content remain engine/content validation gates. Tests intentionally bypass Narrative asset initialization where only the campaign state boundary is under test; they do not claim to prove full protagonist handoff readiness.
