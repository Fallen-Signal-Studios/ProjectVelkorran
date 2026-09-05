# Native campaign state and evidence integration

`USovCampaignStateComponent` is the durable boundary on `ASovPlayerController`. Narrative remains responsible for quest graphs, dialogue, data tasks and disk save records. `USovCampaignDefinition` supplies a validated beat DAG and references the real pawn, player definition, map and entry marker. Optional completion tasks bridge committed native beats into existing Narrative graphs; this is not a second quest runner.

The native Mantle schema protects the facts that Tarrik is named heir, Caelus is wounded and alive, and Tarrik withholds the market shot. One Degree's precision-shot beat commits the same wounded/alive fact, preserving consistency when played after Mantle or initialized as an authored standalone mission. Other world state, objectives, evidence grants and presentation references remain authored content.

## Content contract

Create assets derived from the native M01/M02 definitions and supply the actual PlayerDefinition, pawn subclass, map, entry PlayerStart, task references and presentation content. Keep mission IDs, beat IDs, cinematic IDs and evidence source GUIDs stable across saves. Definition validation rejects cycles, duplicate IDs/edges, missing prerequisites, mandatory dependencies on optional beats, all-optional missions, duplicate/invalid state keys, contradictory protected prerequisite facts and malformed Echo reserves. The controller separately verifies concrete protagonist class, default attribute configuration and appearance before spawning.

Call `CompleteBeat` from authoritative content conditions. Native admission checks the owned protagonist, prerequisite beats, exact hero knowledge, required facts, immutable facts and skip policy. All native changes commit before Tale/delegate callbacks, and the mutation guard remains active through those notifications. A callback attempting another mutation receives `Busy`; schedule a later command to continue after notification completes. Duplicate completion emits no second journal/task/reward event.

Managed cinematic beats set `bRequiresCinematicProof` and use `USovCampaignCinematicComponent` on the existing Narrative Sequence actor. The native owner validates playback, participants, equipment and exit postconditions before issuing the single-use receipt accepted by the campaign journal. Direct `CompleteBeat` or `RecordCinematicViewed` calls cannot substitute for that receipt. Skip still requires a previously completed full viewing of the stable cinematic ID and a noninteractive beat; aborted playback and interactive choices cannot acquire skip permission.

`RecordCinematicViewed` remains only a legacy content signal for beats that explicitly do not require native proof. It checks current prerequisites/facts/knowledge but cannot verify actual playback from an arbitrary Blueprint call. It rejects managed beats. Use the managed component for campaign cinematic integration; do not weaken its proof flag to work around a missing participant or postcondition.

`USovEvidenceSourceComponent` owns physical evidence acquisition. Mandatory beats may also introduce explicitly declared critical evidence through the validated journal. [NarrativeStateEngineering.md](NarrativeStateEngineering.md) defines the current five-stage evidence, provenance, consequence and memory contracts. Author EvidenceId, mission, allowed protagonist(s), required completed beat, granted knowledge and interaction range. Newly placed editor components receive stable GUIDs; normal duplication/import receives a new identity and PIE retains it. Existing placed sources with a missing GUID must be assigned one and saved; runtime-spawned sources must supply a stable identity from their spawning content. GUIDs are not randomly regenerated during game loading.

Acquisition verifies source identity, world, mission, protagonist, prerequisite, finite range and line of sight. The actor's own attached/character presentation is ignored by the visibility trace, while world occluders remain effective. Source GUID reuse for a different artifact or mission is rejected. Repeated source/stage acquisition is idempotent; later validated evidence stages retain their distinct provenance, and grants stay in the eligible protagonist's knowledge record. Recorded provenance includes source GUID, mission/beat, the exact knowledge granted and the journal position at acquisition.

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
