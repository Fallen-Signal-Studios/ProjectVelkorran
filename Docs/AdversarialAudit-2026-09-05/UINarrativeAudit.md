# Adversarial domain audit: UI, settings, feedback and Narrative presentation

Baseline: ProjectVelkorran local HEAD `6225c68d50fbea605d7c7dfc8d61d5569805e28f`, source tree matching the published console pass. Read-only review, 5 September 2026. Latest TDD v2.0 is the August 2026 document, not the December attachment. `Docs/CampaignV2ChangeLog.md` exceptions remain authoritative. This report contains source counterexamples and engineering gaps; no Unreal runtime execution, SDK verification or authored Content inspection occurred.

Severity: P1 means a critical source defect or major accessibility/progression risk to resolve before acceptance. P2 means a substantive production defect or integration gap. These are priorities for engineering; content-specific release severity must follow TDD18.11. In particular, missing canon-critical subtitles can be Critical under that rubric.

## UI-01: A bark interrupting suspended dialogue has no subtitle

**Priority/confidence:** P1, confirmed source control flow.

**Current implementation:** `Source/ProjectVelkorran/Private/Narrative/SovNarrativeCueComponent.cpp` uses one speech arbiter. Tick suspends owned Tales dialogue when a higher-priority bark wins, without clearing `UTalesComponent::CurrentDialogue`, and `StartRequest` broadcasts `OnCueStarted` after starting the bark audio. `UDialogue::SetPlaybackSuspended` in `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/Dialogue.cpp:490-513` pauses line timers/audio while retaining the dialogue instance.

**Defect evidence:** `Source/ProjectVelkorran/Private/UI/SovFrontendComponent.cpp:174-181`, specifically176, rejects every cue whenever `BoundTales->GetCurrentDialogue()` is non-null. It does not distinguish active from suspended dialogue. The legal arbiter transition therefore emits audio without a matching subtitle. The frontend also does not bind `OnDialogueSuspensionChanged` to preserve/pause/restore the interrupted subtitle presentation.

**Repro:** Start a cue-owned suspendable conversation; queue an ObjectiveCritical bark able to interrupt that conversation; tick the real arbiter. Assert dialogue is suspended and bark is current. The frontend returns at176, leaving the previous conversation text or no text while the new bark plays. On resume, the original line does not issue another line-start event, so naively allowing bark subtitles without retaining the old line would lose the resumed line instead.

**TDD:** 9.13 interruption priority, subtitle speaker/direction/context retention; 13.9 complete speech subtitles; 14.8/14.9 critical cue parity.

**Treatment:** Extend the existing cue/frontend presentation contract. Expose the arbiter's effective speech owner or a bounded presentation receipt; suspend and preserve the current dialogue entry; present interrupting bark; restore the original entry, page position and remaining reading duration only if the same dialogue/node is still live. Do not create another speech arbiter or merely delete the non-null guard.

**Dependencies/risk/order:** Existing Tales suspension events, cue epoch, frontend speech epoch and presentation queue; medium risk because audio, subtitles and dialogue history share ownership. First presentation repair slice, together with UI-02/04.

**Regression/DoD:** Integration test real Tales + arbiter + frontend + presentation, interruption during NPC line, player line and choices, nested higher-priority bark, dialogue replacement during bark-end callback, and speaker death. Every spoken line has the correct text, identity and direction; resumed line keeps its original state; no duplicate history or obsolete restoration. Existing `SovNarrativeCueRuntimeTests` exercises arbiter and suspension separately; it does not connect the frontend consumer.

## UI-02: Normal dialogue completion deletes unread subtitle pages

**Priority/confidence:** P1, confirmed source control flow.

**Current implementation:** `SovAccessibilityPresentation.cpp:118-135` queues lines and paginates text, giving each page at least two seconds. `ClearSpeech:148-152` deliberately marks the final produced entry finished without immediately discarding readable text. This is a useful contract to preserve.

**Defect evidence:** `SovFrontendComponent.cpp:168-172` handles every `OnDialogueFinished`, including normal completion, by immediately calling `ClearSceneHistory`. `SovAccessibilityPresentation.cpp:154-155` resets `PendingSpeech`, `SpeechPages`, active speech and captions. Thus the minimum-read contract is bypassed at precisely the final line. Actual Narrative `Dialogue.cpp:1497-1515` broadcasts the player-line finish and immediately exits when that node has no next replies; NPC chains similarly end through `PlayNextNPCReply`/`NPCFinishedTalking:1427-1438,1188-1191`.

**Repro:** Configure one line per subtitle page and a long final player response. Finish its voice while later pages remain unread. `ClearSpeech` preserves it momentarily, but the same normal completion stack clears all pages. Earlier queued lines are lost too. The subtitle-only player can miss the final instruction/meaning.

**TDD:** 9.14 subtitle timing covers complete spoken lines; 13.8 full localized layout/readability; 13.9 complete speech subtitles.

**Treatment:** Refactor scene-history retirement separately from active subtitle retirement. A natural end should finish the owned entry and allow its readable pages to drain. Explicit world/protagonist/scene replacement must still retire sensitive old content. Use scene/line receipts so a later finish callback cannot clear successor text. Preserve grapheme-safe pagination but add language-appropriate word/line boundary handling during presentation work.

**Dependencies/risk/order:** Frontend ownership and scene privacy, cue interruptions and subtitle timing; medium risk. Implement with UI-01 in the first presentation slice.

**Regression/DoD:** Drive the real Tales finish-to-frontend chain for final NPC/player lines at maximum subtitle scale and deliberately short VO, including queued prior pages. Assert every page stays visible for the reading minimum and eventually drains; explicit mission/protagonist replacement removes old text immediately. Existing `SovAccessibilityFrontendRuntimeTests.cpp:150-158` tests `ClearSpeech` and `ClearSceneHistory` separately, missing their real consecutive invocation.

## UI-03: World pause does not fence asynchronous dialogue completion

**Priority/confidence:** P1, confirmed source counterexample; the frequency of a real queued audio callback arriving at the boundary remains an engine test question.

**Current implementation:** Cue-owned dialogue can explicitly suspend timers and audio; dialogue guards `bPlaybackSuspended`. The console pass also marks nonspatial dialogue audio as gameplay audio before starting it. These are appropriate but incomplete protections.

**Defect evidence:** `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/Dialogue.cpp:438,1444,1479` checks dialogue suspension/reentrancy/deinitialization but never world pause. `PlayDialogueSound:1824-1826` directly binds `OnAudioFinished` to `EndCurrentLine`; sequence completion also binds that entry point. A callback already queued before pause can execute graph events, dialogue task completion and the next line in a paused world. A sound class configured as UI sound is another path for completion under world pause.

**Repro:** Stage a valid `LD_WhenAudioEnds` line, pause the world without calling the separate `SetPlaybackSuspended`, then deliver the already-issued completion callback. The guard accepts it, and `FinishNPCDialogue`/`FinishPlayerDialogue` runs state changes. This can be tested deterministically by callback injection, without depending on audio-thread timing.

**TDD:** 13.9 pause in single-player cinematics/gameplay; 18.9 suspend/resume stability and pause behavior; 9.13 interruption semantics.

**Treatment:** Extend the existing dialogue completion path with a deferred completion receipt keyed to dialogue lifetime and line revision. While any required pause/suspension applies, queue exactly one completion; replay after safe resume only if still current. Simply returning when paused would discard an audio-finished event and could hang the line forever. Pause guards must cover graph progression, not just the audio renderer.

**Dependencies/risk/order:** Shared application pause ownership, Tales suspension and line identity; high integration risk because replay/deduplication must remain exactly once. Highest-priority narrative correctness slice, before general presentation polish.

**Regression/DoD:** Queued audio/sequence completion immediately before and after world pause; overlay during pause; explicit dialogue suspension plus world pause in both orders; resume after dialogue replacement, player exit or controller destruction; multiple identical callbacks. No tasks/events/selection advance while held, and exactly one valid completion after resume. Existing cue test calls `SetPlaybackSuspended` explicitly, so it does not prove this boundary.

## UI-04: Low-priority damage captions erase critical feedback immediately

**Priority/confidence:** P1 for critical-cue accessibility parity, confirmed source behavior.

**Current implementation:** Frontend derives native combat captions from resolved damage and presents them in a dedicated caption region. `SovFrontendComponent.cpp:187-200` produces perfect-defense, guard-break, shield-break, poise-break, deflection or ordinary incoming-damage text.

**Defect evidence:** `SovAccessibilityPresentation.cpp:137-145` unconditionally overwrites the current caption and resets its timer. There is no cue priority, concurrency key or minimum display ownership. A chip-damage event 50ms after a shield break replaces the critical shield-break text with `Incoming damage`, despite the three-second per-caption duration calculation. The `else if` classification also reports only one result when guard/shield/poise changes occur in the same transaction.

**TDD:** 14.8 every combat-critical event has priority, concurrency, ducking and caption/visual counterpart; 13.9/13.10 critical audio visual parity and 18.7 maximum-density caption testing.

**Treatment:** Extend the existing caption request with semantic identity, priority, source and minimum-readable lifetime. Coalesce repeated low-value damage, preserve/compose simultaneous important result states, and bound queued captions. The priority decision should use the resolved gameplay event, not display text or animation. Preserve the existing presentation component.

**Dependencies/risk/order:** Damage result schema and feedback producers; medium risk, especially caption backlog and excessive screen occupancy. Implement with UI-01/02 and validate under representative combat bursts.

**Regression/DoD:** Shield/guard break followed by chip damage at50ms; simultaneous poise/shield break; multiple directions; repeated low-priority hits; subtitles and closed captions independently disabled; high scale. Critical state receives its configured minimum display period and cannot be erased by a lower-priority request. Assert bounded queue/coalescing and no invalid-source direction. No existing test feeds a real caption event burst.

## UI-05: Account switching retains another account's accessibility and completion settings

**Priority/confidence:** P1, confirmed architecture gap; does not assume proprietary SDK storage behavior.

**Current implementation:** `USovGameUserSettings` is the engine-wide singleton (`SovGameUserSettings.cpp:33-35`). `LoadSettings:55-70` and `PersistSettings:87` use inherited GameUserSettings config. `SovGameUserSettings.h:180,188-192` stores haptics, settings snapshot, campaign completion, diagnostics opt-in and first-boot completion as global config properties. Native campaign save slots have an account authority, but settings do not participate in its changes.

**Defect evidence/repro:** AccountA completes setup and chooses extra-large subtitles, disables vibration or opts in to diagnostics. Select accountB in the same game instance through the existing account-selection flow. There is no account-keyed settings load/reset/fence in Settings or Platform/Save consumers. The live singleton still contains A's preferences and `bAccessibilitySetupCompleted`; B's opening setup can therefore be skipped and B's setting changes mutate the same singleton/config. Campaign completion unlock is also shared in this source. This in-memory counterexample is valid regardless of whether a future SDK virtualizes an underlying config file.

**TDD:** 11.9 explicitly states platform account owns settings and save namespace; 13.10 first-boot availability and immediate persistence.

**Treatment:** Extend existing verified platform owner service and settings facade. Split physical-device values (display/window settings) from account-owned accessibility/input/audio preferences and progression unlock data. Load the current account's schema before its campaign/first-boot gate, fence pending writes when ownership changes, then atomically notify consumers. Use opaque verified namespaces already owned by save/platform services. Do not create another account authority or simply put all HDR/device calibration into cloud campaign payloads. Preserve the deliberate eleven-byte gameplay export and local-accessibility privacy boundary unless a documented product requirement changes it.

**Dependencies/risk/order:** Platform identity/owner loss, persistent settings storage and migration, Enhanced Input user profile identity, frontend setup gate, diagnostics consent; high risk. Coordinate with platform/account repair slice before console sign-in acceptance.

**Regression/DoD:** Two accounts in one process with divergent accessibility, first-boot, diagnostics and completion states; offline valid profile; nonzero user index; signout during write; failed/corrupt profile migration; return to previous account. No account observes another account's profile preferences/consent; physical display calibration stays device-owned; switches cannot save revoked-owner data. Existing LocalSettingsAtomicPrivacy test validates export exclusion and setter transactions, not account isolation.

## UI-06: Removing the native choices widget can leave a live dialogue permanently invisible

**Priority/confidence:** P2, confirmed source recovery counterexample. This does not assert that an ordinary Menu/Modal overlay always deactivates the independent Game layer.

**Current implementation:** Choices carry Tales presentation revision, narration completion and minimum-reading state. The component can retry when the HUD initially does not exist. Native HUD exposes ordinary CommonUI widget stack APIs.

**Defect evidence:** `SovDialogueChoiceWidget.cpp:171-175,188-189` treats deactivation/destruction as presentation removal. `SovDialoguePresentationComponent.cpp:169` then calls `CancelPresentation:153-166`, which resets active presentation but retains `SeenDialogue` and `SeenRevision`. Recovery in `TickComponent:219-221` only calls `RefreshChoices` when the graph emits a different revision. The old graph may still be waiting for exactly the same replies forever.

**Repro:** Present legal replies; deactivate/remove the native choice widget using the supported widget/HUD API without selecting a response or exiting Tales; tick the component with the same current dialogue/revision. The widget stays absent. No native selection or timer can resolve the graph. HUD replacement or same-layer transient UI should be included as realistic engine integration exposures, but no unseen authored flow is asserted here.

**TDD:** 9.13 interruption/recovery; 13.9 focus memory and menus operable without pointer; overall no progression softlocks.

**Treatment:** Distinguish temporary coverage/suspension from actual widget retirement. Permit re-presenting a still-current Tales revision when its owning layer is available; reset the presentation-readiness marker on removal without accepting obsolete callbacks or immediately stealing a modal's focus. Preserve exact reply revision/commit guards.

**Dependencies/risk/order:** CommonUI stack lifecycle, choice input/focus, Tales current revision; medium risk due unwanted reopen loops. Follow the narrative pause/presentation ownership repair in the same vertical slice if feasible.

**Regression/DoD:** Real HUD stack test, direct removal, HUD recreation, same-layer overlay, separate Menu/Modal overlay, removal during narration, stale old-widget callbacks and a legitimate completed choice. Legal current choices recover once; selected/old dialogue never reopens. Current tests check that a removed widget is not ready, not that a live graph regains a usable widget.

## UI-07: Unheard critical-record archive has no native review/replay consumer

**Priority/confidence:** P2, confirmed native integration gap; Blueprint integration in missing Content cannot be verified.

**Current implementation:** Cue data has critical/recoverable summary metadata. `SovNarrativeCueComponent.cpp:94-97` retains authored summaries in a save-backed `UnheardRecords` list and exposes `GetUnheardRecords`. Tests verify list membership. This is useful state to preserve.

**Missing connection:** `SovAccessibleRecordMenu.cpp:119-144` builds either current scene history or acquired campaign evidence. Neither it nor another native production caller consumes `GetUnheardRecords`; source references are its declaration/implementation and tests. There is no native summary row or replay action for this archive.

**TDD:** 9.13 unheard critical lines should appear in Evidence/Records only when diegetically appropriate; 13.6 record interface; 13.9 readable evidence summaries.

**Treatment:** Extend the current record menu and existing archive API with explicit record kind, stable identity and eligibility, including protagonist knowledge/context. Display authored `RecordSummary`; replay through the existing cue arbiter only when legal, retaining record-only access where replay is inappropriate. Do not bypass cue validation or make a second archive. Review whether the archive needs a captured knowledge-owner receipt for cross-protagonist privacy.

**Dependencies/risk/order:** Cue archive, campaign knowledge, record navigation/narration and save restore; medium risk of spoiler leakage or queuing illegal replay. After interruption and settings ownership fixes.

**Regression/DoD:** Interrupt a diegetically recordable critical cue, open native Records and read it without new knowledge leaks; save/reload and reacquire; different protagonist; expired context; illegal replay; non-recordable critical cue. No duplicated entries and no replay bypasses. Authored Blueprint consumer presence must be checked in the full Content checkout before declaring this absent from the executable.

## UI-08: Actor-order limits can permanently hide required weak-point markers

**Priority/confidence:** P2, confirmed deterministic source behavior; first identified independently by root reviewer.

**Current implementation:** `SovAccessibilityPresentation.cpp:220-230` refreshes at .25s and caps scan work to256 Narrative characters and32 weak-point anchors. Outlines default enabled (`SovGameUserSettings.h:63`). Bounded work is good.

**Defect evidence/repro:** `Inspected++ < 256` is evaluated in the iterator loop before distance, relevance and line-of-sight checks. Spawn/register256 irrelevant Narrative actors before an in-range enemy with a revealed weak point. Every scan restarts at the same iterator beginning, so the relevant target may never be inspected despite an otherwise empty marker budget. An identical cap starvation can result from earlier actors filling32 anchors without prioritizing the focused/critical target.

**TDD:** 13.9 weak-point accessibility outlines; 15.13 scalability constraints; 18.7 maximum-density accessibility testing.

**Treatment:** Query existing active encounter/spatial candidates, prioritize focused and mission-critical weak points, then bounded near/visible candidates. If a full actor iterator must remain temporarily, use a fair cursor with explicit refresh deadline and reserve space for the current target. Do not simply remove every cap. Avoid line-of-sight work for actors without relevant weak-point state.

**Dependencies/risk/order:** Existing encounter/candidate ownership, weak-point component registration, streaming unload and local-player focus; medium risk (stale weak references and critical-marker priority). Part of a relevance/performance slice after correctness fixes.

**Regression/DoD:**256 distant actors before near target, streamed actors reordering, crowd saturation, focused target amidst32 earlier anchors, actor destruction during scan. Required focused marker appears within bounded time irrespective of insertion order. Measure query count and CPU on representative targets rather than inventing milliseconds.

## Additional engineering observations and verification boundaries

- **Default critical-caption coverage is narrow.** The only native `PresentCaption` production producer is resolved damage in the frontend. Native presentation does not subscribe to Echo gain/spend, weapon denial, disruption/mark or off-screen pre-attack events. Blueprint/GameCue producers may exist in absent Content. Treat the gameplay-critical feedback matrix as an explicit integration acceptance gate, not a claim that every one of those systems lacks a visual in the full game.
- **Native HUD is scaffolding.** `SovNativeGameplayHUD.cpp` builds three CommonUI layers. It does not by itself implement the TDD health/shield/poise/Echo/ammo/companion HUD. Existing plugin widget/Blueprint binding facilities should be reused. The source-only fallback must not be presented as evidence that the complete combat HUD is playable.
- **Presentation work is unnecessarily repeated.** `SovAccessibilityPresentation::NativeTick` calls `RefreshText` every frame, resetting formatted text/font/color/layout state; marker refresh does potentially up to256 character LOS calls before filtering for weak-point components. No target frame-time failure was measured. Recommend dirty flags for settings/content/layout and event/candidate-driven marker updates; retain a bounded direction update cadence.
- **Unknown source direction is misrepresented.** Frontend passes world origin when no source actor/avatar is available; `DirectionText` interprets that as a real direction. Represent absence explicitly and omit direction for unresolved sources. This is a quality issue independent of the major findings.
- **Settings reflection lacks a typed row schema.** String/FName-based numeric field access, hand-written audio/HDR special cases and one very long menu make omissions and inconsistent enablement easy. Preserve the settings authority; introduce a bounded typed row descriptor with getter/setter/capability/range/action metadata if the menu grows further. Do not build a parallel settings store.
- **Narration platform support is honestly limited.** Completion-aware provider is compiled only where `SOV_WITH_TEXT_TO_SPEECH` is enabled (currently Win64). Unsupported narration disables timer expiry safely, but console menu screen-reader support remains platform API/integration work. A false timer is not an accessibility substitute for a usable console screen reader.
- **Display ownership work is worth preserving.** Existing HDR transaction receipts, config masking during preview, display identity observation and per-CVar conditional rollback are substantially stronger than naive direct CVar changes. Actual UE5.7 API compile, compositor behavior, forced CVar priority changes, hotplug and console system-owned output still need engine/hardware validation. No hardware compatibility conclusion is drawn from portable policies.
- **Haptic ownership work is worth preserving.** Bounded mixer, short native output leases, pause/death/possession cleanup and channel-level settings are coherent. Real motor behavior, actual device routing and controller audio fallback require hardware verification. UI haptic events and content-specific cues are not proven by exposed APIs alone.
- **Existing tests over-index on helpers and direct method calls.** The key missing class is end-to-end producer/arbiter/frontend/Slate tests: test user-visible text and graph state together, including world pause. Settings tests use overridden `SaveSettings`, and narration tests use fake backends. These are useful isolation tests, not persistence/platform/output evidence. Every native test remains uncompiled/unexecuted in this environment.

## Recommended domain slice order

1. **Pause-safe, interruption-safe narrative and presentation.** UI-03 plus UI-01/02/04/06, sharing line/scene/presentation receipts and real integration tests. Definition of done: graph cannot advance while held; critical speech/caption information is preserved; choices recover after legal UI replacement; stale callbacks cannot overwrite successors.
2. **Verified-account settings and accessible recovery records.** UI-05 then UI-07, reusing platform identity and current record interface. Definition of done: preference/consent/first-boot isolation, coherent failed-write behavior and safe access to unheard critical information.
3. **Relevance-based feedback and measured UI cost.** UI-08 plus dirty update scheduling and the complete critical-feedback integration matrix. Definition of done: insertion order never suppresses focused/required cues, meaningful bounded workload, real device/localization/input tests for target builds.

No production files were edited. This domain report forms part of the consolidated adversarial audit.
