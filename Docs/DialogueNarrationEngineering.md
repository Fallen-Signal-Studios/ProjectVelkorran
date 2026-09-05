# Tales dialogue choices and completion-aware narration

Source implementation: 5 September 2026. Authority: TDD 9.6, 13.8 and 13.9. The existing Tales graph, node/event ownership and CommonUI activation/focus system are retained. This is source implementation, not a claim that UE 5.7 compilation or platform accessibility certification has passed.

## Native path

`USovDialoguePresentationComponent` is a local controller component. It subscribes to the real `UTalesComponent::OnDialogueRepliesAvailable`, dialogue replacement/finish, selected reply and playback-suspension delegates. It opens `USovDialogueChoiceWidget`, a native `UNarrativeMenu`, on the existing HUD's `UI.Layer.Game`. The native HUD registers the existing Game/Menu/Modal CommonUI stacks. No widget Blueprint is needed for this path.

The widget creates a safe-zone panel, scrollable text, named speaker/count, numbered CommonUI buttons, selection focus and timer text. It inherits the earlier CommonUI focus-memory and boundary-wrap fixes. UI text scale and high contrast use the live settings snapshot. Full paraphrase and hint text come from the actual Tales player nodes; the node graph and consequences are never copied into a second dialogue system.

The component accepts readiness only after the active widget is visible, has nonzero laid-out geometry and has constructed every real choice button. A UObject or accessible label alone is not reading readiness. A HUD missing during startup is retried with no running pressure. An intentionally removed widget cancels its presentation; `RefreshChoices()` can explicitly restore only the currently presented Tales reply revision.

## Pressure contract

| Concern | Implemented behavior |
| --- | --- |
| Default | `UDialogueNode_NPC::ReplyPressureSeconds = 0`: no timer. |
| Authored pressure | Explicit finite duration up to 300 seconds, and `SilenceReplyID` resolving to exactly one direct, unconditional, non-auto-selected, readable player response in the current reply set. Invalid or unavailable silence cannot arm pressure. Graph validation rejects invalid authoring. |
| Story ownership | Timeout submits the existing player node through `UTalesComponent::TrySelectPresentedDialogueOption`; all Tales conditions, graph events and consequences retain their authority. Local network clients never create a server-authoritative timeout. |
| Identity | Live dialogue pointer plus monotonically advanced presentation revision. New chunks, NPC/player lines, selected responses and deinitialization invalidate old presentations. Checks before/after reply-shot and replies-available callbacks prevent a retired node from announcing or stopping successor audio. |
| Reading | Display readiness, configured minimum reading duration (2–30 seconds), and successful narration completion when narration is enabled must all be satisfied. The frame that first satisfies minimum reading is not charged to pressure. |
| Settings | Standard/extended/disabled timing, extension factor and minimum reading time come from `FSovUserSettingsSnapshot`. A live settings transaction reconstructs the current presentation and re-announces it; it does not carry an old shorter deadline across the settings change. |
| Pause | Game pause, Tales scene suspension and a covering Menu/Modal pause active elapsed time and retire current speech. Resume re-announces the choices before pressure can continue. |
| Selection speech | Speaker, choice count, every choice, current selection and timer rules are announced. A subsequent focused selection includes its index/count/text and timer state. Pressure does not advance during a selection announcement; there is no per-second speech spam. |
| Unsupported/interrupted narration | No successful completion is invented. The displayed timer waits indefinitely and choices remain manually selectable. A lost completion cannot silently expire a dialogue. |
| Commit/teardown | A commit token is consumed once, and the old UI/state is retired before calling Tales. Dialogue replacement, widget removal, selection and component teardown cancel speech and old timer state. Late, duplicate and superseded utterance completions are rejected by both presentation generation and utterance identity. |

## Narration capability and boundary

`USovAccessibleNarrationSubsystem` is a local-player, owner-scoped in-game narrator. Menus/evidence/dialogue share this service, so newer focused speech interrupts older speech with an explicit **unsuccessful** completion. Each request has a fresh platform TTS object and GUID; stopped objects cannot complete their replacements. Completion is marshalled to the game thread when necessary. Cancellation/factory/speech callbacks may reenter; generation checks preserve the newest request and never overwrite its receipt.

The production adapter uses Unreal's platform TextToSpeech factory and its actual `FOnTextToSpeechFinishSpeaking` callback. It never polls `IsSpeaking()`, estimates spoken duration, treats accessible-name assignment as speech completion, or reports stop/cancellation as completed reading.

The initial source build allowlist is **Win64**, guarded by `SOV_WITH_TEXT_TO_SPEECH`, with the TextToSpeech plugin enabled only there. Even there, a missing platform factory, failed activation or muted backend is unavailable. A backend that accepts a request but never successfully completes it cannot release timed pressure. Other targets intentionally compile the unavailable adapter; adding a platform requires confirming the actual UE 5.7 backend and its packaged behavior before extending the allowlist. No console/private SDK capability is assumed.

This service narrates through the game's Unreal platform TTS backend. It is **not** a claimed completion adapter for external NVDA, JAWS or VoiceOver. Existing UMG accessible names/roles remain available to external readers where Unreal's platform support permits, but those names do not tell the game when an external reader has finished. Users relying solely on an external reader should disable dialogue pressure; completion-gated pressure is supported through the in-game narration setting. Platform readers, input routing and duplicate-announcement behavior require interactive certification.

Official API evidence checked:

- [ITextToSpeechModule](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/TextToSpeech/ITextToSpeechModule): platform-factory creation/access; factory availability depends on allowed platforms.
- [ITextToSpeechFactory](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/TextToSpeech/ITextToSpeechFactory): creation of the engine's speech backend, with no application-side emulation.
- [FTextToSpeechBase](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/TextToSpeech/FTextToSpeechBase): activation, speaking, stopping and the finished-speaking delegate.
- [Successful completion contract](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/TextToSpeech/FTextToSpeechBase/OnTextToSpeechFi-): the engine callback denotes successful speech completion and explicitly excludes interruption/stopping.

Epic's public API currently rolls forward to UE 5.8; the repository targets UE 5.7 and its actual headers/UHT/UBT remain the compatibility authority.

## Validation status

`Tests/Portable/SovDialoguePressurePolicyTests.cpp` compiled and ran with C++17, strict warnings and UBSan in the source workspace. It checks no-pressure default, invalid silence, text gate, required/stale speech, minimum read, pause, extended/disabled timing, resumed speech, invalid delta values and commit-once. This proves only the production-used elapsed-time policy.

Real-engine suites were added under `ProjectVelkorran.UI.Dialogue`:

- `NarrationCompletionOwnership`: real local-player narration subsystem, with a deterministic backend seam, interruption versus completion, late and duplicate callbacks, owner cancellation, reentrant replacement and teardown. The deterministic backend is explicitly not a platform speech certification.
- `NarrationFactoryAndSpeakReentry`: a newer request created by the external backend factory or a failed old `Speak` survives, including when both share the output receipt reference.
- `TalesPresentationRevision`: real Tales replies-available events, early/stale/replaced instance selection rejection and absence of timer readiness without an actual HUD.
- `NativeWidgetTree`: real native UMG/CommonUI construction without assets, complete button set, inherited wrap and rejection of unlaid-out/removed text readiness.

These Unreal suites, UHT and UBT **have not run here**: Unreal Engine is not installed. Monday's required engine gate is:

```text
UnrealEditor-Cmd ProjectVelkorran.uproject -ExecCmds="Automation RunTests ProjectVelkorran.UI.Dialogue" -unattended -TestExit="Automation Test Queue Empty"
```

Then exercise an authored pressure node in the actual Win64 build: keyboard/gamepad/mouse choice selection; all focus changes during a long narration; pause/resume during speech and during countdown; menu/modal coverage; disable/extend timing while choices are live; replace the dialogue during a completion; remove the widget; unavailable/muted/no-completion backend; scale/localization expansion and scrolling; successful silence completion exactly once with the intended existing graph consequences. Repeat in a cooked build and with disabled players. These checks are required validation, not reported passes.

The adjacent `ProjectVelkorran.UI.Frontend` integration suites bind the production native frontend helper to the actual Tales, narrative-cue and combat-damage component delegates, then inspect the native presentation's speech/caption/history. They cover replacement scene ownership, late line/scene completion, producer rebinding, local-avatar damage filtering, teardown, minimum-readable subtitle retirement and asset-free CommonUI Game/Menu/Modal host construction. Test setup bypasses only viewport admission, not the producer bindings or presentation implementation. The first-boot predicate explicitly excludes these headless fixtures; accepted cinematic preparation waits for a real local player's incomplete first-boot setup without charging its loading watchdog.
