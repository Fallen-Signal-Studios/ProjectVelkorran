# UE 5.7 compatibility and work-PC validation

The September 5, 2026 compatibility pass carries the cross-platform source corrections validated with UE 5.7.4 to the Windows work checkout. It fixes observed compilation and native initialization/serialization defects. A Windows build and the complete authored project still need verification on the work PC.

## Changes included

- Correct UE 5.7 headers, incomplete types, pointer conversions, replication macro arguments, and runtime API access. Export out-of-line plugin methods and the dialogue state's custom deleter for module boundaries.
- Read runtime navigation rules through their public fields; guard Mass editor-only data consistently with the engine declaration; use public CommonUI/Slate accessibility APIs.
- Declare direct CommonUI, MassGameplay, and MassCrowd plugin dependencies. Keep the existing UE 5.7 association, V6 build settings, and Windows TextToSpeech support.
- Declare the owning Mass route-fragment storage contract and exercise reflected copies, compaction, archetype migration, and destruction in native automation.
- Initialize Narrative, Navigator, and Sovereign gameplay-tag tables before native constructors copy them, with idempotent repeated initialization. This repairs empty native ability identities, inputs, and activation requirements.
- Capture complete actor/component SaveGame snapshots so default-valued fields overwrite later live state on restore. Non-SaveGame fields remain excluded. Existing tagged saves remain readable; values omitted from old records cannot be reconstructed by this write-side fix.
- Correct UE 5.7 test compilation and fixture world initialization, preserving assertions and each fixture's initialization settings. Add tag and component-default regressions; retain campaign corruption and opt-in legacy support.

Mac-only Narrative module allow-list additions, Xcode/signing workarounds, generated build products, local logs, and binary assets are excluded from this change.

## Evidence and limitations

The same source changes were tested on Apple Silicon with UE 5.7.4 (CL 51494982), Xcode 16.2, and ZenDyn 1.6.0. The validation checkout additionally enabled Narrative modules on Mac to run these checks; the Windows checkout retains its original plugin platform lists.

| Check | Result |
| --- | --- |
| Development Editor, Mac arm64, full unity compilation and link | Passed |
| Development game, Mac arm64, non-editor compilation and link | Passed; package finalization not qualified |
| Python source/report checks | 30 passed |
| Portable C++ policy suites, warnings-as-errors and UBSan | 39 passed |
| Complete native selection | 237 accounted for: 116 passed (6 with warnings), 121 failed |
| Fresh native tag/CDO and repeated-initialization regression | Passed |
| Actor-default and component-default save regressions | Passed |

Six existing tests changed from failed to passed: Handler and Hound ability contracts, Selene core-loop contract, WeakPoint consequence ownership, identity/transform save restoration, and stable-lookup save restoration. Both new regressions passed. No test that passed in the preceding native run regressed.

The overall native suite is still failing. Of the 121 failures, 46 contain only absent Narrative CDO-asset errors, 11 are crash-marked, and 64 contain other errors or assertions. Remaining fixtures need stable controller identities, valid subsystem/player ownership, registered ability-system components, appropriate play/network lifecycle, and correctly advanced game time. Melee branching, Story callbacks, save-bank faults, and cinematic ownership assertions still need focused investigation. Generic status checkpoint integration with campaign saves remains an outstanding source-level integration gap.

Windows/MSVC, Windows-only TextToSpeech, Shipping, cooking, packaging, multiplayer, rendering, and authored Blueprint/mission behavior were not qualified by the Mac run. Existing GameplayEffect stacking and ability-instancing deprecations also remain for a future engine migration; do not replace runtime calls with editor-only setters.

## Work-PC procedure

Use the existing complete work project, with its matching game and Narrative Content assets, and pull the updated `main` with Unreal Editor closed. This repository intentionally excludes `.uasset`, `.umap`, and Content directories; pulling source does not recreate those assets. Preserve the checked-in Narrative source fork when restoring matching plugin content. The enabled ZenDyn plugin must be available for the UE 5.7 installation on that PC.

With a UE 5.7-compatible Visual Studio C++ toolchain and Windows SDK installed, regenerate Visual Studio project files if needed. In PowerShell from the repository root, first build the Editor with the existing validation script (adjust the engine installation path):

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -BuildOnly
```

Then compile the non-editor Development game target:

```powershell
$VelkorranProject = (Resolve-Path '.\ProjectVelkorran.uproject').Path
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' ProjectVelkorran Win64 Development "-Project=$VelkorranProject" -WaitMutex
```

Once the matching content is present, run a focused native regression before the broader suite:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -TestFilter ProjectVelkorran.Foundation.GameplayTags
```

For the full selection:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7'
```

The script stores a fresh result directory under `Saved/Validation`, checks the native JSON report against source registrations, and fails on missing or failing tests. It may stop at a remaining fixture crash; its Windows runner does not implement the temporary Mac runner's restart/checkpoint loop. Preserve the first failed build log or native report for the next correction rather than treating a zero editor exit code alone as success. After builds and targeted automation, open the real project to validate Blueprint dependencies and an authored mission.
