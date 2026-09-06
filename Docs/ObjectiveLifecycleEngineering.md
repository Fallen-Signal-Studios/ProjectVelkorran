# Objective lifecycle and bounded campaign choices

Implemented 6 September 2026 against the August 14 campaign TDD, sections 9.11–9.14, 11.8, and 18.6. This extends `USovCampaignStateComponent`, the existing authoritative owner above Narrative quests and saves. It does not add another quest or campaign state system.

## Objective state

A mission's existing stable `BeatId` is also its objective identity. Existing assets retain campaign-definition schema 1 and their existing `CompleteBeat` behavior. New objective metadata is optional: `ObjectiveType`, `FailureReasonId`, and `FailureRuleText`.

`GetObjectiveState(MissionId, BeatId)` reports:

| State | Source and permitted behavior |
| --- | --- |
| Inactive | Missing prerequisites, protagonist knowledge, required choice, wrong active lead, or an unresolved objective in a previous mission. No activation/completion is granted. |
| Available | Current prerequisites and knowledge permit the objective. It can activate explicitly or use the existing completion path. |
| Active | An accepted `TransitionObjective(BeatId, Active)` request. |
| Succeeded | The existing successful beat transaction, including its normal proof, consequence, knowledge and canon checks. |
| Failed | An accepted optional objective failure. The beat must declare a stable failure reason and readable failure rule. |
| Skipped | An accepted optional non-choice objective skip. |
| Superseded | An unchosen alternative after another outcome in its declared choice group successfully commits. |

Terminal states cannot reopen or later grant beat rewards. Mandatory and canon objectives cannot fail or skip through this API. Choice alternatives cannot fail/skip independently and strand the required reconciliation. Success continues through `CompleteBeat`; `TransitionObjective` cannot manufacture success or supersession. Retry restores the checkpoint's previously validated state rather than reopening a completed transaction.

Available/Inactive are projections of already journaled beat/evidence progress and current lead, so a cached availability flag cannot reveal future information. Explicit activation/failure/skip writes `FSovObjectiveJournalEntry`. Each event records both the beat-journal position and the evidence count at that instant, preventing reload from treating later-acquired knowledge as available earlier. Success and atomic supersession derive from the existing completion journal, whose consumers retain their completion-only contract.

`GetActionableObjectiveIds()` returns only Available/Active IDs. Bind authored objective UI to this query and refresh on `OnObjectiveStateChanged`, `OnBeatCommitted`, `OnEvidenceRecorded`, `OnMissionChanged`, and `OnCampaignStateRestored`. Render an optional objective's failure rule before accepting gameplay that can fail it. Show failed/skipped outcomes as history, not current goals. Concrete mission widget and world-marker authoring remains an engine/content gate.

## A choice with one durable outcome

Create a `ChoiceGroups` entry with a stable `GroupId`, a mandatory `ReconciliationBeatId`, and an explicit `ReconciliationNote`. Add that group ID to the reconciliation beat's `RequiredChoiceGroups`. Author two to four optional interactive outcome beats with the same `ChoiceGroupId`, shared ordered prerequisites, and the same required protagonist.

The bounded first slice requires shared knowledge/state gating on a common prerequisite beat. Outcomes cannot grant distinct protagonist knowledge, write protected canon facts, carry cinematic/co-action/handoff proofs, or act as prerequisites to other beats. Mandatory beats cannot require a variable state value written by an outcome. Depend on the mandatory reconciliation beat for later progression. Existing consequence and asymmetric memory records provide the selected result and its downstream reactions.

Call `ResolveChoice(GroupId, OutcomeBeatId)` after accepting the player's selection. The selected beat, its consequences, selected group ID and superseded alternatives commit under the existing transaction guard. Calling `CompleteBeat` directly still enforces exclusivity. Duplicate selected outcomes are idempotent; different alternatives are rejected, including callback-driven attempts. `GetSelectedChoice(MissionId, GroupId)` lets escape staging or dialogue consume the durable outcome.

The native proof uses `ProtectDominionCrew` and `ProtectReformationCrew` followed by the mandatory `EscapeTogether` beat. Both routes independently verify one consequence, supersession of the other choice, serialized reload, and completion of the same fixed escape. This is an executable engineering fixture, not a claim that M12 world content or its final dialogue has been authored or played.

Existing pre-choice safe-save requests remain in the beat transaction. Accepted choice outcomes and optional terminal changes queue an autosave through the existing safe-boundary mechanism. The save owner decides when the world is safe and reports write failure; a queued request is not represented as a durable disk receipt.

## Save compatibility and validation

Campaign state schema 2 adds objective events plus per-mission objective-state and selected-choice caches. Narrative remains the disk owner. Load replay reconstructs both caches and requires exact equality with saved state. Unknown objective IDs, invented successes, swapped selections, inconsistent event order, duplicate event IDs, invalid failure reasons and terminal reactivation fail validation. Validation emits no gameplay completion events.

Schema 1 completion-only saves migrate completed beats to Succeeded; unresolved beats retain derived availability. Migration invents no activation, failure, or choice history. Old schema 1 mission definitions with newly retrofitted choice groups require an explicit content/save migration rather than guessing which outcome was chosen. Existing authored assets without these opt-in groups require no reauthoring. Loading into a reused component clears newly introduced fields before reading, so fields omitted by an old archive cannot retain current-campaign objective history.

## Verification

- The production-used portable policy compiles with C++17 warnings-as-errors and UndefinedBehaviorSanitizer. Its exhaustive test checks 1,048,576 combinations of source state, destination state and objective flags, plus completion restrictions.
- Six native tests are registered under `ProjectVelkorran.Campaign.Objectives`: terminal lifecycle and real serialized reload; both exclusive protection choices and fixed escape; cache/choice/order tampering; schema 1 migration; authored canon guards; and evidence-before-activation ordering, including an event before the first beat.
- Native Unreal execution, full M12/M13 route authoring, actual failed-save presentation, and verification of both outcomes in the playable world remain required engine/content gates. Portable passing results do not establish Unreal compilation or in-editor behavior.
