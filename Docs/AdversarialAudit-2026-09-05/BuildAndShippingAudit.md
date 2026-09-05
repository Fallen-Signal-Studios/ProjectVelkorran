# Build, native integration, and Shipping audit

Baseline: local commit `6225c68d50fbea605d7c7dfc8d61d5569805e28f`. This report is a read-only source audit. Runtime source was not changed. UE 5.7, UHT, UBT, a Windows graphics runtime, console SDKs, and authored content were unavailable. Host C++ reproductions below establish specific language-level failures, not successful Unreal compilation.

## B1. Identical anonymous-namespace test helpers collide in unity builds

**Priority P1. Confirmed when the affected files occupy one unity translation unit. Actual UBT file grouping is unmeasured.**

Evidence:

- `Source/ProjectVelkorran/Private/Tests/SovTarrikPayloadRuntimeTests.cpp:76-91`: anonymous-namespace `Activate<T>(FAutomationTestBase&, ASovAxiomRuntimeTestCharacter*)` and `Shield(ASovAxiomRuntimeTestCharacter*)`.
- `Source/ProjectVelkorran/Private/Tests/SovSelenePayloadRuntimeTests.cpp:75-89`: the same function signatures in another anonymous namespace.
- `Source/ProjectVelkorran/ProjectVelkorran.Build.cs:9` and both target constructors: no module or target rule isolates these files from unity compilation.

An anonymous namespace provides internal linkage to a translation unit, not isolation between files included into that translation unit. Both test files can pass separately and then fail after UBT changes their grouping. The two unchanged `Shield` function bodies were extracted with minimal type declarations into `UnityCollisionRepro.cpp`. `c++ -std=c++17 -fsyntax-only` exits 1 with a redefinition diagnostic. This intentionally failing evidence is recorded in `BuildPermutationEvidence.txt`.

**Improve:** preserve the tests and production behavior. Give each test suite a unique named implementation namespace and qualify helper calls, or consolidate genuinely shared helpers into one explicit fixture utility. Audit function templates as well as ordinary functions. Do not rely on current grouping or a developer's adaptive working set to avoid collisions.

**Dependencies/order:** first compile-readiness slice, before trusting any native test run. Follow with unity, non-unity, and no-PCH builds of the real module; use a unity stress configuration as an additional diagnostic.

**Risk:** low gameplay risk; medium test integration risk if moving helpers changes lookup or fixture access. **Acceptance:** both helpers compile together, all existing registrations remain present, and test results agree in unity/non-unity builds.

Epic documents UBT's merging of source files and adaptive exclusion behavior in [Build Configuration](https://dev.epicgames.com/documentation/unreal-engine/build-configuration-for-unreal-engine).

## B2. Unity composition can silently change Selene source eligibility

**Priority P1 for build determinism; observed gameplay reachability of the differing state is unverified. Confirmed C++ overload behavior.**

Evidence:

- `Source/ProjectVelkorran/Private/Combat/SovProtectionInterceptReceipt.cpp:20-48`: anonymous-namespace `Alive(UAbilitySystemComponent*)` requires a `UNarrativeAttributeSetBase`.
- `Source/ProjectVelkorran/Private/Combat/SovSelenePayload.cpp:19-27`: anonymous-namespace `Alive(const UAbilitySystemComponent*)` permits an ASC without that attribute set.
- `Source/ProjectVelkorran/Private/Combat/SovSelenePayload.cpp:82-91`: `ValidSource` passes `Context.SourceASC.Get()`, which is a mutable `UAbilitySystemComponent*` from the context's weak pointer (`Public/Combat/SovSelenePayload.h:15`).
- `SovSelenePayload.cpp:111-117`: `EligibleTarget` explicitly uses a pointer to const, so its call does not exhibit the same overload change.

When the protection file is included before the Selene file in one translation unit, the mutable overload is visible and is a better match for `ValidSource`. No compiler error is required. With a non-null, alive ASC lacking the Narrative attribute set, the source helper returns true in isolation and false in that unity composition. `UnityOverloadRepro.cpp` retains both actual helper bodies with stubbed types. The recorded executions demonstrate `1` versus `0`; the const target remains `1` in both runs. This does not prove normal authored Selene players lack their required attribute set. It proves that build composition changes the function's contract.

**Improve:** preserve each system's intended policy and use distinct names such as `IsLivingProtectionParticipant` and `IsLivingSelenePayloadParticipant`, or unique implementation namespaces with qualified calls. Explicitly decide whether source readiness must require the Narrative attribute set and test that requirement in the production API. Shared names with different contracts should not silently merge.

**Dependencies/order:** same initial build-determinism slice as B1. No asset work required.

**Risk:** low if changes only namespace/name lookup; medium if intentionally changing the eligibility contract. **Acceptance:** identical `ValidSource` results under isolated and combined compilation for null, dead, fatal, missing-attribute-set, and ordinary source states; actual runtime tests in both UBT configurations.

## B3. A public reflected header violates Unreal's generated-include ordering contract

**Priority P1 pre-compilation repair. Confirmed source-layout violation; UE5.7 UHT rejection was not executed.**

`Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/Activities/NPCActivityComponent.h:13` includes `NPCActivityComponent.generated.h`. Lines 20-23 then conditionally include `CoreMinimal.h` and `GameplayDebuggerCategory.h` under `WITH_GAMEPLAY_DEBUGGER`. The header also contains a second `#pragma once` at line 18 and a gameplay debugger class declaration at 28-38 before the component UCLASS. A scan of 470 headers containing generated includes found this one ordering violation.

Epic requires the generated header to be the last include in [Objects in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/objects-in-unreal-engine). The conditional changes which build permutations encounter the extra include; it does not make the layout a sound public-header contract. The debugger is explicitly configured in `NarrativeArsenal.Build.cs` through `SetupGameplayDebuggerSupport(Target, true)`.

**Improve:** at minimum move the conditional debugger include above the generated include and remove duplicate boilerplate. Prefer moving the debugger category into its own private header, leaving a forward declaration where component declarations need it. Preserve the debugger implementation.

**Dependencies/order:** first compile-readiness slice alongside B1/B2. Check editor Development and debugger-disabled Game/Shipping permutations.

**Risk:** low, with a possible public/private dependency adjustment when extracting the debugger. **Acceptance:** generated include is lexically last in all headers; actual UE5.7 UHT and compiler pass with the debugger enabled and disabled. This audit does not assert an exact UHT error without running that tool.

## B4. GPU diagnostics can leak COM interfaces and dereference failed queries

**Priority P1 for the exposed failure path. Confirmed unsafe source behavior; invocation by authored UI is unverified.**

`Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/ArsenalStatics.cpp:142-160` implements the BlueprintPure `GetGPUInfo` API declared in `Public/ArsenalStatics.h:65-67`:

1. Factory and adapter pointers are uninitialized and HRESULTs are ignored.
2. `EnumAdapters` returns an `IDXGIAdapter` interface, but its output pointer is reinterpreted as `IDXGIAdapter3**` without obtaining that interface with `QueryInterface`.
3. The factory and adapter references are never released. Repeated successful queries leak references.
4. `QueryVideoMemoryInfo` failure leaves the local structure uninitialized, but its fields are copied and the API returns true.
5. Adapter zero may be the desktop display adapter rather than Unreal's render adapter. The OS process memory budget is exposed as `TotalVRAM`, which is not physical installed VRAM.

Microsoft specifies the returned interface and reference ownership in [EnumAdapters](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-enumadapters), interface conversion in [QueryInterface](https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-queryinterface%28refiid_void%29), and fallible process-budget reporting in [QueryVideoMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo).

**Improve:** preserve the Blueprint API or add a compatible status result. Prefer Unreal's render-adapter metrics; if DXGI remains, use scoped COM references, acquire the correct IID, check every HRESULT, zero the output on all failures, identify the rendering adapter, and distinguish budget from physical capacity. Poll at a bounded rate if used in a widget binding. The adjacent `GetMonitorNames` implementation at `ArsenalStatics.cpp:80-94` should also return an empty result when Slate is unavailable rather than unconditionally calling `FSlateApplication::Get()`.

**Dependencies/order:** platform diagnostics hardening after compile barriers. Windows runtime or injected provider failures are required to validate COM handling. Asset inspection must identify whether any current widget calls the APIs; no native `GetGPUInfo` caller was found.

**Risk:** low gameplay risk; medium compatibility risk for Blueprint labels that assume `TotalVRAM` means physical capacity. **Acceptance:** failed factory creation, no adapter, unsupported interface and failed query return false with initialized output; repeated successful queries retain no extra COM references; multi-adapter values describe the active renderer; headless monitor query returns safely.

## B5. Reflected test fixtures are part of the Shipping runtime modules

**Priority P2. Confirmed source/module inclusion; shipped binary size and cook impact are unmeasured.**

There are 113 UCLASS declarations under `Source/ProjectVelkorran/Private/Tests` and 3 under NarrativeArsenal's private Tests directory. They reside in the runtime modules, with fixture declarations and fixture implementation files outside automation guards. Guarding the registration bodies in `*RuntimeTests.cpp` does not exclude those reflected classes or constructors.

Concrete examples:

- `Private/Tests/SovCampaignMassRoundTripFixtures.h:7-18` and `.cpp:8-23` define an unconditional reflected NPC fixture; its constructor performs an `FObjectFinder` load of `/Engine/BasicShapes/Cube.Cube` and creates a mesh component.
- `Private/Tests/SovHandoffRuntimeTestFixtures.cpp` contains unconditional fake readiness setup and the static serialization callback for `USovTravelOwnerFenceTestSave`.
- `Private/Tests/SovSaveRuntimeTestFixtures.h:11-55` and `SovSettingsTestFixtures.h:10-76` include probe actors, save classes and settings overrides in production reflection.

UCLASS types have class default objects initialized by their constructors, as described in [Epic's object documentation](https://dev.epicgames.com/documentation/unreal-engine/objects-in-unreal-engine). This is unnecessary runtime type, constructor, and dependency surface. The amount of cooked content pulled in must be measured rather than inferred from file counts.

**Improve:** preserve regression coverage but move reflected fixtures and native automation registrations into an explicit test/developer module included by development test targets and excluded from Shipping. Keep only narrow, justified test seams in runtime classes. Do not simply wrap UCLASS declarations in arbitrary preprocessor guards; use module boundaries that UHT understands. Fixture classes crossing module boundaries need correct exports or self-contained fixtures. Update test discovery to include the new module.

**Dependencies/order:** immediately after build determinism, before release packaging. Requires module rules, target/descriptor configuration, and native test registration validation.

**Risk:** medium to high test migration risk because fixtures inherit and access production types and one another. **Acceptance:** all native tests remain discoverable in the test target; Shipping has no fixture UClass registrations, fake save classes, or fixture-only asset dependency. Compare actual cook manifests and binary reflection inventory.

## B6. Disabled diagnostics still bind and tick in Shipping

**Priority P3 efficiency issue, not a demonstrated frame-budget failure or privacy leak.**

`Private/Diagnostics/SovDiagnosticsSubsystem.cpp:31-44` always returns false for recording in Shipping but still calls `RefreshBindings()` every 0.25 seconds. `RefreshBindings` at 96-124 attaches damage, ability, health, Echo and campaign delegates without checking recording state. Initialization also attaches the settings listener. Record storage and export correctly remain disabled, but a nominally disabled subsystem still participates in the combat event graph.

**Improve:** preserve opt-in development diagnostics. Prevent creation in Shipping or disable its tick/bindings when recording is disabled; enable bindings only while required. Maintain explicit cleanup when toggling recording off or replacing the player.

**Dependencies/order:** release overhead cleanup after correctness fixes. No new telemetry system is needed.

**Risk:** low, mostly missed development records if enable/disable rebinding is wrong. **Acceptance:** no diagnostics tick/delegates in Shipping, no data recording while disabled, immediate correct rebinding when development diagnostics are enabled, no duplicate callbacks after repeated toggles. Measure before assigning a millisecond saving.

## Coverage and unresolved build risks

Inspected game and Narrative module/target/plugin descriptors, the public reflected include layout, fixture declarations and implementation guarding, test helper linkage, selected native gameplay/animation/Mass integration, platform adapter declarations and callback structure, and Windows diagnostic APIs. This is not a claim that every Unreal API expression was compiled or every plugin function was reviewed.

The following remain gates or risks rather than newly proven defects:

- No UE5.7 UHT/UBT run exists in this environment. New or deprecated API signatures, engine default build settings, platform extension headers and transitive include exposure remain unverified against installed 5.7 headers.
- Narrative's four required runtime module platform allowlists exclude the target consoles; ZenDyn is enabled but absent from the checkout. These already documented blockers are not solved by the source audit.
- Narrative publicly exposes types whose module dependencies are private, including DeveloperSettings and some Mass/UI implementation headers. The actual transitive graph may currently supply them. Require non-unity/no-PCH and modular builds to expose missing direct dependencies; this report does not claim a particular missing-header error without that graph.
- All native automation results remain pending in UE. The host reproductions isolate C++ namespace/overload rules and cannot validate engine registration, reflection, garbage collection, RHI, audio, or assets.
- Tests executing without authored content do not establish a successful cook, packaged startup, Level 1/2 traversal, hardware suspend/resume, or frame/memory budgets. The main audit owns the production validation pipeline findings and roadmap.

The proper compile-readiness claim after source repairs is that the known blockers and reproducible language defects are addressed, with a concrete engine validation gate. It cannot be an assertion of an airtight compiled layer before the engine has processed it.
