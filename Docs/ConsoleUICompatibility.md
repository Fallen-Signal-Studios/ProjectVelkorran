# Console UI compatibility pass

Source pass: 5 September 2026. Targets Xbox Series X|S and the PS5 family, including PS5 Pro. This is source verification, not a console build or certification result.

## Findings and implementation

| Finding | Change | Preserved behavior / dependency |
| --- | --- | --- |
| Native accessibility settings and evidence review did not opt into CommonUI Back handling. Narrative's `bDeactivateOnBack` member alone did not register a Back action. | Both native menu constructors enable CommonUI's built-in back handler. Settings consumes Back during first boot; normal Back follows CommonUI deactivation and existing HDR/cloud cleanup. | Uses the project's CommonUI platform Back mapping, never hardcoded Xbox/PlayStation face buttons. Completing first boot still requires Continue. |
| Settings listened for literal keyboard/D-pad left and right on the row parent, while the focused button and analog navigation could consume them first. A sideways press on a command row could execute Continue, HDR confirmation, or cloud import. | The actual button has custom left/right navigation delegates using Slate's resolved direction. Only value rows adjust. Custom navigation returns no new focus target, allowing a callback's newly opened modal to retain focus. | Native button activation still uses Slate/CommonUI platform accept handling. Up/down navigation, user wrap preference, transactional settings and focus narration remain in place. |
| Long evidence/history text preceded all controls in one scroll box. Moving focus to a control could skip the start of the text, and a controller could not independently scroll its middle. | Controls are fixed above a separately scrollable body. Right-stick vertical input scrolls the summary, with a dead zone, finite input validation, bounds and a 50 ms elapsed-time cap. Page Up/Down and mouse wheel also work. Changed records start at their beginning. | Existing protagonist knowledge checks, scene history ownership and narration remain the source of the text. No additional evidence store or permanent input listener. |
| Native speech/caption positions and wrapping used the entire viewport and ignored TV safe-area padding. | A native `USafeZone` wraps the speech/caption canvas. Pagination and wrapping use its measured width, including runtime safe-area changes. Live caption relayout does not append duplicate scene history. | Engine/platform safe-area metrics remain authoritative. No guessed console margin or resolution. |
| World-space marker labels could run past the screen edge. Moving the entire HUD into a padded coordinate space would misalign weak-point and interactable outlines. | Only label positions are constrained and clipped to the measured safe area. World projections and target outlines retain full player viewport coordinates. | Existing reveal, reach, line-of-sight and screen-space marker domain rules remain authoritative. Oversized world labels are clipped; complete acquired evidence text remains in review. |
| Desktop HDR controls could remain actionable on a console with system-owned display output. | All `HDR.*` controls are disabled with a console display-settings explanation when `IsDisplayOutputSystemManaged()` is true. Preview also checks actual preview/full-calibration capability. Confirm/revert require a live receipt. | Output agent owns the settings capability and console render safeguards. UI cannot bypass the settings transaction owner. |
| A console's native save backend must not be presented as the optional generic manual cloud provider. | Cloud rows explain platform save-system ownership when `IsCloudManagedByPlatform()` is true. Existing `IsCloudAvailable()` guards disable the manual cloud actions. | The platform/save subsystem owns this capability and rejects manual cloud operations before transport. |

## Source files

- `Source/ProjectVelkorran/Public/UI/SovAccessibilitySettingsMenu.h` and `Private/UI/SovAccessibilitySettingsMenu.cpp`
- `Source/ProjectVelkorran/Public/UI/SovAccessibleRecordMenu.h` and `Private/UI/SovAccessibleRecordMenu.cpp`
- `Source/ProjectVelkorran/Public/UI/SovAccessibilityPresentation.h` and `Private/UI/SovAccessibilityPresentation.cpp`
- `Source/ProjectVelkorran/Public/UI/SovConsoleUIPolicy.h`
- `Source/ProjectVelkorran/Private/Tests/SovAccessibilityFrontendRuntimeTests.cpp`
- `Tests/Portable/SovConsoleUIPolicyTests.cpp`

## Validation

Executed: the production scroll policy compiled as C++17 with `-Wall -Wextra -Werror -pedantic -fsanitize=undefined -fno-sanitize-recover=all` and passed dead-zone, direction, edge clamp, long-record reachability, NaN/infinity and long-resume-delta checks. `git diff --check` passed.

Added, not executed here: `ProjectVelkorran.UI.Console.BackAndFirstBoot`, `NavigationAndSafeArea`, and `SystemManagedHDR`. These exercise native first-boot Back semantics, actual widget-tree/control navigation wiring, stale-menu navigation, and disabled console HDR writes. UE 5.7/UHT/UBT are not available in this environment.

Required device/UE validation:

1. Compile UE 5.7 for the approved Xbox and PlayStation platform toolchains; run the three native tests and existing accessibility/frontend tests.
2. On both console families, open first boot without a keyboard/mouse, adjust values with D-pad and stick, traverse all rows and confirm Continue. Back must not bypass setup; after setup it must close the native settings and review screens.
3. Verify the configured CommonUI accept/back actions and controller glyphs for each platform. Those input-data assets are not present in this source snapshot. No title-specific platform binding or glyph asset is invented by this patch.
4. At 1080p and 4K output, minimum/maximum text scales, system safe-area extremes and runtime display changes, read multi-page subtitles/captions and the entire longest acquired record. Record controls remain reachable, text scrolls while gameplay is paused, and no content jumps across a suspend/resume.
5. Reveal weak points and focus interactables near every screen edge. The target outline must stay on its world target while its label remains within the safe area. Repeat with a different viewport DPI scale.
6. Open review over settings, close it using platform Back, and verify CommonUI restores the correct settings focus without activating another row. Disconnect/reconnect the active controller and repeat after system UI/suspend.
7. On console, HDR rows explain system ownership and cannot start a desktop calibration transaction. On desktop, output-only SDR preview and supported full HDR preview still commit/revert through the existing receipt.

Public API references: [CommonUI input routing and navigation](https://dev.epicgames.com/documentation/unreal-engine/commonui-input-technical-guide-for-unreal-engine), [CommonUI activatable widget behavior](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidget), [UMG custom navigation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/UWidget), and [Slate clipping geometry](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/FSlateClippingZone). Epic's public API index may show a newer version; the project's UE 5.7 compiler and approved console platform source remain the final compatibility gate.
