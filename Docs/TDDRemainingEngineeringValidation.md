# Remaining engineering: validation record

5 September 2026. Applies to the changes from baseline `144544d5d97fa4a3c1720b8420dba822a961bce1` on `codex/tdd-engineering-gaps`. See [the audit and roadmap](TDDRemainingEngineering-2026-09-05.md) and [exact changed-file inventory](TDDRemainingEngineeringFiles.md).

## Executed checks

| Check | Result | Evidence and limits |
| --- | --- | --- |
| `python Scripts/Test-NativePolicies.py` | **30 portable C++ suites passed** | [Captured output](TDDRemainingEngineeringPortableTests.txt). g++ 13.3, C++17, warnings as errors, UndefinedBehaviorSanitizer with recovery disabled. These exercise the production policy headers; they do not compile reflected Unreal classes. |
| `python -B Scripts/Tests/TestUnrealReport.py` | **17 Python tests passed** | [Captured output](TDDRemainingEngineeringReportTests.txt). Tests exercise the actual report validator with intentionally synthetic reports, including omitted tests, duplicate rows, malformed counters and concealed error events. These are not Unreal automation results. |
| Changed-source consistency | **74 C++ files, 26 reflected headers, zero detected errors** | [Captured output](TDDRemainingEngineeringSourceChecks.txt). Repository include resolution, generated-header order, preprocessor balance and module direction checks. This is not UHT, compilation or linking. |
| Native automation inventory | **174 unique selected registrations found** | Source inventory across project and plugins using the production report checker. This is the total expected source registration count, not a count of tests executed here. |
| Patch hygiene | **`git diff --check` passed** | Applied to the final reviewed changes before publication. |

The ballistic suite includes 28,477 assertions; the expanded cinematic policy suite includes 41,878. Existing campaign/combat/save policies also ran through the same portable runner. A separate review traced load ownership, stale AI knowledge, actor replacement, deferred Mass operations, frontend focus callbacks and output rollback. Review found and corrected concrete callback and sensor-cache defects; source review does not substitute for engine execution.

## Authored engine regressions

The implementation includes real UE automation cases for unique load receipts and terminal failure, accepted-world archive failure, exact transform restoration, stable-only identity lookup, 100 storage-fault/restart cycles over serialized bytes, overlapping encounter threat suspensions, real perception and native GAS attack acquisition, pawn-assignment reentry, sensor reactivation without a new stimulus, homing retirement, deferred Mass actor lifetime/order, ballistic launch/physics, Slate focus/semantics, haptic lifecycle, HDR rollback persistence, partitioned cinematic lifecycle and world-postcondition rollback.

**These UE cases have not been compiled or run here.** The 100-iteration storage fixture is neither a packaged death/retry playthrough nor a process-kill certification. Detailed test names and contracts are in the per-system engineering documents linked by the audit.

## Unavailable gates and release status

[Environment preflight](TDDRemainingEngineeringEnvironment.txt) found no UnrealEditor, UnrealEditor-Cmd, UnrealBuildTool, dotnet or PowerShell on PATH. This checkout contains zero `.uasset` and zero `.umap` files; required engine/plugin dependencies are not available. Consequently, UHT/UBT, Blueprint compilation, automation in Unreal, cooking, PIE, packaged campaign playthroughs, device checks and performance captures remain **unrun**. Nothing in this record certifies a playable or shippable campaign.

On the actual UE5.7 workstation, run the existing `Scripts/Validate-Unreal.ps1` with the real project and editor paths as described in [UnrealValidation.md](UnrealValidation.md). Keep its build gate enabled. The runner now requires a source-complete successful engine report and saves `coverage.json`; same-name stale implementations still require a fresh build. Resolve compile/link/runtime failures before marking the changes ready. Then run actual M01/M02 and M12/M13 mission fixtures, restore/streaming failure cases, packaged retries and device/performance passes.

No editor content was authored. Remaining native frontend/platform consumers, campaign Mass rehydration and cinematic inventory transactions are explicitly open in the audit. This delivery closes implemented source gaps and adds validation gates; it does not declare the full TDD complete.
