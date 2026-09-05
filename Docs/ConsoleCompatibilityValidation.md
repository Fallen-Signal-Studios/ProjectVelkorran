# Console pass validation, 5 September 2026

This distinguishes executed host checks from authored Unreal regressions and console hardware gates.

| Check | Result | Scope |
| --- | --- | --- |
| `python3 -B Scripts/Test-NativePolicies.py` | **39 portable suites passed** | C++17, `-Wall -Wextra -Werror -pedantic`, UBSan and no sanitizer recovery. New scrolling/interruption policies and extended output/storage capability cases are included. [Full log](ConsolePortableTests.txt). This does not compile UE classes. |
| `python3 -B -m unittest discover -s Scripts/Tests -p 'Test*.py' -v` | **27 tests passed** | 17 existing Unreal-report-checker cases plus 10 console descriptor/preprocessor cases. [Full log](ConsoleHostChecks.txt). The Mass matrix preprocesses the actual changed source with UE includes omitted, across four debug/visual-log flag combinations; it is not a UBT build. |
| `python3 -B Scripts/Check-ConsoleBuild.py` | **Blocked, exit 1** | [Retained preflight](ConsoleBuildPreflight.json) reports four Narrative runtime module filters, missing ZenDyn and unavailable engine dependencies. This is the actual unresolved build state, not a successful console test. The tool says `compiled=false` and `certified=false`. |
| Production report-checker source inventory | **224 unique selected registrations**, 12 added | Registration discovery succeeds with no duplicate names. All 224 native tests remain uncompiled/unrun in this workspace. Several are editor fixtures; use the editor suite for complete registration coverage and separate supported device harnesses for console testing. |
| Changed reflected headers | **19 passed generated-include inspection** | Required generated headers are present and included last. This textual check is not UHT. |
| Project and Narrative descriptors | **Valid JSON; unique plugin references** | Removed one pre-existing duplicate `HairStrands` entry; no platform module allow-list was widened. |
| `git diff --check` | **Passed** | Patch whitespace only. |

## Added Unreal registrations

| Area | Cases |
| --- | --- |
| Storage/provider | `ConsoleOwnerRoutingAndSignout`, `SuspendHoldsStorageAndWatchdogs`, `RevokeBeforeCloudCancellationCallbacks` |
| Display | `SystemDisplayOwnership` |
| Persistent interruption/pause | `SharedPauseOwnership`, `ControllerReplacementRetainsApplicationHold`, `NativeResumeSafeZone`, `NarrationSuspension` |
| Cinematic preparation | `ConsoleInterruptionPreservesPreparation` |
| Controller/UI | `BackAndFirstBoot`, `NavigationAndSafeArea`, `SystemManagedHDR` |

Existing travel coverage was extended without adding a registration: it now rejects invalid users, clears stale read outputs, revokes ownership from actual `PrepareForSave` and save-subclass serialization callbacks, verifies unchanged prior slot bytes after rejected writes, and refuses records when ownership changes during deserialization. The shared pause regression also exercises late semantic input suppression through the existing Narrative input router.

The controller-replacement regression uses the actual native save suspension gate through an injected dependency, destroys/replaces the controller and checks that foreground reopens that gate while explicit resume remains pending. It does not simulate a real OS process freeze. Display fixtures suppress physical device writes and use isolated state. Console account/provider fixtures do not contact a real console account or storage service.

## Review fixes included

- Moved global application state to a GameInstance lifetime so a destroyed controller cannot strand save suspension or lose the foreground event.
- Balanced overlapping named pause owners; kept previous/later authored pauses; removed phantom ownership after a refused pause request.
- Allowed an interruption prompt to release back to an existing Failed recovery state while retaining transition input ownership.
- Closed storage before cloud-cancellation callbacks and retained that gate through foreground account revalidation.
- Required a private native authorization record before Blueprint account selection can access a console user compartment; removed the public native authorization-bool setter.
- Reacquired deferred OSS interfaces against the eventual world, while preserving an in-flight/draining request's exact provider and delegates.
- Checked travel ownership after capture, after serialization and immediately before the platform write; checked it again after destination read/staging. Travel also refuses disk access while application storage is suspended.
- Excluded interrupted time from cinematic preparation without double-counting first-boot waits; retained genuine timeout cleanup.
- Directly suppressed semantic input/haptic admission during interruption; marked non-spatial Narrative dialogue as gameplay audio before starting playback.

## Not executed, and known limits

No UE 5.7/UHT/UBT compilation, link, Unreal automation execution, Blueprint compile, console cook/stage/sign/deploy, full mission playthrough, physical output, live platform storage, process-kill test or performance measurement occurred. Native regression source and descriptor checks are not substitutes for those results. The engine build remains the planned Monday gate.

Narrative console support and ZenDyn are hard dependencies. Private platform account/entitlement/storage integration, offline identity mapping, system accessibility/narration, platform cloud conflicts, output calibration and device profiles require the licensed implementation. Existing platform save APIs are retained; that does not prove the correct SDK backend is configured.

Legacy Narrative display-name/user-0 multiplayer save entry points remain for compatibility. Console campaign UI must use the project save authority and the explicit campaign travel path; do not route new console menus into those legacy APIs.

An interruption reaching an on-disk travel read before storage reopens is refused; the existing checkpoint remains the recovery authority. Verify that the packaged project's travel-error/recovery presentation handles this path, along with late account loss after staging. This source pass does not promise seamless Quick Resume across every map-load boundary.

The non-spatial dialogue change relies on normal Unreal gameplay-audio pause behavior. Authored sound-class overrides and an audio-ended callback already queued at the pause boundary still require playback validation. A platform that freezes its game thread before draining marshalled lifecycle events cannot be proven safe from source alone.

Run `Scripts/Validate-Unreal.ps1` with the complete UE 5.7 project, without `-SkipBuild`, and retain the report-checker result for all 224 registrations. Then use the actual licensed platform tools and suitable on-device harnesses for the per-console acceptance matrix in [ConsoleCompatibilityPass-2026-09-05.md](ConsoleCompatibilityPass-2026-09-05.md). Editor-only fixtures are not silently counted as on-device results.
