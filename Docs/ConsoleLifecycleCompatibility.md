# Console application and controller lifecycle pass

Source pass: 2026-09-05. Targets: Xbox Series X|S and the PlayStation 5 family. This is implemented portable Unreal source, not a console SDK build or certification result.

| Finding | Implementation | Preservation and risk |
| --- | --- | --- |
| No native suspend, system-overlay, controller-loss or reassignment owner | `USovApplicationLifecycleSubsystem` owns Core application delegates across world/controller replacement. `USovApplicationLifecycleComponent`, installed on `ASovPlayerController`, projects that state into the existing controller and CommonUI modal layer. | Extends the current architecture. It neither writes a fresh checkpoint during suspend nor adds a second save system. Platform SDK lifecycle event delivery still needs hardware validation. |
| Reconnect could resume active combat, including held trigger/toggle input | All interruption reasons latch until the player selects **Resume game**. Input routing releases held ability inputs, clears pressed keys, and owns separate move/look input locks. The existing Narrative semantic input router queries a project suppression hook, so late input and reentrant release callbacks cannot re-latch a trigger while interrupted. Reconnect checks the current local player's mapped devices and save owner. | Existing transition and other input locks remain owned by their original callers. Cached device IDs recognize a disconnect even if its event has already lost the old platform-user ID. |
| Independent first-boot/save/system pauses could unpause one another | `AcquireSystemPause` / `ReleaseSystemPause` use named owners and Unreal's `FCanUnpause` hook. First-boot accessibility and save failure now use those APIs. Authored menu pauses before or after a system interruption remain independent. Failed pause requests do not create phantom owners. | Refactors ownership without changing the underlying Unreal world pause. Campaign initialization is allowed to finish its world-timer work before lifecycle acquires a simulation pause; the existing transition input lock remains in place. |
| Suspend could leave storage fenced forever after map travel | The GameInstance subsystem retains application state while controllers are destroyed and recreated. It closes save I/O before cloud cancellation, refreshes account ownership before reopening I/O, and preserves the explicit-resume latch after foreground. | No account identity is transferred implicitly. Existing validated checkpoints and the save subsystem's suspension-aware watchdog accounting are preserved. |
| Haptics or narration could play over system UI | Haptics are immediately cancelled and directly reject any new output while interrupted, including UI/cinematic channels if world pause is refused. Narration cancels with unsuccessful completion, rejects reentrant announcements during suspension, and resumes for the recovery prompt after foreground. | Uses the existing feedback and speech systems. Motor behavior and system narration coexistence require device testing. |
| Failed campaign initialization plus an interruption could trap recovery behind an undismissable modal | The interruption can be cleared in either Idle or Failed transition state. The Failed-state message directs the player back to recovery, and the transition's own input lock remains intact. | Does not mark a failed mission transition successful or fabricate a playable pawn. |
| Unconfirmed display previews could survive an application interruption | The first application-unavailable event invokes existing `RevertUnconfirmedHDRPreview`, with generation checks around callback boundaries. | Reverts only the settings transaction's owned fields; platform/system display values retain their own ownership. |
| Narrative's non-spatial speech used UI audio | `UDialogue::PlayDialogueSound_Implementation` now creates the 2D component without starting it, marks it as gameplay audio, binds line completion and then plays it. | Keeps the existing dialogue graph, sound, volume and spatial branch. The audio-ended line now observes normal world pause instead of intentionally playing through it as UI. Authored sound classes and audio-thread completion at the exact pause boundary still need playback validation. |

The interruption screen is native, uses the existing CommonUI Modal layer, has a real focused controller-accessible button, supports narration/UI text scale, and uses a TV safe zone plus scrolling. Back does not silently dismiss a blocking account or controller interruption. Headless worlds and commandlets never acquire a local-viewport UI prerequisite.

`USovApplicationLifecycleComponent::ActiveTimeSeconds(Player)` excludes the union of overlapping interruptions and the explicit-resume wait. Cinematic loading/watchdog code uses that clock, preserving its separate first-boot wait accounting without subtracting the same interval twice. World-time combat and initialization timers retain Unreal's normal pause behavior.

## Validation

Executed locally:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=undefined -fno-sanitize-recover=all -ISource/ProjectVelkorran/Public Tests/Portable/SovLifecyclePolicyTests.cpp -o /tmp/SovLifecyclePolicyTests
/tmp/SovLifecyclePolicyTests
git diff --check
```

The portable suite passes duplicate background events, overlapping application/controller/account holds, deliberate resume, repeated intervals, invalid/rewound clock rejection and all 80 combinations of active reason subsets with a selected last-cleared reason.

Native regressions added, pending UE 5.7 compilation/execution:

- `ProjectVelkorran.Platform.Lifecycle.SharedPauseOwnership`: three overlapping native pause owners, duplicate acquisition, early/later authored menu pause, refusal cleanup.
- `ProjectVelkorran.Platform.Lifecycle.ControllerReplacementRetainsApplicationHold`: the actual native save subsystem's suspension gate survives controller replacement and clears on foreground; GameInstance application state still requires explicit acknowledgement, and headless worlds do not become UI-blocked.
- `ProjectVelkorran.Platform.Lifecycle.NativeResumeSafeZone`: actual asset-free widget tree and safe-zone root.
- `ProjectVelkorran.Platform.Lifecycle.NarrationSuspension`: cancellation is not successful reading completion, duplicate suspend is idempotent, callback reentry cannot start hidden speech, foreground can announce the resume screen.
- The cinematic lifecycle regression separately exercises an interruption longer than its load timeout, then resumes timeout accounting.

## Required runtime checks

1. On each console family, suspend with attack/guard/aim held, during dialogue VO and timed choices, during first-boot setup, while saving, while reviewing a cloud action on applicable platforms, during an HDR preview, and during map travel. Resume must preserve the last durable checkpoint and display a recoverable prompt without background combat or phantom input.
2. Exercise Background/Deactivate/Overlay events in different orders, including controller destruction between background and foreground. Save I/O remains blocked until the final unavailable reason clears and ownership is refreshed. Reconnect alone never resumes gameplay.
3. Disconnect/reassign the active input device, including a disconnect event whose user is already invalid, several mapped devices, a foreign user's device, and offline account changes. The owning account must remain authoritative.
4. Test the native modal with controller-only input, supported confirm/cancel layouts, maximum text scale, TV safe margins and narration enabled. During a failed mission load, clearing the platform interruption must reveal the existing recovery state instead of claiming the level is ready.
5. Confirm the packaged campaign GameMode accepts standalone pause. A refused pause is reported accurately and input/haptics remain suppressed; source cannot establish a console's OS-level simulation suspension or audio interruption semantics without that platform runtime.

The platform implementation must dispatch/drain Unreal game-thread lifecycle work before suspending execution. Off-thread notifications are marshalled safely, but asynchronous code cannot promise that a platform which freezes its game thread first has executed queued cleanup. Cold resume after process termination uses existing durable save recovery, not an in-memory Quick Resume claim.

Public API references: [Unreal application delegates](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FCoreDelegates), [platform input device mapping](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/ApplicationCore/IPlatformInputDeviceMapper), [native pause ownership hook](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AGameModeBase/SetPause), [audio component UI behavior](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UAudioComponent). Public documentation is rolling; the installed UE 5.7 console source/SDK is the compile authority.
