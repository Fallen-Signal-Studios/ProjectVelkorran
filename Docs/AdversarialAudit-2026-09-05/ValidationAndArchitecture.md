# Validation and architecture audit

Audit target: tree `049edf431aa27a57f6d1ef1c00add8b10fe90de0`, integrated by PR #30. Findings below are source-observed engineering gaps. They do not assert that an external studio CI service does not exist, or substitute for an Unreal execution result.

## V01. The checked-in gate cannot qualify a playable or Shipping build

**Priority:** P1 validation gap. **TDD:** 15.14, 15.18, 16.11, 18.3, 18.8.

**Exists:** `Scripts/Validate-Unreal.ps1:161` builds `ProjectVelkorranEditor Win64 Development`; its automation invocation uses `-NullRHI`. It creates a fresh report directory, checks process exit and report errors, and invokes `Check-UnrealReport.py` against registrations in both project and plugin source. Preserve those useful protections. The checker correctly rejects missing tests and misleading successful aggregates with error events. Warnings are reported and accepted.

**Gap and failure case:** the runner never builds a Game Test/Shipping target, invokes the campaign commandlet, compiles all Blueprints, runs map validation, cooks a representative mission, launches a packaged route, or evaluates performance. A revision can pass this runner while a required encounter actor has an invalid GUID, a Game-only compilation path fails, or the campaign cannot cook. The tracked repository has no CI definition wiring these separate commands into one candidate gate. The source inventory covers simple-test registrations, not execution of every state transition or behavioral coverage of same-name tests. `-SkipBuild` explicitly permits stale implementation bodies and is appropriate only for diagnosis.

**Improvement:** extend this runner or its existing orchestration with explicit stages and separate outcomes: clean Editor compilation, clean Game compilation, complete engine automation, campaign-manifest validation, Blueprint/map/cook checks, then a packaged mission route. Record source tree, engine build, plugin versions, target, compiler, command line, and output paths in one machine-readable run manifest. Require a reviewed owner/expiry for accepted candidate warnings. Keep focused filters and `-SkipBuild` available but label their results diagnostic, never candidate qualification. Gauntlet can orchestrate actual game sessions; it does not supply the game's tests automatically. [Epic Gauntlet overview](https://dev.epicgames.com/documentation/unreal-engine/gauntlet-automation-framework-overview-in-unreal-engine).

**Dependencies/risk/order:** required engine/plugins/content and licensed platform workers. Medium tooling risk: accidental success after a skipped stage, stale binaries, or killed child processes must fail the candidate outcome. Introduce the manifest and stage contract in the first compilation slice; add mission execution after the progression fixes.

**Validation/definition of done:** deliberately fail each stage and verify the aggregate rejects the candidate. Test empty and partial reports, old reports with identical test names, interrupted cook, missing SDK, absent content and accepted-warning expiry. A clean run must retain actual target binaries, reports and package route results. The current 39 portable suites and 27 host tests remain useful lower-level gates, not a replacement.

## V02. Campaign validation does not cover several native contracts that already exist

**Priority:** P1 validation gap. **TDD:** 16.11, 18.3; encounter, save, cinematic, and status contracts.

**Exists:** `Source/ProjectVelkorran/Private/Validation/SovValidateCampaignCommandlet.cpp` validates mission definitions, required predecessor consequences, dependency packages, evidence, melee definitions, corruption profiles, narrative cues, and dialogue graphs. It detects forbidden path segments and supports explicit dynamic assets. This is substantive validation and should be extended.

**Gap and failure case:** `ValidateNativeAsset:66` dispatches only a small set of asset validators; the dependency filter at `:104` skips known unrelated asset classes. It does not inspect placed encounter components/role budgets, checkpoint actor GUID uniqueness, cinematic participant/skip contracts, or generalized ability cleanup/cost and status cleanse/UI declarations. At `:264` the commandlet explicitly reports that map actors and Blueprint compilation need separate gates. A readable map package can therefore pass while its required placed actors violate source-enforced admission rules and fail only when the player reaches them. `-AdditionalAssets` does not cure missing validator dispatch. Forbidden-reference checking is a path heuristic, not proof that prohibited runtime classes are absent under renamed content directories.

**Improvement:** keep one validation authority and add typed dispatch to the existing native validators. Add a controlled world/actor validation pass for stable GUIDs, authored roles/waves, critical actor residency, sequence configuration and transit anchors. Add ability/status declarations where no inspectable contract exists, then validate those declarations and their runtime cleanup tests. Check prohibited class/module dependencies as well as path names. Report every omitted category explicitly; do not return a candidate-ready aggregate when a required category was skipped.

**Dependencies/risk/order:** editor world loading, actual mission manifests and engine validation APIs; medium risk from executing runtime side effects while validating. Use editor validation contexts and pure configuration checks, not BeginPlay-driven gameplay. Follow the first build fixes and precede the packaged M01/M02 gate.

**Validation/definition of done:** negative fixture assets/worlds for duplicate GUIDs, over-budget role composition, missing or incompatible participant, invalid sequence fallback, broken ability/status declaration, and prohibited class in a harmlessly named folder. Each must produce a stable diagnostic ID and nonzero candidate result. Valid fixtures and intentionally optional assets must still pass. The complete shipping manifest must exercise every required category.

## V03. Local combat diagnostics are not a performance qualification harness

**Priority:** P2 instrumentation/verification gap. **TDD:** 6.15, 15.12, 18.8–18.9.

**Exists:** `SovDiagnosticsSubsystem.cpp` records bounded opt-in combat/campaign events, supports local export, and disables recording in Shipping. Its records identify actions and amounts, not frame-time percentiles, asset stalls, memory residency, or mission-route completion. `Tick:40` still refreshes bindings while recording is disabled; this is an avoidable overhead candidate, not a measured frame-budget violation. Project runtime source contains no project-specific CPU profiling scopes beyond the subsystem tick stat declaration.

**Gap:** no checked-in deterministic capture controller, frame percentile report gate, memory-trend comparison, shader/PSO readiness gate, or bounded reload soak driver appears in the available source/tooling. Consequently the TDD's 99% frame target, no sustained combat spike above 50 ms, and three-reload memory requirement cannot be accepted from these logs. Full-world scans and dynamic component creation deserve measurement, but source inspection alone cannot assign their milliseconds.

**Improvement:** instrument the existing damage owner, encounter tick/promotion, save capture/commit, transition initialization, narrative presentation, and async asset residency. Add deterministic capture routes and fault points to the existing test architecture; emit raw Unreal Insights/CSV data and a small report checker for the agreed TDD gates. Use separate Series S/X and PS5/Pro results. Leave gameplay-critical collision, weak points, roles and attacker timing invariant under scalability.

**Dependencies/risk/order:** runnable content, engine profiling support and target hardware. Low/medium instrumentation risk, high risk of reporting an unrepresentative workload. Define counters and capture contracts during source repair; collect acceptance evidence after packaged routes work.

**Validation/definition of done:** deliberately inject a long combat frame, growing allocation and delayed promotion and verify each is visible and fails its corresponding gate. Validate 30/60/120 fps gameplay behavior separately from frame-budget measurements. Three consecutive mission reloads, long-session input/suspend/travel stress, and cold-cache captures must produce reproducible reports. No invented performance budgets or numbers.

## V04. Combat bark loading can synchronously stall its critical presentation path

**Priority:** P2 source-observed latency risk; no measured stall claimed. **TDD:** 12.7, 14.8, 15.10.

**Exists:** `Source/ProjectVelkorran/Private/Narrative/SovNarrativeCueComponent.cpp:152` selects a bark variant and calls `Variant.Sound.LoadSynchronous()` and `ControllerAudioClass.LoadSynchronous()` at `:154` before installing the bark and presenting its caption. The scheduler supports combat-priority cues. Missing audio falls back to a caption, which is the correct fallback to retain.

**Gap:** there is no guaranteed asynchronous prefetch/residency contract on this path. A cold cue may block the game thread before its caption is delivered. A successfully loaded audio asset proves neither bounded load latency nor timely warning presentation. The Mass report records the related representation residency gap separately; these should share asset-management policy, not an independent loader.

**Improvement:** preload mission/encounter cue bundles through the existing Asset Manager and hold bounded handles. Allow a late voice asset to complete asynchronously only while its cue/context generation is current; show the caption immediately when the cue is accepted. Define whether late audio is suppressed or played, and never replay a stale warning. Preserve controller-speaker fallback and caption priority.

**Dependencies/risk/order:** caption arbitration fixes, active mission bundles, memory budget and native cue receipts. Medium change risk around stale completion and dialogue suspension. Implement after the correctness fixes and before performance qualification. Epic describes soft references and streamable loading for this purpose; soft references alone are not residency ownership. [Epic asynchronous asset loading](https://dev.epicgames.com/documentation/unreal-engine/asynchronous-asset-loading-in-unreal-engine).

**Validation/definition of done:** delay/fail the actual asset request during active combat, then replace cue/mission/controller. The current caption remains timely; obsolete audio never plays; handles release on every end path. Measure cold/warm cue latency and memory on target builds.

## Architectural improvements that should guide the repairs

The dominant defect is inconsistency at ownership boundaries, not absence of features. A callback can cancel, replace, destroy or reenter its producer. Several systems have robust receipts/epochs while nearby systems use an unprotected boolean and continue after the callback. Adopt an explicit transaction contract in each existing owner: validate and reserve, commit authoritative state, publish a committed outcome, then continue only if the same operation still owns the context. Resource payment, damage, pickups, phase completion and storage require different concrete implementations of that contract. A universal rollback manager would obscure their semantics.

Retain Narrative's ASC, inventory, input router, save serializer and Mass representation bridge. Extract shared admission/completion helpers only where semantics are truly identical. Do not start a wholesale module split before first compilation. The TDD's proposed module layout is a useful dependency direction, but rearranging 176,477 lines into more modules cannot prove correctness. First make plugin/project dependencies explicit, isolate file-local helpers, and centralize duplicated invariants. Then move the largest stable boundaries behind narrow interfaces incrementally.

Maintain a vendor-baseline manifest for Narrative and a reviewable patch inventory. Locally modifying a vendor plugin is reasonable here, but vendor upgrades and console support must replay these changes without silently replacing combat/save behavior. Require the same adverse callback tests against every reconciled plugin revision.

Test quality should be judged by adversarial behavior and the real owner under test. Avoid a test that only calls the policy predicate used by the implementation and repeats its expected result. Keep those unit checks, then add production delegate/overlap/input/serialization cases that invoke cancellation, a second mutation, or delayed completion at each dangerous boundary. Test retained state, exact resource deltas, emitted events, and absence of stale side effects, not just a returned boolean.
