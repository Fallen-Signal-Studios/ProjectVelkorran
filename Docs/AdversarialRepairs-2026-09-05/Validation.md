# Validation evidence and remaining gates

Executed on the source-only Linux worker on 5 September 2026. The retained logs distinguish real host execution from simulated Unreal stage orchestration. No Unreal Engine executable, licensed console toolchain or runnable mission content was available.

| Check | Result | Evidence and scope |
|---|---|---|
| Portable production-policy C++ | **40 suites compiled and passed** | [FinalPortablePolicyTests.txt](FinalPortablePolicyTests.txt). Strict C++17 warnings and UBSan; includes production save framing, overflow-safe combat math and existing gameplay policies. Does not compile UObject/GAS integration. |
| Host tooling regression discovery | **58 tests passed** | [FinalHostTests.txt](FinalHostTests.txt). Real Python/host process and extracted-C++ execution; Unreal build, automation, cook and route stage responses are explicitly simulated. Includes an all-implemented-stages-success case that still rejects incomplete candidate coverage. |
| Reflected header/source/module contracts | **Passed available source checks** | [SourceVerification.json](SourceVerification.json): 485 generated headers checked, no include-order violation; 138 fixture classes reside in the Editor-only test module. Host contracts reject runtime fixture placement and reproduce the fixed helper compilation/overload cases. |
| Native Unreal registration inventory | **274 registrations authored; 0 executed** | [SourceVerification.json](SourceVerification.json). 224 baseline registrations, 50 additional registrations; existing tests were also extended. An inventory is not behavioral coverage or engine execution. |
| Engine runner preflight | **Correctly blocked** | [UnavailableEnginePreflight.json](UnavailableEnginePreflight.json): exit 2, no build stage executed on this host. |
| Console descriptor preflight | **Blocked, exit 1** | [ConsoleDescriptorPreflight.json](ConsoleDescriptorPreflight.json). Installed engine plugin descriptors/ZenDyn are unavailable and Narrative's console module support is unresolved. No licensed platform identifier was guessed. |
| Patch whitespace, local report links and structured metadata | **Checked before publication** | `git diff --check`, final relative Markdown link resolution and JSON parsing. These checks validate the handoff artifacts, not gameplay. |
| UE 5.7 UHT/UBT; native automation; Blueprint/world/cook/package | **Not run** | Requires the actual engine, plugins and authored game content. |
| Xbox/PlayStation compilation, device execution, performance and certification | **Not run** | Requires licensed platform workers, SDK integrations and target hardware. |

The final source inventory records a digest of 1,250 `.h`, `.cpp` and `.cs` source files. Its digest identifies the source state inspected here; the Git tree of the published repair identifies the entire deliverable. Older domain logs and the transfer-time migration manifest are checkpoint evidence and may show earlier test counts. Final files above take precedence for consolidated counts.

## Candidate gate behavior

`Scripts/Validate-Unreal.ps1` delegates to the staged Python runner. Diagnostic and candidate outcomes are distinct. Clean Editor unity/non-unity, Game Development/Shipping, full native reports, campaign/world contracts, Blueprint/DataValidation, cook/package, immutable package identity, packaged route and retained performance evidence are separate stages.

Current source **cannot report `candidate_passed`** while generalized status/cleanup declarations and exhaustive prohibited-runtime-type validation remain incomplete. The native commandlet rejects `-RequireCompleteCoverage`; the independent runner records missing/partial required categories and returns `candidate_blocked` even if every implemented stage succeeds. World Partition and unloaded streaming coverage also fail explicitly. Diagnostic execution remains available. There is no configuration waiver that silently upgrades incomplete source coverage.

The performance checker validates supplied raw capture structure and reviewed thresholds. Its synthetic regression data does not demonstrate game performance. A content-specific deterministic route driver and profiler normalization remain source/integration work, followed by actual target captures.

## Regression acceptance emphasis

- Run the full native inventory in both build permutations; do not accept only the new test prefix. The new Editor-module boundary needs actual UHT and modular-link validation.
- Exercise real GAS callbacks that cancel/restart actions, mutate resources, restore a life, replace an avatar, or invoke nested damage. Require exact payment, no stale attack continuation, no old-life fatal reward and one committed finisher outcome.
- Use genuine engine-generated legacy save bytes for migration; fuzz nested reflected bodies with valid recomputed checksums, malformed counts and slow/failed provider I/O. The generic storage backend's pre-project allocation boundary remains unresolved here.
- Drive failed travel, destroyed required participants, final-kill/promotion ordering, save-acknowledged cinematics, foreground/suspend/account ABA, cloud revision review, and title-return failure on a packaged route.
- Verify input profile round trips and owner replacement during callbacks, retained subtitle pages, same-node dialogue restart, deferred media, critical caption priority and focus/record disclosure in real UI/audio output.
- Capture cold/warm load, three reloads, combat spikes, active Mass promotion, memory and PSO readiness separately for each intended console target. Preserve raw evidence and reviewed budgets.

No build or native test failure is concealed as a pass: those suites have not been executed. The source is ready for review and the next engine qualification attempt; engine compatibility and runtime correctness remain to be established there.
