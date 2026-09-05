# Native frontend accessibility engineering

Audit and implementation: 5 September 2026. Authority: TDD v2 sections 9.6 and 13, with the existing Narrative CommonUI framework retained. This closes specific native frontend defects; it does not certify the shipped UI or complete operating-system screen-reader support.

## Findings and implemented behavior

| Gap | Existing implementation and requirement | Change and disposition | Dependencies and risk |
| --- | --- | --- | --- |
| Menu focus memory could be bypassed | `UNarrativeActivatableWidget::NativeOnActivated` called `NativeGetDesiredFocusTarget()` and `SetFocus()` directly. CommonUI already owns restoration of a cached focus target, but this extra forced focus could reset a reopened menu to its default or take focus from a newer modal. TDD 13.9 requires focus memory. | Preserve CommonUI's focus cache and input tree. Replace direct focus with `RequestRefreshFocus()` after registration callbacks. The owning CommonUI layer decides whether it may receive focus and chooses its restoration/default target. There is no second focus manager. | Existing menus must retain CommonUI's Auto Restore Focus setting and provide a valid desired target when there is no cache. Lifetime is the retained activatable instance and CommonUI cache, not a disk-persisted cross-session widget ID. Moderate integration risk for widgets placed outside CommonUI's routed activation tree, which must be integrated with the existing HUD layers. |
| Activation callbacks can retire the requesting menu | Native and Blueprint activation/registration callbacks can close, replace or reactivate the same object. The previous flow continued registering and forcing focus afterward. | Extend the current lifecycle with an activation generation. A retired activation does no further action registration or focus request, even if the same instance has already reactivated. | Existing CommonUI activation lifecycle. Low risk; no new input router or global Slate listener. |
| Menu navigation escaped the outer boundary | `UNarrativeMenu` had no native behavior. TDD 13.9 requires wrap for keyboard/controller menu navigation. | Extend the existing menu root with Slate navigation `Wrap` for escape/default directions, including Next/Previous. Existing child, explicit, custom and Stop rules remain authoritative. Runtime disable releases only still-owned rules; detached roots and replaced navigation objects retire ownership. The option defaults on for menus. | Real widget geometry, visibility, enabled state, focusability and navigation remain owned by Slate/CommonUI. Menus can disable wrap with `SetMenuNavigationWrap`. Moderate presentation risk for unusual multi-column or nested layouts; actual menu navigation must be checked. |
| Native common buttons lacked a consistent accessible name | `UNarrativeCommonButtonBase` only updated its optional visual text block. A generic wrapper or decorative children could provide incomplete/duplicate descriptions. TDD 13.9 requires menu semantics for supported readers. | Extend the existing button with a default UMG accessible-data binding to its full localized `ButtonText`, synchronized through `GetAccessibleWidget()` to CommonUI's real internal Slate button. Decorative descendants are excluded from duplicate announcements by default. An explicitly authored accessibility definition is preserved. Both editor and cooked-data paths are supplied. | Unreal UMG/Slate accessibility must be built/enabled and the platform must provide a reader adapter. This adds accessible names and exposes the existing button role/action; it does not synthesize speech or claim reader completion. Labels still require shipping string-table authoring. |

No shared gameplay settings, player controller, build rules, dialogue graph ownership or save schema were changed by this slice.

## Validation

Three Unreal regression suites were added under `ProjectVelkorran.UI.Menu`:

1. `CommonUIFocusAndReentry`: activation requests routed refresh after registration, avoids direct default-target queries, and does not issue an obsolete request after deactivation or same-instance reactivation.
2. `BoundaryWrapOwnership`: keyboard/controller boundary defaults, authored Stop/wrap preservation, runtime overrides, detached root cleanup, and replacement navigation-object ownership.
3. `NativeButtonAccessibleLabels`: creates real native CommonUI/Slate button state without a widget asset; checks live full-label binding, duplicate-child suppression, the actual Slate accessible label when accessibility is compiled, and authored-label preservation.

`git diff --check` passed for this implementation. Unreal Engine is not installed in this environment. UHT, UBT and these engine tests have **not run**. No portable test is presented as evidence for Slate focus or an OS screen reader.

Required engine gate:

```text
UnrealEditor-Cmd ProjectVelkorran.uproject -ExecCmds="Automation RunTests ProjectVelkorran.UI.Menu" -unattended -TestExit="Automation Test Queue Empty"
```

Use the repository's broader validation script for the full project gate as well. A real CommonUI HUD must then be exercised with keyboard and gamepad: reopen a retained menu at its last selection; open/close nested modals without focus theft; remove or disable a previously selected control; swap populated tab contents; traverse first/last eligible controls in each direction; and verify controls remain reachable at enlarged UI scale. Repeat in a cooked build with platform accessibility enabled. These interactive behaviors are required validation, not reported passing tests.

## Remaining engineering and integration work

- **Implemented in the subsequent source closure:** actual Tales reply presentation/pressure ownership, supported in-game narrator completion/cancellation and safe unsupported fallback. See [Dialogue narration](DialogueNarrationEngineering.md). This is not a completion signal from an arbitrary external OS reader.
- **Implemented native consumers:** first-boot/settings frontend, independent text scaling, palettes/non-color labels, captions/subtitles, navigation/interactable/weak-point markers, and current-protagonist evidence/current-scene history readout. See [Native accessibility frontend](NativeAccessibilityFrontendClosure.md) and the [source closure handoff](SourceEngineeringClosure-2026-09-05.md).
- **Certification remains.** Validate real supported reader/OS/controller combinations, dynamic and disabled controls, large text, focus order, ultrawide/safe zones, localized expansion and pause/resume timing with disabled players. No source-only pass can certify these outcomes.

Implementation order: adversarially audit the source integrations, run the UE build/native regressions, then validate real menu/reader timing, focus/navigation and localized presentation. Risk remains highest at the asynchronous reader/dialogue boundary and where authored widgets bypass CommonUI.

## API references checked

- [CommonUI routed focus refresh](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidget/RequestRefreshFocus?lang=en-US) and [desired focus fallback](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidget/NativeGetDesiredFocusTarget?lang=en-US).
- [UMG boundary navigation rules](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/UWidget/SetNavigationRuleBase?lang=en-US).
- [UMG accessible data](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/USlateAccessibleWidgetData?lang=en-US) and [CommonUI's internal button](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/CommonUI/UCommonButtonInternalBase?lang=en-US).

Epic's rolling public API pages may display a later engine version; the actual UE 5.7 headers/compiler remain the required compatibility authority.
