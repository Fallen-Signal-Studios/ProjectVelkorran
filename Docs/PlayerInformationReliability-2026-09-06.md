# Player information reliability

Source implementation against audited main `93bf5c2`, 6 September 2026. This closes the bounded player-information slice in the August TDD alignment assessment. Unreal execution and visual accessibility acceptance remain outstanding.

## Behavior delivered

| Concern | Native behavior |
| --- | --- |
| Critical combat captions | Guard, Shield and Poise breaks carry Critical priority. Defense success carries Important priority and ordinary incoming damage carries Routine priority. A higher-priority event can interrupt routine information; an active critical caption completes every page with a minimum three-second interval. Equal-priority events wait in order. |
| Caption bursts | Duplicate active or queued text coalesces without restarting its reading clock. Eight pending distinct captions are retained; higher-priority arrivals may evict the oldest lowest-priority pending item, while routine arrivals cannot evict critical items. Recent history records distinct accepted production events even when the visible queue is saturated. |
| Dense-scene weak points | A weak actor roster is built once per world and tracks new character spawns. Each 250 ms refresh advances a persistent cursor through at most 256 roster entries, independent of whether early actors qualify. The previous maximum 32 marker owners are revalidated alongside the batch. Revealed, nearby, visible, forward-facing anchors rank by view alignment and distance; up to 32 are displayed, within the existing 48 total marker budget. Invalid owners are removed at sweep boundaries. This eliminates permanent starvation behind the first 256 world actors. |
| Final dialogue information | Normal graph completion marks the final speech entry finished and allows its remaining pages to drain. The bounded 64-entry recent history remains available across completed and replaced scenes. A new scene can immediately replace an unfinished predecessor's speech surface while preserving its history. Rebinding the producer owners or tearing down the frontend still clears private session history. This does not introduce persistent full transcripts. |
| Unheard critical records | The existing review menu reads `USovNarrativeCueComponent::GetUnheardRecords`, which is already backed by Narrative component saves. Interrupted critical cues are explicitly labeled as unheard important records. Reviewing their summaries neither fabricates audio completion nor modifies replay/repetition state. The existing evidence view and knowledge restrictions remain authoritative. |
| Dialogue widget replacement | CommonUI removal retires only the component's own widget/speech generation and rearms the observed reply revision. After pause/suspension ends, the component reconstructs the current Tales response set even when the graph revision did not change. Retired or selected graph choices are not resurrected; higher menu/modal layers retain CommonUI focus ownership. |
| Deferred media completion | Audio, sequence and native duration timers now invoke an exported `UDialogueLineCompletionToken` tied to the exact dialogue, node and reply revision. True world pause and explicit dialogue suspension retain a pending completion. The next unsuspended dialogue tick drains it once. New lines, replacement client chunks and teardown retire predecessor tokens. Mutable line-start calls are checked again before broadcasting or scheduling further work. |
| Failed travel recovery | The native frontend observes the save owner's `RecoveryAvailable` result. The existing settings host presents an explicit Retry checkpoint recovery action and its failure explanation. The action calls the save owner's `RetryTravelRecovery`; ownership/pending/suspension checks remain in that subsystem. The row is hidden without a retained recovery and closes after a load request is accepted. Focus refresh goes through CommonUI, preserving higher modal ownership. |

## Validation

`Scripts/Tests/PlayerInformationPolicyTests.cpp` compiled and passed locally with C++17, warnings-as-errors and undefined-behavior sanitization. It exercises caption preemption and dense-roster cursor coverage, including empty, expanded and reduced rosters. These are production policy functions consumed by the actual overlay.

Added native Unreal registrations:

- `ProjectVelkorran.UI.Frontend.FinalLineSurvivesDialogueEnd`
- `ProjectVelkorran.UI.Frontend.CaptionPriorityAndReadableLifetime`
- `ProjectVelkorran.UI.Dialogue.QueuedMediaCompletionAcrossWorldPause`
- `ProjectVelkorran.UI.Dialogue.RetiredMediaCannotFinishReusedNode`
- `ProjectVelkorran.UI.Dialogue.RemovedWidgetRebuildsCurrentChoices`

Extended existing real producer/delegate tests for repeated chip damage, recent-history ownership, and review of an actual interrupted critical cue. The pause test sets the actual world pauser independently of dialogue suspension and verifies that native graph completion and reply publication wait for foreground execution. Widget recovery uses the real native CommonUI host.

**These Unreal tests have not been run here.** Required engine validation includes UHT/UBT after the exported token addition, media callbacks during pause and scene replacement, nested modal focus, retained recovery after accepted-then-failed travel, small-screen/localized caption pagination, and dense scenes with streamed or destroyed weak-point owners. The initial world roster build is proportional to the number of character actors; expensive per-refresh candidate inspection is bounded to 256 new candidates plus at most 32 retained owners. Actor count, authored zone count and viewport projection require measured acceptance on target hardware.

API reference for the world-pauser test setup: [Epic AWorldSettings API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/AWorldSettings). Rolling documentation can show a later engine version; the project's UE 5.7 headers remain the build authority.

## Remaining limits

This slice does not qualify native screen readers or physical console output, supply localized critical cue summaries, create authored HUD art, persist full transcripts, or isolate the existing global settings configuration by account. The production proving route still needs real engine, content and device acceptance.
