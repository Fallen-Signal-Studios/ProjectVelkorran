# Unreal validation

`Scripts/Validate-Unreal.ps1` builds the real `ProjectVelkorranEditor` target for Win64 Development, then runs all `ProjectVelkorran` automation in Unreal Engine 5.7, including Campaign and World suites. A narrower `-TestFilter` remains available for diagnosis. It does not substitute static checks for compilation or manufacture an automation result.

## Prerequisites

- Windows PowerShell 5.1 or PowerShell 7 on Windows.
- Python 3 for source-to-report coverage validation. The runner checks Unreal's bundled Windows Python, then `python.exe` on PATH; `-PythonExecutable` accepts an explicit executable path. Build-only validation does not require Python.
- Unreal Engine 5.7, including `Engine/Build/Build.version`, `Build.bat`, and `UnrealEditor-Cmd.exe`.
- The Visual Studio C++ toolchain and Windows SDK required by the installed UE 5.7 build. UnrealBuildTool checks their compatibility.
- This project's source, configuration, and NarrativePro plugin. Install the UE 5.7 version of **ZenDyn**, which is enabled in the project descriptor but absent from the source repository. Other enabled engine plugins must also be available.
- For automation, restore the working project's `Content` and NarrativePro's `Content`. Both are absent from the audited source repository. The default map and game mode reference NarrativePro assets. The runner checks that each content directory contains Unreal assets; Unreal remains responsible for resolving specific referenced assets and dependencies.

The source audit environment has no Unreal installation, UnrealBuildTool, Windows toolchain, PowerShell, gameplay assets, or ZenDyn. **The Windows runner and Unreal compilation/automation could not be executed in that environment.** A source review or host-language test run is not evidence of a successful Unreal build or playable encounter.

## Commands

From the project root, build and run the campaign suite:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7'
```

Compile without loading content or starting the editor:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -BuildOnly
```

After compiling this exact source revision, rerun the focused Axiom tests:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' `
    -SkipBuild -TestFilter 'ProjectVelkorran.Campaign.AxiomNullPulse'
```

For a different working-copy path or output location:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' `
    -ProjectPath 'F:\ProjectVelkorran\ProjectVelkorran.uproject' `
    -OutputDirectory 'F:\Validation Results' -AutomationTimeoutSeconds 1800
```

`-BuildOnly` and `-SkipBuild` are mutually exclusive. `-SkipBuild` cannot verify that binaries contain the latest source; use it only immediately after a successful build of the revision being tested. Paths containing spaces are supported. The batch invocation rejects percent signs and line breaks to prevent command-shell path expansion.

## Results and failure handling

Every invocation that reaches the build phase uses a fresh timestamped folder under `Saved/Validation`, or the supplied output directory. It records build/editor stdout and stderr, `UnrealEditor.log`, `AutomationReport/index.json`, and `summary.json`. Preflight failures print the missing prerequisite; failures after the folder is created also write `validation-error.txt`.

- Native build/editor failures preserve the process exit code.
- Automation timeout returns `124` and stops the launched editor process.
- Missing prerequisites, a missing/malformed report, zero matching tests, failures, skipped tests, or incomplete tests return `2`.
- Success requires every selected native simple-test registration in project/plugin source to appear as successful. `Scripts/Check-UnrealReport.py` rejects missing/duplicate rows, inconsistent totals, hidden error events and failed/unrun/in-progress tests, and writes `coverage.json`. Success with warnings is reported; review the warnings. A same-name stale implementation still requires rebuilding.
- Build-only success explicitly states that automation was not run.

`-NullRHI` is appropriate for these native gameplay tests. It does not validate rendered materials, animation presentation, input/UI wiring, replication across multiple processes, or Level 1/2 campaign playthroughs. Those remain separate Unreal editor and PIE acceptance checks.

The runner uses Epic's documented [automation command-line interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine?application_version=5.7) and [JSON report fields](https://dev.epicgames.com/documentation/en-us/unreal-engine/review-test-results-in-unreal-engine?application_version=5.7). No CI runner or remote Unreal build service is configured by this script.

## Expanded native engineering pass

`python Scripts/Test-NativePolicies.py` compiles and runs all production-used portable C++ policy suites with warnings as errors and undefined-behavior sanitization. These cover numeric/policy behavior only. They are not UHT, GAS, serialization, physics, AI, Blueprint, or Unreal build results.

The campaign automation namespace now includes the additional bot, combat-routing, Guard, native payload, resource, encounter, story, handoff, Technique, travel and other newly authored suites. Run the complete namespace after a clean Development Editor build; a subset cannot certify the complete engineering pass. Then run `CompileAllBlueprints`, the explicit mission preflight in [CampaignHandoff.md](CampaignHandoff.md), and the actual M01/M02 playthrough and repeated checkpoint tests with content.
