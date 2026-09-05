# Console display and audio compatibility pass

Source review date: 2026-09-05. Targets: Xbox Series X|S and the PS5 family, including PS5 Pro. This is a source compatibility pass. UE 5.7, restricted platform sources, console SDKs and target hardware were unavailable here.

## Findings and changes

| Existing path | Console risk | Resolution |
|---|---|---|
| Narrative `ApplyMonitorSelection` runs for every non-editor settings apply and dereferences engine, Slate and viewport window. | Consoles have no desktop monitor selection; headless startup/teardown can have no window. | Preserve desktop behavior, but return before monitor enumeration on platforms without windowed mode or with fixed resolution. Check engine, viewport, Slate and window before movement. |
| Sov `ApplySettings` performs an extra HDR output write after Narrative moves the desktop window. | An unnecessary desktop-specific replay can compete with the console platform's output policy. | Keep the engine/Narrative settings apply path and skip the extra Sov replay and confirmed calibration on system-managed output. |
| HDR previews require a physical desktop monitor identity. | A console can support HDR without supplying that desktop identity; the UI previously offered a preview that could never start. | Expose system ownership and separate output-preview/full-calibration capabilities. Console display controls cannot create an in-game preview receipt or write calibration CVars; the native settings menu explains platform ownership. HDR support/state queries remain available through the engine. |
| Pending desktop HDR previews automatically time out after 15 real seconds. | An application handoff can retain an unconfirmed preview until its next ticker call. | Add idempotent `RevertUnconfirmedHDRPreview` for the application suspension owner. It restores the existing transaction and retires its receipt. |
| Authored controller bark class uses `ControllerFallbackToSpeaker`; missing devices/audio still produce authored captions. | Controller speaker availability differs between hardware/controller combinations. | Preserve this existing fallback contract. No controller-only routing, proprietary device IDs or presumed Xbox controller speaker support were introduced. Physical output routing remains an SDK/hardware verification item. |

The display gate uses the public engine capabilities `FPlatformProperties::SupportsWindowedMode()` and `HasFixedResolution()`, rather than restricted platform-name constants. Treating fixed/fullscreen output as system managed is this project's conservative policy, not a claim that every such device has a callable system-calibration API. A future console-specific adapter must supply actual verified capabilities before exposing an in-game replacement.

The general engine `ApplySettings` / `ApplyNonResolutionSettings` paths are preserved. Their platform-aware implementation, audio controls, scalability and any authored quality selector must remain intact. This pass does not claim to validate the proprietary engine branch's HDR startup, VSync, frame pacing or dynamic-resolution behavior, and does not install guessed Series S, Series X or PS5/Pro performance profiles.

## Source ownership

- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativeGameUserSettings.cpp`: desktop monitor guard.
- `Source/ProjectVelkorran/Public/Settings/SovGameUserSettings.h` and `Private/Settings/SovGameUserSettings.cpp`: output ownership, status capabilities, preview rejection and suspension cancellation.
- `Source/ProjectVelkorran/Public/Feedback/SovPlatformOutputTypes.h`: Blueprint-visible capability fields.
- `Source/ProjectVelkorran/Private/Settings/SovDisplayCalibration.cpp`: renderer-calibration ownership gate.
- `Source/ProjectVelkorran/Private/Settings/SovDisplayPolicy.h`: portable capability decisions shared by the native adapter and its policy tests.
- `Source/ProjectVelkorran/Private/Tests/SovPlatformOutputTestFixtures.h` and `SovPlatformOutputRuntimeTests.cpp`: host-independent system-output fixture and native integration regression.
- `Tests/Portable/SovDisplayPolicyTests.cpp`: extended capability matrix.

The console UI pass consumes `bSystemManaged`, `bCanPreviewInGame` and `bCanCalibrateInGame`; callers must not infer permission to change output from `bSupported` alone.

## Validation

Executed locally:

```text
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=undefined -fno-sanitize-recover=all -ISource/ProjectVelkorran/Private Tests/Portable/SovDisplayPolicyTests.cpp -o /tmp/velkorran-console-display-policy
/tmp/velkorran-console-display-policy
PASS: 10096 display calibration and physical viewport overlap policy checks
git diff --check
```

Added native automation `ProjectVelkorran.Campaign.PlatformOutput.SystemDisplayOwnership` verifies observable HDR on a system-managed fixture, rejected HDR/SDR/full-calibration requests without writes or saves, direct production CVar adapter rejection, independent desktop output/full-calibration capability, unknown-monitor rejection and cancellation of a pending preview on suspension. Renderer variables are isolated test registrations; the test does not drive the test machine's display. Native automation was not compiled or executed here.

## Target validation required

1. Compile UHT and both Development/Shipping game targets against the actual UE 5.7 console branches. Run the platform-output automation suite using an automation-capable build.
2. On each target SKU, boot with SDR and HDR displays. Apply unrelated accessibility/audio settings and verify there is no extra Sov output replay, resolution change or desktop window operation. Confirm platform HDR settings and startup policy remain correct through the engine path.
3. Check HDR television disconnect/reconnect, system display changes, suspension/resume and relaunch. Verify a stale preview cannot be confirmed on desktop regression runs and no in-game desktop preview is offered on consoles.
4. Verify headphones, television audio, supported controller speaker, disconnected controller and controller-audio mute. Critical direction remains available through authored captions; no console speaker path is counted as working until heard on hardware.
5. Measure the authored performance/quality modes on Series S, Series X, PS5 and PS5 Pro separately, including UI/caption legibility, HDR UI luminance, resolution scaling and frame pacing. No frame-rate or memory-budget guarantee follows from these source checks.

Public API references: [Epic platform properties](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FGenericPlatformProperties?lang=en-US) and [UE 5.7 HDR display output](https://dev.epicgames.com/documentation/en-us/unreal-engine/high-dynamic-range-display-output-in-unreal-engine?application_version=5.7). These public references do not replace the target SDK or proprietary platform requirements.
