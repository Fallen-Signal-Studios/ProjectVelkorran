# Build and validation repairs — 2026-09-05

This records source changes for B1–B6 and V01–V03 from the adversarial audit. The engine, UHT/UBT, Windows/console toolchains, ZenDyn, working game content and target hardware are unavailable in this workspace. **Source/host checks below are not an Unreal build, gameplay result, console compatibility result or certification claim.**

## Dispositions

| Finding | Source disposition | Remaining acceptance |
|---|---|---|
| B1: unity helper redefinitions | Renamed Tarrik/Selene test activation/shield helpers to file-specific names. Exact current shield bodies compile together in a real host C++ translation unit. | Actual forced-unity and non-unity Editor UBT builds. |
| B2: `Alive` overload changes semantics | Protection and Selene predicates have distinct names; both callers retain their original eligibility semantics. The host repro confirms missing-attribute-set Selene behavior is identical with/without the protection function present. | Native combat regressions in both engine permutations. |
| B3: include after generated header | Moved GameplayDebugger include above `NPCActivityComponent.generated.h`; removed duplicate pragma/CoreMinimal block. Full project/plugin generated-header-last scan passes. | UE 5.7 UHT and gameplay-debugger target variants. |
| B4: unsafe DXGI query | Separate platform implementation uses scoped COM ownership, checks factory/enumeration/description/QI/residency HRESULTs, matches RHI vendor/device/name instead of adapter zero, rejects ambiguous identical GPUs, clears failure outputs, separates physical memory from process budget, caches queries for one second and safely handles headless monitor calls. RHI is a private module dependency. | Windows failure injection, hybrid/multi-GPU and device-removal tests with the actual UE 5.7 RHI. Windows API code was not compiled here. |
| B5: reflected fixtures in Shipping | Moved all 118 existing project/plugin test headers and sources into the new `ProjectVelkorranTests` Editor module, preserving new destination regressions. Runtime modules have no remaining Tests directory/reflected fixture headers. Added explicit Editor target/module wiring and narrow Mass/world-validator symbol exports. | Editor UHT/modular link, complete native registration inventory, Game Shipping UHT/generated-code/cook proof that fixture classes are absent. |
| B6: disabled diagnostics overhead | Shipping never creates the diagnostic subsystem. Disabled non-Shipping instances do not tick, retain combat bindings or records; enabling binds immediately. Cached settings avoids global lookup from each tickability query. Added native real-delegate opt-out regression. | Run native toggle/late-event test and inspect Shipping subsystem absence with UE. |
| V01: incomplete qualification gate | Replaced the orchestration body with a tested Python runner behind the existing PowerShell entry point. Explicit clean Editor unity/non-unity, Game Development/Shipping, automation/report, mission/world, Blueprint, DataValidation, cook/package, route assertions, package-integrity and performance stages. Fresh source/content/plugin/package-bound manifests distinguish diagnostic success from candidate qualification. Manual self-hosted workflow refuses wrong revisions/dirty native source. | Windows worker execution, real UBT/commandlet flags/toolchains/content and content-specific packaged-route driver. No workflow was dispatched. |
| V02: native authoring coverage | Extended existing commandlet to read placed-world contracts and existing pure validators; added raw Echo/weapon/payload defaults, retired Selene effect override and Technique Blueprint dispatch. Existing cinematic sequence validator is reused through a pure wrapper. Missing/duplicate identities, participants, role budgets, entry/handoff/cinematic/transit contracts and renamed vendor classes produce stable errors. | **Partial coverage remains:** complete World Partition/streaming actor enumeration is not implemented; these maps fail explicitly. Runtime-spawned actors, generalized status/cleanup declarations, exhaustive prohibited-type classification and actual sequence binding/skip acceptance remain engine/source follow-ups. This is not full validator coverage. Every run records missing/partial required categories, and current candidate qualification is blocked even on conventional maps. |
| V03: no performance evidence gate | Added strict raw-capture checker with reviewed workload minima/tolerances, frame continuity, total/combat 99% targets, sustained >50 ms combat spikes, baseline plus three reloads, nonzero promotion coverage, PSO readiness and latency checks. Root/feature owners instrument central runtime scopes separately. | **Partial implementation remains:** deterministic content-specific route driving, profiler normalization and real cold-cache/hardware capture are still required. No frame, memory or latency measurement is claimed here. |

## Important implementation contracts

- Preserve Narrative's public GPU query shape. `FGPUInfo.BudgetVRAM` is additive; `TotalVRAM` now means physical dedicated video memory, while `CurrentVRAM` is local-segment process usage. Existing UI should check the boolean result and label the new budget distinctly. Multi-adapter ambiguity returns unavailable instead of guessing; a future licensed RHI/LUID provider may resolve identical adapters.
- Candidate warnings have no implicit waiver. Build/commandlet warnings fail; automation's native report reconciles `AddExpectedError` negative-test logs, while raw fatal/crash output still fails. This avoids rejecting successful adversarial tests merely because their intentional stimulus is an error log.
- A fresh automation report must contain the current source inventory. Raw process exit zero, a focused filter, `-SkipBuild`, `-BuildOnly`, omitted world coverage and an absent route are never candidate evidence.
- The package manifest is rechecked after driving. Original binary/cooked inputs cannot change; newly generated Saved logs/traces/config/save/cache outputs are distinguished from new executable payloads.
- The raw capture checker rejects absent promotion/combat workloads and idle-frame dilution. All numeric tolerances not fixed by the TDD must come from a reviewed policy. A custom driver's assertions still require human review against its retained raw trace; the checker cannot authenticate gameplay by itself.
- The commandlet never calls BeginPlay, initializes combat, or invents placed actors to make validation pass. It invokes existing read-only authoring contracts and logs explicit incomplete-category failures.

## Files and areas

- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal`: `NarrativeArsenal.Build.cs`, `Private/ArsenalStatics.cpp`, new `Private/NarrativeGPUInfo.cpp`, `Public/ArsenalStatics.h`, `Public/AI/Activities/NPCActivityComponent.h`, and narrow Mass fragment export.
- `Source/ProjectVelkorran`: protection/Selene helper names; diagnostics header/implementation; Echo authored-default/accessor API; cinematic pure sequence wrapper; Mass route fragment export; existing validation commandlet and new `Private/Validation/SovCampaignWorldValidation.*`.
- Native regressions: `SovBuildValidationRuntimeTests.cpp` / `SovBuildValidationTestFixtures.h` plus preserved renamed payload helper tests. Three new registrations exercise placed-world negative fixtures, actual diagnostic delegate removal and raw Echo defaults.
- `Scripts/Validate-Unreal.ps1`, new `Validate-Unreal.py` / `Check-PerformanceReport.py`, strengthened `Check-UnrealReport.py`, new `Tests/TestValidationPipeline.py` / `TestBuildContracts.py`, and extended `Tests/TestUnrealReport.py`.
- `.github/workflows/unreal-validation.yml` and updated `Docs/UnrealValidation.md` document/configure the real-worker contract without claiming a worker exists.

## Executed checks and engine gates

Executed in this workspace before final module relocation:

- 24 host orchestration/performance tests passed. Engine process I/O is explicitly synthetic; real host process exit, literal argv and timeout handling are also exercised. Negative cases cover each process stage, empty reports, skipped/stale/focused candidate attempts, source mutation, wrong identity, package replacement, injected DLL, expected automation errors, absent promotion, idle dilution, leaks, delayed promotion, pending PSOs and malformed samples.
- 19 report-checker tests passed, including warnings hidden under successful totals.
- Three source/build-contract tests passed, including two real host C++ compile/run permutations using the exact current production helper bodies with minimal declarations. These prove the narrow C++ helper issue, not Unreal header/API compatibility.
- Project/plugin generated-header-last scan and `git diff --check` passed at the checked working state.

Final complete host/module results and relocation inventory follow. UE 5.7 UHT/UBT, native automation, Blueprint/DataValidation/cook, real packaged route, Windows DXGI behavior, Xbox/PlayStation builds and all target measurements remain **not run**.

Primary API/workflow references: [Epic subsystem creation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USubsystem/ShouldCreateSubsystem), [tickable world subsystem](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UTickableWorldSubsystem), [RHI globals](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/RHI/FRHIGlobals), [RHI GPU identity](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/RHI/FRHIGlobals/FGpuInfo), [COM QueryInterface](https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-queryinterface%28refiid_void%29), [DXGI video-memory query](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo). Public Epic pages are rolling documentation; the first actual UE 5.7 build must verify exact headers and fields.


## Final relocation and host verification

- `Source/ProjectVelkorranTests/ProjectVelkorranTests.Build.cs` and `Private/ProjectVelkorranTests.cpp` define one Editor module. `ProjectVelkorran.uproject` uses `Type: Editor` and `PostEngineInit`; only `ProjectVelkorranEditor.Target.cs` explicitly requests it. Runtime targets retain no test dependency.
- Moved **118** `.h`/`.cpp` files, including untracked repair fixtures, and preserved the new account-preference/dialogue-start files already written at the destination. The transfer-time byte digests and paths are in `TestModuleMigration.json`. The sole transfer-time content adjustment is the lifecycle test GameMode URL changing to `/Script/ProjectVelkorranTests.SovLifecycleTestGameMode`; the real campaign GameMode URL stays in the runtime module.
- At verification, the Editor test directory held **122** files / **137** reflected fixture classes and the source inventory contained **272** native simple-test registrations. These are source inventory counts, not executed Unreal tests. No production source includes a `Tests/...` header.
- Narrow cross-module link review added `PROJECTVELKORRAN_API` to `FSovCampaignMassRouteFragment` and the world-validator function, and `NARRATIVEARSENAL_API` to `FNarrativePedFragment`, because Mass test templates call generated `StaticStruct` across DLLs. Existing receipt classes/structs and production public APIs are already exported. The first modular Editor link remains the authority for any additional export requirement.
- **57 host tests passed** via `python3 -m unittest discover -s Scripts/Tests -p 'Test*.py'`, including the now-enabled Editor-only fixture contract and current-source C++ helper compile/run checks.
- **40 portable policy suites compiled and passed** with `-Wall -Wextra -Werror -pedantic -fsanitize=undefined -fno-sanitize-recover=all` via `python3 Scripts/Test-NativePolicies.py`.
- Manual self-hosted workflow YAML parsed successfully; no dispatch occurred. The actual stage-runner CLI was invoked with a build-only request in this workspace and correctly returned **2**, recording `failed` because Windows/UE execution is unavailable. No build stage ran.
- `git diff --check` passed after relocation. Source include ordering and fixture/module placement checks passed. Exact native engine compile/runtime assertions remain unexecuted.


## Final required-category gate correction

The final review found that conventional maps could previously reach `candidate_passed` despite the documented V02 source gaps. This has been corrected:

- Every manifest now includes `required_validator_coverage.complete: false` and separate required entries for `status_cleanup_declarations` (`missing`, `not_evaluated`) and `prohibited_runtime_class_closure` (`partial`, `not_evaluated`). These are source-owned facts, not configurable waivers.
- Candidate mode passes `-RequireCompleteCoverage` to the existing commandlet. It reports `VALIDATION.COVERAGE_INCOMPLETE` and returns nonzero with `VALIDATION.REQUIRED_CATEGORIES` until those native validators and their negative fixtures exist.
- The runner independently appends a blocked required-coverage stage and returns **2 / `candidate_blocked`** even if every implemented process, report, package, route and performance stage returns success. Thus a conventional map or misleading zero commandlet exit cannot qualify the incomplete engineering layer.
- Diagnostic mode may still execute all implemented stages and return `diagnostic_passed`, with the required category stage explicitly `incomplete`. No current-source run can legitimately report `candidate_passed`.
- Added/updated host regressions drive the full simulated stage pipeline to success and verify candidate rejection, and verify that full diagnostic execution retains both missing categories. Engine I/O in these tests remains synthetic; no Unreal execution is implied.

Final rerun after this correction: **58 host tests passed** via full host discovery; `RequiredCoverageHostTests.txt` retains the output. Native test inventory counts above are a migration-time snapshot and may grow as other owners finish regressions.
