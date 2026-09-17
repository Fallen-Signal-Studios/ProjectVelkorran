# Adversarial audit: UI/UX, HUD, accessibility, localization and presentation engineering

Reviewer domain 4 of 5. Baseline: local HEAD `f07538c9` (branch `codex/aurelion-tdd-content-20260913`, with the uncommitted Content changes listed in git status). Read-only source review on 17 September 2026. No build, no editor, no runtime capture. Binary Content was only listed or searched for class/function name strings.

TDD: `Docs/Design/Sovereign_Call_Origins_TDD_v2_2026-08-14.md`, sections 13 (UI/UX/accessibility/localization), 14 (visual, animation, VFX, audio, cinematic) and 6.12 (threat readability). Accepted deviations: `Docs/CampaignV2ChangeLog.md` (none of its six entries touch HUD layout, accessibility or presentation). The 2026-09-16 removal of aim-down-sights is deliberate and is not flagged. The HUD design notes (`Docs/AurelionHolographicHUD-2026-09-15.md`, `Docs/AurelionHUDRefinement-2026-09-13.md`) are treated as claims to check, not as evidence.

Severity: **P1** blocks acceptance: an accessibility failure that shuts players out of information, or state corruption. **P2**: a significant defect or missing integration. **P3**: minor. Evidence classes: **SRC** is a defect proven from source; **INT** is missing integration (the code exists but is not wired, or the feature is absent); **RT** needs runtime or Content evidence.

---

## 1. Scope and method

Files read in full or in their relevant parts:

- **UI:** `SovFrontendComponent`, `SovAccessibilityPresentation`, `SovHolographicHUDWidget`, `SovCombatVitalsWidget` and `SovCombatReadinessWidget` (read path), `SovAccessibilitySettingsMenu`, `SovAccessibleRecordMenu`, `SovAurelionPauseMenu`, `SovThreatCueLayout`, `SovCombatHUDQuiet`, `SovPlayerInformationPolicy`, `SovAccessibilityPolicy`, `UI/Dialogue/SovDialoguePresentationComponent`, and `Campaign/SovAurelionWorldPresentation` (the threat overlay).
- **Settings:** `SovGameUserSettings` (.h and validation/load), plus the plugin's `NarrativeGameUserSettings` audio buses and `NarrativeInputSettings`.
- **Narrative and accessibility:** `SovNarrativeCueComponent`, `SovAccessibleNarrationSubsystem`, and the deferred-completion guards in the plugin's `Dialogue.cpp`.
- **Cinematics:** `SovCampaignCinematicComponent` (skip, pause, accessibility gate, event-track policy), `SovAurelionStorySequenceActor`, `SovCinematicPolicy`.
- **Presentation and feedback:** `SovCombatFeedbackComponent`, the `SovCorruptionComponent` presentation request, `SovHapticFeedbackComponent` (surface only), the plugin's `NarrativePlayerCameraManager` (shake and lens comfort).
- **Animation and combat data:** `SovMeleeAttackDefinition` fields, attack classification tags, and a grep for hit stop, hit reaction and music.
- **Engine layering check:** UE 5.7 `SGameLayerManager.cpp` / `.h` (the player canvas is ordered below the viewport overlay; `AddToPlayerScreen` Z order only orders widgets inside the player canvas).
- **Tests:** `SovHolographicHUDRuntimeTests`, `SovAccessibilityFrontendRuntimeTests`, `SovFrontendIntegrationRuntimeTests`, `SovNarrativeCueRuntimeTests` (assertion lists).
- **Config:** `DefaultInput.ini` (Enhanced Input user settings), `DefaultEngine.ini` (settings class). There is no `Config/Localization/*`, and no `Content/Localization`.

Not assessable from source: art, animation, VFX and audio asset quality; authored Blueprint HUD (`WBP_AurelionGameplayHUD`, `WBP_WeaponInfo`); Narrative Pro settings/remap widgets; GameplayCue content (`Content/Cues/*`); Sequencer content; facial and performance capture; mix authoring; music content.

---

## 2. TDD coverage table

| TDD ref | Requirement (abridged) | Engineering evidence | Status |
|---|---|---|---|
| 6.12 | Attack classes with non-color presentation | `Damage_GuardClass_{Standard,Heavy,Unblockable}` tags in `SovMeleeAttackDefinition.cpp:15-16` and ability payloads. Per-enemy anticipation presenters exist (Hound horn anticipation `SovDominionHandler.cpp:563`, drone shot presenter). Off-screen "INCOMING FIRE" plus word and chevron in `SovAurelionWorldPresentation.cpp:173-197`. There is no class→presentation contract and no grab, area-denial or command readability layer in source. | Partial (RT for authored cues) |
| 13.1 | Color never the sole distinction | Text/glyph cues on subtitles, threat cards and markers. The default holographic HUD's ability pips are distinguished by color and outline opacity only (UX2-03). | Partial |
| 13.2 | Combat HUD hierarchy; 6 s fade | Default holographic HUD shows health, shield, Echo, ammo, 6 pips and radar. There is no stamina, poise, companion, corruption state, field-recovery charges or ability binding. The 6 s quiet fade exists only in the disabled `SovCombatVitalsWidget` (UX2-03). | Partial / regressed |
| 13.3 | Protagonist HUD identity, same zones | Per-protagonist palette and letterspaced name (`SovHolographicHUDWidget.cpp:325-363,455-457`). Same placement for both. | Implemented (visual quality RT) |
| 13.4 | Zone layout; safe zone 80–100%; UI scale independent of subtitles | Holographic HUD uses full-viewport geometry with no `USafeZone` (`:215-222`). Vitals are at upper center (TDD says lower left) and abilities at top center (TDD says lower right). This is not recorded in the change log. Subtitle scale is independent (`SovGameUserSettings.h:54,56`). | Partial; collides with text (UX2-02) |
| 13.5 | Enemy UI (health when damaged/focused, boss/poise) | No native source. Presumably Narrative/Blueprint Content. | RT |
| 13.6 | Pause menu entries (Mission, Techniques, Equipment, Evidence, Map, Tutorials, Settings, Save/Load, Main Menu) | Native Aurelion pause has Resume / Load checkpoint / Settings / Quit (`SovAurelionPauseMenu.cpp:46-54`). Evidence, objectives and history are reachable only inside Settings (`SovAccessibilitySettingsMenu.cpp:174-176`). | Partial |
| 13.7 | Tutorials (replayable, binding-adaptive) | No tutorial system in source (grep "tutorial": none). | Missing (UX2-09) |
| 13.8 | Subtitles: scale, background, speaker name/color/pattern, direction, captions, line limits, cinematic/ambient placement, history; choice minimum read; screen-reader announcements | All present in `SovAccessibilityPresentation` (pagination by grapheme/line break, speaker glyph pattern, `[left]`/`[right]`, anchors .8/.9), history review (`SovAccessibleRecordMenu.cpp:167-183`), choice pressure/narration (`SovDialoguePresentationComponent.cpp:128-134,267-291`). Speaker color is not implemented (pattern only). Per-language line validation is absent. Interruption defect: UX2-01. | Mostly implemented |
| 13.9 | Accessibility feature set | See section 3. | ~50% of line items consumed |
| 13.10 | Menus without pointer; subtitle collision tested against HUD/ultrawide/languages; settings before opening cinematic, saved immediately | Controller navigation and custom left/right rows (`SovAccessibilitySettingsMenu.cpp:92-96`). Cinematic waits for first-boot setup (`SovCampaignCinematicComponent.cpp:606-617`). Immediate persistence (`SovGameUserSettings.cpp:96-104`, apply path). Collision against the default HUD is not handled or tested (UX2-02). | Partial |
| 13.11 | String-table IDs, no concatenation, 30–40% expansion, plural/gender variants, localized launch markets | One code string table (`SovHUDStyle.cpp:10-13`). LOCTEXT elsewhere. No gather config, no cultures, FString error text shown to players, plural-insensitive formats (UX2-08). | Weak |
| 14.1–14.3 | Visual target, character and weapon art rules | Production content. | Not assessable |
| 14.4 | Animation architecture (motion matching, warping, control rig, physical animation, IK, additive layers, sync markers) | Plugin links `PoseSearch` and `MotionWarping` (`NarrativeArsenal.Build.cs:49,72`). Project animation source is only `SovDismembermentCopyPoseAnimInstance.cpp`. Everything else is AnimBP/Content. | RT |
| 14.5 | Combat animation declarations | `FSovMeleeAttackNode` declares startup/active/recovery, branch windows, trace sockets, classifications, charge, aim correction (`SovMeleeAttackDefinition.h:13-41`). No root-motion policy, super-armor windows, AV/camera requests, warp bounds or mirroring fields. | Partial |
| 14.6 | Hit reaction selection; hit stop 35–90 ms with accessibility setting | No hit-stop / time-dilation code in project or plugin; no hit-reaction selector in source. `Content/Cues/TakeDamage/GC_TakeDamage*` (untracked) may do reactions. | Missing (hit stop) / RT (reactions) (UX2-10) |
| 14.7 | VFX priority, budgets, reduced variants | `SovCombatFeedbackComponent` break-over-impact priority, local/world budgets, reduced Niagara variants with fallback (`:45-64,157-183`). `NS_Aurelion_*_Reduced` assets exist. | Implemented for its narrow scope |
| 14.8–14.9 | Audio readability: priority, concurrency, ducking, caption counterpart; event families | Bus sliders including tinnitus and dynamic-range mixes (plugin `NarrativeGameUserSettings.cpp:93-121`). Bark arbiter priority/cooldown. Captions only from resolved damage and one sweep scanner (`SovFrontendComponent.cpp:350-361`, `SovAurelionSweepScanner.cpp:193`). Echo, mark, corruption, ability-ready and off-screen audio have no native caption producer. | Partial |
| 14.10 | State-driven music | No music system in source. | Missing / RT |
| 14.11 | Line IDs, takes, subtitle timing, localization state in one record; combat line variants | Aurelion story cues carry only speaker, text and timing (`SovAurelionStorySequenceActor.h:12-23`). Narrative Tales owns in-world lines. | Weak |
| 14.12, 14.14 | Cinematic tiers; canon gates | No tier or canon-gate metadata in the cinematic contract. 18 Aurelion LS assets exist. | Content/process; no engineering model |
| 14.13 | Cinematic integration: validate participants, prestream, input, pause, skip after full view, apply state on skip, recover from missing actor, subtitle sync, localization handles | `USovCampaignCinematicComponent` implements participant validation, partition prestream, event-track prohibition, full-view accounting, a skip commit path, abort on missing participant, and sequence-clock subtitles. There is no `USovCinematicSubsystem`. Skip and pause have no production caller (UX2-06). | Substantially implemented, not wired to player |
| 14.15 | Photo mode (BASELINE OPTIONAL) | Absent. Acceptable cut per TDD. | Cut (not a finding) |

---

## 3. Accessibility feature table (TDD 13.9)

"Consumed" means a production runtime reader outside settings/menu/test code, verified by grep.

| Feature | Setting exists | Consumed at runtime (cite) | Tested | Verdict |
|---|---|---|---|---|
| Complete remapping | Enhanced Input user settings on (`DefaultInput.ini:97-98`); plugin `WBP_InputMapping` Content | Engine-owned. The native settings menu has no remap row (`SovAccessibilitySettingsMenu.cpp:159-242`), and the Aurelion pause menu opens only that menu (`SovAurelionPauseMenu.cpp:212-219`) | No | RT / likely unreachable in slice (UX2-07) |
| Hold/toggle/tap alternatives | `bTapInteractions`, `bToggleAim/Guard/Sprint/AbilityModifier` | `PlayerInteractionComponent.cpp:254`; `NarrativePlayerController.cpp:928-933` | Partial | Yes |
| Adjustable hold duration | `InteractionHoldScale` | `PlayerInteractionComponent.cpp:253` (interactions only; charged melee `FullChargeSeconds` unscaled) | Menu only | Partial |
| Simultaneous-input reduction | `bToggleAbilityModifier` | `NarrativePlayerController.cpp:928` | No | Partial |
| Combat input buffering assistance | `InputBufferAssistanceSeconds` | `NarrativeCombatInputBuffer.cpp:88` | Not seen | Yes |
| Rapid-press replacement | none | — | — | Missing (may be N/A if no mash inputs exist) |
| One-stick camera / auto-camera strength | `AutoCameraStrength` | `SovTargetingComponent.cpp:212,219` | Not seen | Partial (no one-stick mode) |
| Aim assist, snap, friction, lead | `Melee/RangedAimAssistStrength`, `bAimSnap`, `bProjectileLead` | `SovGameplayAbility_Melee.cpp:237`, `NarrativePlayerController.cpp:354,371`, `SovTargetingComponent.cpp:237`, `SovAimAssist.cpp:80` | Not seen | Yes |
| Defense-window assistance | `DefenseWindowScale` | `SovGameplayAbility_Exertion.cpp:151` | Not seen | Yes |
| Optional auto-sprint | `bAutomaticSprint` | `NarrativePlayerController.cpp:1051` | Not seen | Yes |
| Menu wrap and focus memory | `bMenuNavigationWrap` | `NarrativeMenu.cpp:37,86,91`; `SovAccessibleRecordMenu.cpp:200` | Yes (nav test) | Yes (focus memory unverified) |
| Vibration intensity by channel | `FSovHapticSettings` 6 channels | `SovHapticFeedbackComponent` (damage/death/interaction/sequence producers) | Platform-output tests | Yes (UI/ambience producers not seen) |
| Scalable UI and text | `UIScale` 1–2, `SubtitleScale` 1–2.5 | Presentation, holographic HUD (`:743`), menus, threat card (`SovAurelionWorldPresentation.cpp:150`) | Yes | Yes, but scaling causes HUD/text collision (UX2-02) |
| High-contrast HUD | `bHighContrastHUD` | Presentation, holographic, threat card, vitals, dialogue choice | Yes | Yes |
| Color-vision presets + independent team/threat colors | `ColorVisionPreset`, `Team/ThreatColor` overrides | Presets only in `SovAccessibilityPresentation.cpp:91-102` (world markers/outlines). Threat override only at `SovAurelionWorldPresentation.cpp:117-119`. The holographic HUD and radar ignore both. | Tint unit test | Partial (UX2-05) |
| Non-color attack/status signals | n/a | Threat card words and chevron; caption text; speaker glyph pattern | Partial | Partial (pip states, UX2-03) |
| Navigation contrast/pulse | `bNavigationContrast/Pulse` | `SovAccessibilityPresentation.cpp:661-662,716-717` | Not seen | Yes |
| Interactable and weak-point outlines | `bInteractableOutlines`, `bWeakPointOutlines`, `OutlineThickness` | `:700`, `:473`, `:616-617` | Partial | Yes |
| Reduced bloom/lens/CA/DOF/grain | `bReduceLensEffects` (+ plugin bloom/motion-blur toggles) | `NarrativePlayerCameraManager.cpp:94-107` | Not seen | Yes (plugin toggles not in native menu) |
| Brightness/HDR calibration | HDR preview/confirm/revert rows; plugin gamma | `SovGameUserSettings` HDR transaction; gamma is a plugin setting | Yes (HDR) | Yes (HDR); SDR brightness not in native menu |
| Screen reader for menus, evidence, choices | `bMenuNarration` + `SetAccessibleBehavior` on buttons | `SovAccessibleNarrationSubsystem` (TTS only when `SOV_WITH_TEXT_TO_SPEECH`, Win64: `ProjectVelkorran.Build.cs:46-48`); choice announcer `SovDialoguePresentationComponent.cpp:267-335` | Fake backend tests | Partial (platform-limited) |
| Complete subtitles for speech | `bSubtitles` | Tales lines, cue barks, story cues | Yes | Defect on bark-during-suspension (UX2-01) |
| Closed captions for gameplay-critical sound | `bClosedCaptions` | Damage results (`SovFrontendComponent.cpp:350-361`); sweep scanner | Priority test | Partial: narrow producer set |
| Direction indicators | `bSubtitleDirections` | `SovAccessibilityPresentation.cpp:395-403`; threat card sides | Not seen | Partial: wrong direction for sourceless events (UX2-12) |
| Separate sliders incl. tinnitus, controller audio | Audio.* rows, `ControllerAudioVolume` | Plugin sound-class mix (`NarrativeGameUserSettings.cpp:93-111`); `SovNarrativeCueComponent.cpp:170,204` | `SovAudioSettingsRuntimeTests` | Yes (sound-class assignment RT) |
| Visual alternatives for parry, mark, corruption, shield-break, off-screen | n/a | Parry/deflect/shield-break captions; off-screen threat card (Aurelion only). No mark or corruption visual/caption consumer in native source. `OnPresentationRequested` has no native listener (`SovCorruptionComponent.cpp:532,537`). | Partial | Partial |
| Dynamic-range presets | `Audio.DynamicRange` | Plugin mix submit (`:113-121`); availability-gated | Yes | Yes (mix assets RT) |
| Objective reminder and path assist | `bShowObjectiveText` | Objective panel and waypoint (`SovAccessibilityPresentation.cpp:498-514,635-699`) | Yes | Yes |
| Tutorial replay | — | — | — | Missing (UX2-09) |
| Evidence summaries fact vs. interpretation | n/a | `SovAccessibleRecordMenu::DescribeEvidence` | Yes | Yes |
| Puzzle hint cadence / direct solution | — | — | — | Missing (RT whether puzzles exist) |
| Pause in cinematics and gameplay | System pause | Aurelion pause menu `AcquireSystemPause`; presentation freezes when paused (`SovAccessibilityPresentation.cpp:447`); dialogue completion deferred while paused (plugin `Dialogue.cpp:465-474`). `SetCinematicPaused` is unused. | Pause-menu tests | Partial (UX2-06) |
| Motion blur, camera shake, head bob, hit flash, corruption distortion | `bDisableCameraShake`, `bReduceCorruptionEffects`; plugin motion blur | `NarrativePlayerCameraManager.cpp:29`; `SovCorruptionComponent.cpp:515` (read only at broadcast) | Partial | Partial: no head bob, hit flash or hit-stop control |
| FOV | Plugin `FieldOfView` | Plugin camera | — | Not in native menu |
| Content warning / reduced body horror | `bReduceCombatEffects` (blood/particles) | `SovBloodFeedbackComponent.cpp:87`, `SovCombatFeedbackComponent.cpp:161` | Yes | Partial |
| Difficulty decomposition | Damage, recovery, defense window, aim, navigation separate | Consumers above; `IncomingDamageScale` via plugin `NarrativeAttributeSetBase.cpp:459` | Settings tests | Mostly (no aggression/puzzle axis) |

Summary: 36 line items. 16 are consumed and wired, 13 partial, 7 missing or not reachable. That is roughly 50–55% weighted.

---

## 4. Findings

### UX2-01. A bark that interrupts cue-owned dialogue plays with no subtitle (prior UI-01, still open)

- **Severity:** P1. **Class:** SRC. **Confidence:** high.
- **TDD:** 13.8 and 13.9 (complete subtitles for speech), 14.8 (caption counterpart), 9.13.
- **Failure sequence:**
  1. The cue arbiter starts a suspendable conversation it owns.
  2. Combat begins, or a higher-priority bark wins. The arbiter calls `OwnedDialogue->SetPlaybackSuspended(true)` (`Source/ProjectVelkorran/Private/Narrative/SovNarrativeCueComponent.cpp:216` and `:245`), but Tales' current dialogue stays non-null.
  3. `StartRequest` plays the bark audio and broadcasts `OnCueStarted` (`:161-188`).
  4. `USovFrontendComponent::OnCueStarted` returns immediately because `BoundTales->GetCurrentDialogue()` is non-null (`Source/ProjectVelkorran/Private/UI/SovFrontendComponent.cpp:333`).
- **Result:** the bark is voiced and not subtitled. Meanwhile the suspended NPC line, presented with duration `-1` (`:299`), stays on screen under the wrong speaker, because pages only advance once `bFinished` is set (`SovAccessibilityPresentation.cpp:451`). Critical ObjectiveCritical barks are the ones allowed to interrupt, so this is the worst case for deaf players.
- **Tests:** `SovFrontendIntegrationRuntimeTests.cpp:120-121` broadcasts `OnCueStarted` with no current dialogue, so it cannot catch this.
- **Fix inside existing owners:**
  - The frontend should gate on "Tales dialogue active and not `IsPlaybackSuspended()`", or on the arbiter's effective speech owner, rather than "any current dialogue".
  - Preserve the suspended entry with `RetireSpeechPresentation` semantics plus re-present on `OnDialogueSuspensionChanged(false)`. `SovDialoguePresentationComponent.cpp:39` already binds that delegate for choices.
  - Add an integration test with real `SovNarrativeCueComponent` suspension feeding the frontend.

### UX2-02. The default holographic HUD paints over subtitles and captions, and ignores the safe zone

- **Severity:** P1. **Class:** SRC geometry; RT to confirm exact pixels. **Confidence:** medium-high.
- **TDD:** 13.4 (safe zone 80–100%), 13.10 (subtitle collision tested against HUD), 13.9 (scalable text).
- **Evidence:**
  - Holographic HUD is added at player-screen Z 50 (`SovFrontendComponent.cpp:56-60`). Presentation (subtitles and captions) is added at Z -1 (`:77-79`). Both sit in the same player canvas, so the HUD draws on top.
  - The HUD root is a bare `SBox` with full-viewport geometry and no `USafeZone` (`SovHolographicHUDWidget.cpp:215-222`). The design note records that a SafeZone root was reverted.
  - The Echo arc runs from (.175W, .862H) through control (.56W, .942H) to (.965W, .80H), drawn with 16/15/12 px × UI-scale strokes plus a centre "ECHO" label (`:584-628`). Its midpoint is about (.565W, .887H).
  - All Tales NPC and player lines and all Aurelion story lines are presented as cinematic (`SovFrontendComponent.cpp:299,313`; `SovAurelionStorySequenceActor.cpp:102`). Cinematic lines anchor the subtitle box bottom at 90% of safe height (`SovAccessibilityPresentation.cpp:115,422`). The last subtitle line therefore sits at roughly .86–.89H, directly under the arc's centre.
  - The radar disc (up to 175 px × scale, opacity .78, `:637-644`) spans .06–.35W and .59–.88H, overlapping the left side of wide or multi-line subtitles.
  - Captions anchor at .13H (`:119`). The identity plate spans .022H to .022H + 96 px × UIScale (`:445-447`). At 1080-unit height the plate bottom reaches 144 at UIScale 1.25 and 216 at UIScale 2, covering the caption's content line. The players most affected are those who raise UI scale.
  - The threat overlay's "AHEAD" card sits at .10H centre (`SovThreatCueLayout.h:42`). It avoids only `CombatVitals` and presentation panels (`SovAurelionWorldPresentation.cpp:135-145`). `GetCombatVitals()` is null whenever the holographic HUD is up (`SovFrontendComponent.cpp:65`), so the card lands on the health plate. In high contrast the card backing is opaque (`:159-160`) and hides the health bar at UIScale ≥ 1.5.
- **Failure sequence:** a player with default settings talks to any NPC, and the Echo arc and "ECHO" label draw through the last line of every subtitle. A low-vision player sets UIScale 2, a shield breaks, and the "Shield broken" caption is hidden under the identity plate. This also holds on a TV at 90% safe area, where the plate and ammo readouts additionally sit outside the title-safe region.
- **Tests:** none. `SovHolographicHUDRuntimeTests.cpp` covers snapshot, palette and hide-tag only.
- **Fix inside existing owners:**
  - Host the holographic surface in the presentation's safe area (a `USafeZone` root, or paint within `SafeTextCanvas` bounds).
  - Publish its occupied rectangles through the same geometry-reader pattern the presentation uses (`GetObjectivePanel`/`GetSubtitlePanel`).
  - Have `LayoutObjectives`, subtitle anchoring and `SovThreatCueLayout::AvoidPanels` treat those rectangles as occupied, or suppress the arc and radar while speech or captions are visible.
  - Order text above the HUD.
  - Add a collision test at UIScale 1/1.5/2, SubtitleScale 2.5, and safe zones of 80% and 100%.

### UX2-03. The holographic HUD replaces labelled readiness with color-only pips and drops required HUD information

- **Severity:** P2. **Class:** SRC and INT. **Confidence:** high.
- **TDD:** 13.1 (color never sole distinction), 13.2 (always-available list, 6 s fade), 13.7 (prompts adapt to bindings).
- **Evidence:**
  - `bShowHolographicHUD = true` and `bShowCombatVitals = false` by default. The vitals and readiness widget stands down whenever the holographic HUD is up (`SovFrontendComponent.h:33-35`; `SovFrontendComponent.cpp:65`).
  - The retired path displayed per-ability name, remapped binding, status text ("Needs 40 Echo", "Input locked", "Equip weapon"), companion health and co-action state, stamina, poise, and a 6 s quiet fade (`SovCombatReadinessWidget.cpp:155-190`; `SovCombatVitalsWidget.cpp:196-258`).
  - The holographic pips show only filled-or-outlined (`SovHolographicHUDWidget.cpp:509-532`):
    - EchoReady and Active differ by gradient color and outline alpha (.95 vs .45).
    - In high contrast both fills become white (`:338-339`).
    - NeedsEcho, InputLocked, Unbound, WeaponRequired and Unavailable are visually identical.
  - Readiness produces at most three slots (`SovCombatReadinessWidget.cpp:136-138`) drawn into six fixed pips, so three pips are permanently empty and read as "unavailable".
  - There is no stamina, poise, corruption state, companion sidecar or field-recovery charge, and no fade. The surface stays up at full opacity whenever vitals are readable (`:278-302`).
  - The design note claims "Text labels stay. Each resource keeps a short label". The code draws only the name plate and "ECHO". Shield and health carry glyphs but no labels or values.
- **Failure sequence:** a player who remapped Ability 2 cannot see its binding. A color-blind player in high contrast cannot tell a ready ability from an active one. A player at 0 stamina gets no indicator.
- **Fix:**
  - Reuse `FSovAbilityHUDEntry::Name/Binding/Status` and `Icon` (already read every frame into `Displayed.Readiness`) for a short per-pip glyph plus binding label, with distinct shapes per state.
  - Draw only the granted slot count.
  - Add stamina (when changing), poise and companion text from the existing readers.
  - Drive opacity from `SovCombatHUDQuiet::FState`, which is already implemented.
  - Record the layout-zone change in `CampaignV2ChangeLog.md` if it is a creator decision.

### UX2-04. Default HUD bypasses the Blackout modifier through the radar

- **Severity:** P3. **Class:** SRC. **Confidence:** high.
- **TDD:** modifier integrity; 13.2.
- **Evidence:** Blackout is described as "no navigation or threat markers" (`SovAccessibilitySettingsMenu.cpp:202`) and is honoured by the presentation (`SovAccessibilityPresentation.cpp:473,637,712`). The holographic radar draws hostile contacts regardless (`SovHolographicHUDWidget.cpp:251-254,693-709`).
- **Fix:** skip contacts when `Displayed.Settings.bModifierBlackout` is set.

### UX2-05. Color-vision presets and threat/team color overrides do not reach most color-coded surfaces

- **Severity:** P2. **Class:** INT. **Confidence:** high.
- **TDD:** 13.9 (color-vision presets plus independent team/threat colors), 13.3.
- **Evidence:**
  - `ColorVisionPreset` is read only by `TeamTint`/`ThreatTint` (`SovAccessibilityPresentation.cpp:91-102`), which feed world markers and the interactable outline.
  - The holographic HUD hard-codes red/amber for Tarrik health, green/cyan for Selene, and radar hostiles in `HealthTo` (`SovHolographicHUDWidget.cpp:346-361,702-707`). It never reads the preset or the overrides.
  - The threat card uses fixed amber unless the override is on, and ignores the preset (`SovAurelionWorldPresentation.cpp:116-119`).
  - There is no engine color-deficiency correction (no `ColorVisionDeficiency` usage).
- **Failure sequence:** a protanopia player picks "Protanopia" and sees no change to health bars, radar hostiles or threat cards.
- **Fix:** route the holographic palette and threat accent through `TeamTint`/`ThreatTint`, or through a `SovHUDStyle` palette that takes the snapshot. Optionally expose the engine's Slate/renderer color-deficiency correction as a preset.

### UX2-06. Cinematic skip and pause have no production caller

- **Severity:** P2. **Class:** INT. **Confidence:** high for source; RT for Blueprint.
- **TDD:** 14.13 (allow pause; skip after first complete viewing), 13.9.
- **Evidence:**
  - `USovCampaignCinematicComponent::RequestSkip` (`SovCampaignCinematicComponent.cpp:737-745`) and `SetCinematicPaused` (`:714-723`) are referenced only by tests.
  - A binary string search of `Content/` finds neither name. The only production entry is `RequestPlay` from `SovAurelionRequestActor.cpp:287`.
  - `ESovAurelionRequest` has no skip value (`SovAurelionRequestActor.h:24`).
  - Pause during a scene relies on world system pause. Whether the Level Sequence player honours world pause, and whether `ObservePlaybackProgress` (world-time based, `:727-735`) then stays eligible, needs a runtime check. If the sequence advances while world time is frozen, `ValidProgress` fails and the first viewing aborts (`:750-752`).
- **Fix:** bind a Back/Skip input (hold-to-skip, with toggle alternative) in the frontend or pause flow to `RequestSkip` when `CanSkipCinematic` is true. Route the pause menu to `SetCinematicPaused` while the phase is Playing, and cover pause-resume full-view eligibility with a test.

### UX2-07. Remapping and several comfort settings are not reachable from the native menus used by the Aurelion slice

- **Severity:** P2. **Class:** INT; RT for Blueprint menus. **Confidence:** medium.
- **TDD:** 13.9 (complete remapping, FOV, motion blur, brightness), 13.10 (settings available before the opening cinematic).
- **Evidence:**
  - The native settings menu row list (`SovAccessibilitySettingsMenu.cpp:159-242`) has no key remap, camera sensitivity/invert, FOV, motion blur, bloom or SDR gamma rows.
  - These exist only as plugin settings (`NarrativeInputSettings.h`, `NarrativeGameUserSettings.h:160-198`) and plugin Content (`W_Settings_Input`, `WBP_InputMapping`).
  - The Aurelion pause menu offers only the native menu (`SovAurelionPauseMenu.cpp:51-52,212-219`), and the first-boot setup is the same native menu (`SovFrontendComponent.cpp:115-117`). A player cannot remap before the opening cinematic, as 13.10 requires.
- **Fix:** add "Controls" and "Display comfort" entries that open the existing Narrative input and settings widgets from `USovAccessibilitySettingsMenu` (via `HUD->OpenMenu`), or add typed rows against `UNarrativeInputSettings`. No parallel store is needed.

### UX2-08. Localization pipeline absent; player-visible FString text and plural-insensitive formats

- **Severity:** P2. **Class:** SRC and INT. **Confidence:** high.
- **TDD:** 13.11.
- **Evidence:**
  - No `Config/Localization/*.ini` gather targets, no `Content/Localization`, and no culture config in `Config/*.ini`.
  - Player-visible errors are FString literals shown through `FText::FromString`. Examples: `SovAccessibilitySettingsMenu.cpp:437,457,467,476,516`, `SovAurelionPauseMenu.cpp:209`, and save/recovery messages such as `SovSaveSubsystem.cpp:123,140,201`, which reach the pause and settings Status text.
  - The checkpoint timestamp uses a fixed `%Y-%m-%d %H:%M:%S` format (`SovAurelionPauseMenu.cpp:122-124,170-172`), whereas the settings menu correctly uses `FText::AsDateTime` (`:299`).
  - Plural-insensitive formats: "{0} more objectives" (`SovAccessibilityPresentation.cpp:355`), "{1} choices" (`SovDialoguePresentationComponent.cpp:272`), "INCOMING FIRE {0}" (`SovAurelionWorldPresentation.cpp:183`).
  - Identity letterspacing splits UTF-16 units and cuts at '/' (`SovHolographicHUDWidget.cpp:151-165`), which breaks for combining marks, CJK and RTL scripts.
  - Width heuristics assume 27 px per character (`SovAccessibilityPresentation.cpp:184,227`).
  - Aurelion story cues have no line or string-table IDs (`SovAurelionStorySequenceActor.h:12-23`).
- **Fix:**
  - Convert result/error channels to `FText` with LOCTEXT at the producer, or map error enums to FText at the UI.
  - Use ICU plural forms, and `FText::AsDateTime`.
  - Letterspace with a grapheme iterator, or drop letterspacing for non-Latin cultures.
  - Add a gather config and at least one pseudo-localized culture for expansion and collision tests.
  - Give story cues string-table keys.

### UX2-09. Tutorials (13.7) are not implemented in source

- **Severity:** P2. **Class:** INT. **Confidence:** high for source; RT for Content.
- **Evidence:** grep for "tutorial" in `Source/` returns nothing, and there is no replay entry in the pause or settings menus (`SovAurelionPauseMenu.cpp:46-54`). Binding-adaptive prompt text exists (`SovCombatReadiness::BindingForInput`, `SovCombatReadinessWidget.cpp:56-92`) but is no longer displayed by the default HUD.
- **Fix:** a record-menu mode listing seen tutorial cards (the `USovAccessibleRecordMenu` pattern already supports modes and narration), fed by a data asset of cards with binding tokens resolved through `BindingForInput`.

### UX2-10. Hit stop and hit-reaction selection (14.6) have no engineering implementation

- **Severity:** P2. **Class:** INT; RT for GameplayCue content. **Confidence:** medium-high.
- **Evidence:** no `TimeDilation`, hit-stop or hit-reaction selection code in project or plugin source. `Content/Cues/TakeDamage/GC_TakeDamage{,_Block,_Heavy}` and `BP_FlinchCameraShake` exist (untracked) and may provide flinch and shake only. There is no accessibility control for hit stop or hit flash.
- **Fix:** a presentation-only hit-stop owner keyed to `FSovDamageResult` severity (35–90 ms), applied through actor-local `CustomTimeDilation` on attacker and target meshes with a policy and a settings scale, never through global dilation. Add a reaction selector reading source direction, poise break, shield-vs-health and airborne state from the same result.

### UX2-11. Unheard-record review is partial: no replay, no knowledge fencing, history persists across protagonist handoff (prior UI-07 residual)

- **Severity:** P3. **Class:** SRC. **Confidence:** medium.
- **TDD:** 9.13, 13.6, 13.9.
- **Evidence:**
  - `SovAccessibleRecordMenu.cpp:171-177` lists all `GetUnheardRecords()` summaries with no `HasKnowledge` or `RequiredProtagonist` filter, unlike evidence (`:187-193`).
  - Scene history clears only when the Tales or cue component itself changes (`SovFrontendComponent.cpp:235-237,399`). A protagonist handoff on the same controller keeps the previous protagonist's conversation lines in "Recent dialogue".
  - There is no replay action.
- **Fix:** filter by the cue's `RequiredProtagonist`/`RequiredKnowledge` against the active protagonist, clear or partition history on active-protagonist change, and offer replay through `RequestCue` when `MatchesContext` holds.

### UX2-12. Direction indicators report a direction for sourceless or stand-in locations

- **Severity:** P3. **Class:** SRC. **Confidence:** high.
- **Evidence:**
  - `FVector::ZeroVector` is passed when the avatar or source is missing (`SovFrontendComponent.cpp:299,313,338,361`).
  - Story cues pass the sequence actor's location (`SovAurelionStorySequenceActor.cpp:102`).
  - `DirectionText` renders "[left]"/"[behind]" for any non-coincident point (`SovAccessibilityPresentation.cpp:395-403`).
  - The location is captured once per line and not updated as the speaker moves.
- **Fix:** add an explicit `bHasSource` to entries, omit direction when absent, and resolve the speaker actor through a weak pointer at refresh.

### UX2-13. Caption queue can present stale warnings, and simultaneous break states collapse to one caption

- **Severity:** P3. **Class:** SRC. **Confidence:** high.
- **Evidence:**
  - Up to 8 queued captions, each with a 3 s minimum (`SovAccessibilityPresentation.cpp:200,210-222,462-468`). A burst can therefore surface "Guard broken" 20 s or more later with no expiry or context check.
  - The `else if` chain reports only one of guard/shield/poise broken in a single result (`SovFrontendComponent.cpp:350-355`).
- **Fix:** give each entry a context deadline (e.g. 2× duration for Routine/Important), and compose simultaneous critical flags into one caption.

### UX2-14. Presentation and frontend do full refresh and layout work every frame

- **Severity:** P3. **Class:** SRC (cost is RT). **Confidence:** high.
- **Evidence:**
  - `USovFrontendComponent::TickComponent` runs `RefreshFrontend` every frame, including while paused (`SovFrontendComponent.cpp:31-33,281-284`).
  - That drives `USovHolographicHUDWidget::RefreshHolographicHUD`. Its `ReadSnapshot` runs `SovCombatReadiness::Read`, which iterates activatable abilities ×3, performs `QueryKeysMappedToAction` per binding and allocates FText, then copies contacts and sets 6 MID parameters every frame (`SovHolographicHUDWidget.cpp:224-257,278-302`).
  - `USovAccessibilityPresentation::NativeTick` calls `RefreshText` every frame (`:469`). That resets fonts, colors and wrap, and runs `LayoutObjectives` with several `ForceLayoutPrepass` calls and FText formats (`:304-394,404-428`).
  - The prior audit's observation stands.
- **Fix:** dirty flags on settings, content, layout size and objective generation; refresh readiness at 10 Hz or on ASC events.

### UX2-15. Any single invalid persisted field resets all accessibility preferences and re-arms first-boot

- **Severity:** P3. **Class:** SRC. **Confidence:** high.
- **TDD:** 13.10 (saved immediately and reliably).
- **Evidence:** `SovGameUserSettings.cpp:71-77` replaces the entire snapshot with defaults and clears `bAccessibilitySetupCompleted` if `ValidateSnapshot` fails for any reason. Examples: a Sovereign preset without the unlock flag, a future schema range change, or one out-of-range color channel. A player relying on SubtitleScale 2.5 loses it silently.
- **Fix:** sanitize per field (clamp or default only the invalid field) and keep local accessibility values when gameplay validation fails.

### UX2-16. Reduced-corruption preference is not re-applied on change, and has no native HUD consumer

- **Severity:** P3. **Class:** SRC; RT for Blueprint consumer. **Confidence:** medium.
- **Evidence:** `GetPresentationRequest` reads the setting only when a broadcast happens (`SovCorruptionComponent.cpp:510-526`). Broadcasts occur on `SetReducedEffects` or replication (`:528-537`), not on `OnUserSettingsChanged`. `OnPresentationRequested` has no native listener. The holographic HUD shows no corruption band or remedy text (13.2 "critical status/corruption state").
- **Fix:** subscribe the corruption component (or the frontend) to settings changes and re-broadcast. Surface band and remedy text in the HUD.

### UX2-17. Combat bark load is synchronous on the critical path (prior V04, still open)

- **Severity:** P2. **Class:** SRC (latency RT). **Confidence:** high.
- **Evidence:** `SovNarrativeCueComponent.cpp:154-155` calls `Variant.Sound.LoadSynchronous()` and `ControllerAudioClass.LoadSynchronous()` before `OnCueStarted` (`:188`). The caption is delayed by the load, and the game thread can hitch in combat.
- **Fix:** as V04 recommended, preload cue bundles and present the caption on acceptance.

### UX2-18. First-boot gate depends on Narrative HUD availability that the cinematic wait does not check

- **Severity:** P3. **Class:** SRC; RT. **Confidence:** medium.
- **Evidence:**
  - `IsInitialAccessibilitySetupPending` returns true for any local viewport until setup completes (`SovFrontendComponent.cpp:36-43`).
  - The menu opens only if `PC->GetNarrativeGameplayHUD()` exists and the campaign transition is Idle (`:113-120`).
  - `WaitForInitialAccessibilitySetup` holds cinematic loading indefinitely while pending (`SovCampaignCinematicComponent.cpp:606-617`).
- **Failure sequence:** on a map or controller without `GameplayHUDClass`, or with the HUD created late, a scene request waits with no visible menu.
- **Fix:** report "setup pending but no host" as a distinct state. Open the setup on a fallback viewport widget, or abort with a message.

---

## 5. Prior-finding dispositions

| ID | Prior summary | Disposition | Evidence |
|---|---|---|---|
| UI-01 | Bark interrupting suspended dialogue has no subtitle | **Still open** | `SovFrontendComponent.cpp:333` still rejects on any current dialogue; the arbiter still suspends at `SovNarrativeCueComponent.cpp:216,245`. See UX2-01. |
| UI-02 | Normal dialogue completion deletes unread pages | **Fixed** | `OnDialogueEnded` now calls `ClearSpeech` (`SovFrontendComponent.cpp:325-330`), and `RetirePreviousSpeech` separates scene replacement (`:364-371`). Tests `SovFrontendIntegrationRuntimeTests.cpp:169-180` (FinalLineSurvivesDialogueEnd). |
| UI-03 (other reviewer) | World pause does not fence dialogue completion | Appears **fixed** (not fully re-audited) | Plugin `Dialogue.cpp:425,465-474,708-711` defers completion while the world is paused. |
| UI-04 | Low-priority captions erase critical ones | **Fixed** (residuals in UX2-13) | `ESovCaptionPriority` plus `CanPreempt` (`SovPlayerInformationPolicy.h:9-10`), priority queue with no routine eviction of critical (`SovAccessibilityPresentation.cpp:188-222`), priority mapping at `SovFrontendComponent.cpp:358-360`, test `SovFrontendIntegrationRuntimeTests.cpp:185-201`. |
| UI-05 (other reviewer) | Account switching retains settings | Still open (observed) | Settings remain a global config singleton with no account key (`SovGameUserSettings.h:204-208`); no account handling in `SovGameUserSettings.cpp`. |
| UI-06 | Removed choice widget leaves dialogue invisible | **Fixed** | `OnWidgetRemoved` resets `SeenRevision` (`SovDialoguePresentationComponent.cpp:171-177`); tick re-presents current replies (`:224-231`). |
| UI-07 | Unheard critical records have no consumer | **Partially fixed** | Record menu lists summaries (`SovAccessibleRecordMenu.cpp:167-178`). No replay and no knowledge/protagonist fencing (UX2-11). |
| UI-08 | Actor-order caps hide weak-point markers | **Fixed** | Roster with round-robin cursor and retained visible set (`SovAccessibilityPresentation.cpp:524-568`, `SovPlayerInformationPolicy.h:13-19`). The 256-per-refresh window still delays discovery by up to ceil(N/256) × 0.25 s; acceptable. |
| V04 | Synchronous bark load | **Still open** | `SovNarrativeCueComponent.cpp:154-155` (UX2-17). |

---

## 6. Alignment estimates

### (a) Section 13: HUD, accessibility, localization: **38–50%, midpoint 44%**

What pushes it up since 2026-09-11:

- Four of five presentation defects in this domain are fixed with real integration tests (UI-02, UI-04, UI-06, UI-08; UI-07 partly).
- The subtitle and caption system is comprehensive: scale, background, speaker pattern, direction, cinematic placement, grapheme and line-break pagination, history review.
- Dialogue-choice pressure honours reading time and narration.
- A broad set of motor and assist settings is genuinely consumed by gameplay (about 16 line items wired).
- High contrast is applied across surfaces.
- HDR and haptic ownership are strong.
- First-boot setup gates cinematics.

What holds it down:

- The new default HUD (P1 collision with subtitles and captions, no safe zone, dropped labels and bindings and stamina/poise/companion/corruption, no fade, color-only pip states).
- A remaining P1 bark-subtitle gap.
- Color-vision presets barely applied.
- Remapping and comfort settings not reachable natively.
- No tutorials.
- Minimal pause menu.
- Localization essentially unstarted as a pipeline.
- Enemy UI unverifiable from source.

The range is wider on the upside if the Blueprint HUD and Narrative settings widgets provide enemy UI and remapping in the shipped flow.

### (b) Section 14: visual, animation, VFX, audio, cinematic

- **Engineering-assessable portion: 35–48%, midpoint 41%.**
  - Strong: the cinematic contract (participant/equipment/inventory validation, partition prestream, event-track prohibition, full-view accounting, skip commit applying state writes, abort on missing participant, sequence-clock subtitles).
  - Implemented: reduced-VFX variants with budgets and break priority; audio buses including tinnitus and dynamic-range presets and controller-speaker routing; bark priority arbitration.
  - Partial: melee frame-data declarations.
  - Missing: hit stop and reaction selection, a state-driven music system, cinematic tier and canon-gate metadata, a line-record model (IDs, takes, localization state), a generic threat-class presentation contract (6.12), and skip/pause wiring (UX2-06). Photo mode is a legitimate optional cut.
- **Whole-section estimate including production content: 17–30%, midpoint 23%.** Art, animation sets, facial capture, VFX look, mix, music and Tier A/B cinematic quality cannot be judged from source. The 18 Aurelion LS assets and reduced-VFX assets show content exists for one slice only. Canon gates beyond the Aurelion missions have no visible cinematic assets. Treat the content share as low and unverified rather than zero.

---

## 7. Verification boundaries

- No runtime capture. UX2-02 pixel overlaps are computed from source constants at a 1080-unit layout height and need one captured frame at UIScale 1/2 with a cinematic subtitle and a critical caption to confirm.
- Blueprint `WBP_AurelionGameplayHUD` may add or duplicate readouts (the design note reports `WBP_WeaponInfo` still live). Enemy health and boss UI, remapping widgets and GameplayCue hit reactions are Content-dependent.
- Tests were not executed. Assertions were read only.

No production files were edited.
