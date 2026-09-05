# Unreal validation

`Scripts/Validate-Unreal.ps1` invokes the checked-in Python stage runner. Every run owns a fresh directory and records the source/config/content hash, external Narrative/ZenDyn implementation hashes, engine `Build.version`, plugin versions, exact commands, target/configuration, durations, process exits, logs and final outcome in `Saved/ValidationRuns/<run-id>/summary.json`. Build logs retain UBT's compiler/SDK selection. Source, plugin implementation and route-policy changes during a run invalidate the outcome.

**Full candidate qualification is currently blocked by two remaining source categories:** generalized status/ability cleanup declarations and exhaustive prohibited runtime type validation. A nonpartitioned map does not remove those gaps. Every run records them in `required_validator_coverage`; there is no config waiver. Diagnostic mode can still execute all implemented stages.

**No Unreal compilation, UHT run, Windows process execution, gameplay route or target performance capture was performed in the source repair environment.** Host tests exercise the orchestration and evidence checkers using explicitly synthetic engine I/O; they are not Unreal test reports.

## Prerequisites and commands

Use Windows PowerShell 5.1+ and Python 3.10+. The wrapper locates UE's bundled Python or accepts `-PythonExecutable`. Python is now required for build-only runs too. Restore UE 5.7, its supported Visual Studio/Windows SDK toolchain, Narrative content, game content and the enabled ZenDyn plugin. Required plugin descriptor ambiguity is rejected. The script does not change vendor platform filters or install console SDKs.

```powershell
# Compile Editor unity/non-unity and Game Development/Shipping. Diagnostic only.
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -BuildOnly

# Build those targets and run the complete native automation inventory.
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7'

# Explicitly diagnostic reuse of already-built binaries for a focused investigation.
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' `
    -SkipBuild -TestFilter 'ProjectVelkorran.Campaign.AxiomNullPulse'

# Full candidate pipeline with the actual project's authored mission/route manifest.
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' `
    -Mode candidate -ConfigPath 'D:\VelkorranValidation\campaign.json'
```

`-OutputDirectory`, `-AutomationTimeoutSeconds`, `-BuildTimeoutSeconds` and `-RouteTimeoutSeconds` control retained artifacts and bounded process durations. Batch arguments reject shell expansion characters/embedded quotes; arguments containing spaces and shell metacharacters are quoted individually. Windows timeout cleanup uses `taskkill /T /F` to retire the child tree; Windows behavior still requires its first real worker run.

## Qualification contract

| Mode | Successful outcome | Requirements |
|---|---|---|
| Diagnostic | `diagnostic_passed` | Requested stages passed. Missing campaign/route configuration leaves explicit `skipped` stages. `-BuildOnly`, `-SkipBuild` and focused filters are allowed. |
| Candidate | `candidate_blocked` / failure until required source coverage is implemented; `candidate_passed` is unavailable in the current source | Complete required validator categories **and** clean Editor unity/non-unity and Game Development/Shipping builds, complete automation, campaign/world validation, Blueprint compilation, asset DataValidation, Shipping cook/package, actual packaged route and accepted performance evidence all pass. No skipped build, build-only mode, focused filter, warning or source mutation is accepted. |
| Failed/interrupted | `failed` or interrupted manifest retaining `running` | Never qualifies. Process zero without its required fresh report does not pass automation or the packaged route. |

Candidate mode currently targets **Win64 only**. Its successful result says nothing about console compilation, certification or hardware budgets. Separate licensed Xbox Series S/X and PS5/Pro workers must use their actual SDK/platform adapters, packages and captures. No guessed platform token is embedded here.

`Check-UnrealReport.py` requires every current native simple-test registration in project/plugin source, unique rows, coherent totals, successful states and zero error events. Warnings hidden beneath successful aggregate counters remain visible and fail candidate mode. Build and commandlet logs also reject engine/compiler severity errors and warnings. Automation uses the native report to reconcile `AddExpectedError` stimuli; raw expected negative-test errors do not defeat that report, while fatal/crash output still fails the process stage. There is currently no warning waiver facility; correct warnings before qualification.

## Campaign and route configuration

The JSON passed with `-ConfigPath` must contain these actual working-project values:

| Key | Contract |
|---|---|
| `missions` | Nonempty list of real `/Game/...Asset.Asset` mission definitions, including M01_Mantle and M02_OneDegree. |
| `maps` | Nonempty list of real `/Game/...` map packages to cook. |
| `additional_assets` | Optional list of actual dynamically referenced asset paths to add to dependency validation. |
| `route_command` | Nonempty argv array for the studio's actual packaged-route driver. It must launch the freshly archived Shipping package with a real RHI and test gameplay. Supported substitutions are `{run_id}`, `{source_sha256}`, `{package_sha256}`, `{output}`, `{package}`, `{project}`. |
| `performance_policy` | Explicit reviewed capture policy described below. |

No route names, Blueprint input sequence, campaign assets or performance captures are fabricated in this repository. Supplying a valid manifest and implementing the content-specific route driver are still required before candidate qualification can execute. The runner cannot independently prove that a custom driver tells the truth; review and execute the driver against its retained raw capture and gameplay assertions.

The package file manifest is hashed before driving and rechecked afterward. Original inputs must remain byte-identical; only newly created Saved log/trace/config/save/cache outputs are tolerated. A replaced executable or injected DLL fails qualification.

The route driver writes `{output}/route.json` with this run's `run_id`, `source_sha256`, `package_sha256`, `status: "passed"`, `null_rhi: false`, and `completed_missions` containing both opening mission IDs. It must assert `checkpoint_restored`, `fatal_recovery_completed`, `protagonist_handoff_completed` and `save_reload_completed` as actual booleans. An assertion is emitted only after observing the corresponding native gameplay result. Even if all these implemented stages succeed, the runner independently returns exit 2 / `candidate_blocked` while required validator categories are incomplete; a misleading zero commandlet exit cannot bypass that restriction. Omitted, false, stale or wrong-package assertions fail the gate.

## Placed-world validation and remaining coverage

The existing `SovValidateCampaign` commandlet now supports `-ValidateWorlds`; `-ShippingValidation` requires it. The runner requests both. Candidate mode additionally requests `-RequireCompleteCoverage`, which currently returns nonzero with `VALIDATION.REQUIRED_CATEGORIES` because the two required source categories are incomplete. The commandlet always identifies these gaps with `VALIDATION.COVERAGE_INCOMPLETE`. Diagnostic mode omits the complete-coverage flag and may pass its implemented checks without claiming full qualification. It reads mission maps without starting gameplay and validates:

- Native save GUID validity/uniqueness; entry PlayerStart identity; encounter IDs, native role/wave budgets, participant uniqueness/ownership/residency.
- Native evidence, command-link and weak-point configuration; Blueprint Echo raw cost/identity/weapon/payload contracts, retired Selene effect overrides and Technique grant policy; transit identity; actual vendor component/class inheritance even beneath renamed folders.
- Handoff anchor/mission/beat/destination contracts; cinematic configuration, mission beat, existing presentation-only sequence constraints, exact transit references, unique participant tags and compatible character types and required placed cinematic coverage.

Diagnostics use stable `WORLD.*`/`ASSET.*` codes and include coverage counts. An unloaded streaming level or World Partition map explicitly fails the current category (`WORLD.STREAMING_COVERAGE` / `WORLD.PARTITION_COVERAGE`): loading only persistent actors is incomplete evidence. The next engine-backed extension must enumerate/load all editor actor descriptors and streaming levels, run the same checks, then verify residency and data-layer combinations. This source change does not claim that extension is complete. Runtime-spawned actors, generalized ability/status cleanup, full prohibited-class coverage, translation coverage and actual cinematic binding/skip behavior remain separate native/Blueprint/route acceptance requirements.

## Performance evidence

`Check-PerformanceReport.py` accepts `{output}/performance.json` plus a hashed, normalized raw CSV in the same fresh run. Metadata identifies run/source/package, platform, physical device, rendering mode, driver, capture tool, target FPS, real-RHI execution and a cold-cache capture. `samples_csv` is a relative path and `samples_sha256` binds the exact bytes. The driver must preserve the originating Unreal Insights/CSV trace as additional evidence.

Each CSV row has `frame`, `frame_ms`, `memory_bytes`, `reload_index`, `combat`, `pso_pending`, `promotion_latency_ms`, `promotion_events`. Promotion latency samples require corresponding completed events; a route with no promotion cannot qualify that metric. Frames are consecutive; values must be finite and valid. `combat` is `0` or `1`; memory is resident bytes, not a budget. `reload_index` covers baseline `0` and three consecutive reloads `1..3` with consistent settled sampling windows. Keep load peaks in the raw trace; document the driver's settling criteria so windows cannot be selected to conceal growth.

The reviewed policy requires `reviewed_by`, `target_fps` (30/60/120), `minimum_frames`, `minimum_combat_frames`, `minimum_frames_per_reload`, `minimum_promotion_events`, `sustained_spike_frames`, `maximum_reload_growth_bytes`, and `maximum_promotion_latency_ms`. No missing numeric threshold is guessed. The checker rejects fewer than 99% of total or combat frames meeting `1000/target_fps`, sustained combat frames above 50 ms, growth above the agreed three-reload tolerance, outstanding combat PSO work, delayed promotion, missing/duplicate frames and short/unrepresentative captures. Passing one device/mode does not qualify another.

Actual deterministic route driving, normalization from the selected engine profiler, cold-cache capture and licensed target-hardware collection are still required. The checker tests deliberately inject long frames, leaks, delayed promotion, missing PSO readiness and stale evidence to verify rejection; these are checker tests only.

## Native test module

All native tests and reflected fixtures live in `Source/ProjectVelkorranTests/Private/Tests`. The `ProjectVelkorranTests` module is `Editor` only and loads at `PostEngineInit`; Game/Shipping targets do not request or reflect these fixture classes. Test names remain unchanged. Production modules do not depend on the test module. The first Editor modular link must validate symbol exports; Game Shipping UHT/cook must verify fixture absence.

## Host regression commands

The checked-in `unreal-validation.yml` workflow is manual-dispatch only. It requires a studio-managed Windows worker labeled `velkorran-ue57` and repository variables `SOV_UE57_ENGINE_ROOT`, `SOV_UE57_PROJECT_PATH`, and (for candidate mode) `SOV_VALIDATION_CONFIG`. Provision the hydrated checkout at the exact dispatched commit first; the job rejects mismatched revisions or dirty/untracked native source. It preserves licensed content instead of cleaning or replacing that checkout. Reports stay on the worker under `Saved/ValidationRuns`. No worker was connected and no workflow was dispatched during source repair.

```text
python Scripts/Test-NativePolicies.py
python -m unittest discover -s Scripts/Tests -p "Test*.py"
```

Portable policies compile real production-used numeric/policy code with host warnings/sanitizers. Build-contract tests compile the exact current helper bodies together using minimal declarations to verify the previously reproduced C++ name collision/overload defects are removed. None of these host checks establish UHT, GAS, serialization, physics, cooked content, Blueprint or engine behavior.

API/workflow references: [Epic automation execution](https://dev.epicgames.com/documentation/unreal-engine/run-automation-tests-in-unreal-engine), [Blueprint compilation commandlet](https://dev.epicgames.com/documentation/unreal-engine/API/Editor/UnrealEd/UCompileAllBlueprintsCommandlet), [Data Validation commandlet](https://dev.epicgames.com/documentation/unreal-engine/data-validation-in-unreal-engine), [UAT build/cook/package](https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine), [World Partition builders](https://dev.epicgames.com/documentation/unreal-engine/world-partition-builder-commandlet-reference).
