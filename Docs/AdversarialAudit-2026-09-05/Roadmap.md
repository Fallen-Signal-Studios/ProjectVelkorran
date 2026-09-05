# Corrective vertical slices

This roadmap repairs the audited revision through existing systems. It is implementation guidance, not a record of completed repairs. Finding IDs refer to the [audit](README.md) and its linked domain reports. Runtime code was not changed by the audit.

`P/` = `Source/ProjectVelkorran`; `N/` = `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal`. File paths below are exact relative paths after expanding those prefixes. Test names identify existing suites to extend, not new tests claimed as implemented.

## Slice 1. Deterministic compilation and a clean runtime build

**Playable improvement:** the same source can produce repeatable development/game builds; changing a unity grouping cannot change gameplay eligibility or break test compilation.

**Findings:** B1–B5, first stage of V01. **Depends on:** the full UE5.7 project and required plugins for execution. Source naming/header repairs can be prepared before that installation; console vendor/SDK blockers remain explicit.

**Systems/files:** `P/Private/Tests/SovTarrikPayloadRuntimeTests.cpp`, `SovSelenePayloadRuntimeTests.cpp`; `P/Private/Combat/SovProtectionInterceptReceipt.cpp`, `SovSelenePayload.cpp`; `N/Public/AI/Activities/NPCActivityComponent.h`; `N/Private/ArsenalStatics.cpp`, `N/Public/ArsenalStatics.h`; `P/ProjectVelkorran.Build.cs`, both `Source/ProjectVelkorran*.Target.cs`, `ProjectVelkorran.uproject`, `N/NarrativeArsenal.Build.cs`; reflected fixture files in both existing Private/Tests directories; `Scripts/Validate-Unreal.ps1`, `Scripts/Check-UnrealReport.py`.

**Implementation:** qualify file-local helpers with distinct implementation namespaces; repair generated-include ordering and debugger dependencies; use initialized, checked, correctly owned platform queries. Move reflected fixtures/registrations into an explicit development test module with correct exports and target inclusion. Preserve every existing test and runtime test seam that has a justified use. Add source tree/engine/plugin/target/build-option identity to validation output. Do not silence collisions by relying on one developer's adaptive working set.

**Tests and failure cases:** compile affected helpers together; run identical eligibility inputs with both compilation layouts. Run actual UHT plus Editor Development and Game Test/Shipping builds, unity and non-unity, debugger enabled/disabled; use a diagnostic no-PCH build for include hygiene. Exercise missing Windows adapter/interface/query and headless monitor calls. Verify test module is present in test targets and absent from Shipping reflection/cook dependency inventories. Verify omitted modules cannot produce a passing registration report.

**Risk:** low for namespace/include repair, medium/high for test-module migration. The first successful engine compile may reveal additional API/link errors that source review could not see; fix those in this slice and record them.

**Definition of done:** known B1–B3 hazards removed; checked platform query outputs; all required native registrations retained in the test target; no fixture-only classes/assets in Shipping; real clean-build logs identify the exact source/engine/plugins/options. Console builds are either executed successfully on licensed workers or explicitly blocked, never inferred from Win64.

## Slice 2. Combat transactions and action cancellation that preserve state

**Playable improvement:** stagger is punishable, one shot costs one loaded round, one pickup pays once, and canceled attacks/defenses cannot retain gameplay state.

**Findings:** ED-01/02/03/06/07, PC-01/02/03. **Depends on:** Slice 1 for reliable native execution. Design semantics of committed released payloads remain explicit.

**Systems/files:** `N/Private/GAS/NarrativeAttributeSetBase.cpp`, `NarrativeCombatAbility.cpp` and associated headers; `P/Private/Abilities/SovGameplayAbility_Echo.cpp`, `SovGameplayAbility_SeleneDeflection.cpp`, `SovGameplayAbility_ReformationDrone.cpp`; `P/Private/Components/SovDeflectionComponent.cpp`, `SovShieldComponent.cpp`, `SovPoiseComponent.cpp`; `P/Private/Melee/SovGameplayAbility_Melee.cpp`, `SovAbilityTask_MeleeSweep.cpp`; `N/Private/Items/WeaponItem.cpp`, `InventoryComponent.cpp`; `P/Private/Combat/Pickups/SovCombatSustainPickup.cpp`, `SovAmmoCombatSustainPickup.cpp`, `SovEchoCombatSustainPickup.cpp` and corresponding headers.

**Implementation:** preserve damage math and receipts while serializing same-target nested mutation and terminal outcomes with a bounded recursion policy. Make activation epochs live in the shared owner before payment; respect valid/deferred GAS ending. Apply Guard/Hound-style ownership discipline to Deflection and drone release. Retire Shield/Poise work on death/avatar loss and deliberately restart after restore. Cancel active melee on actual interruption. Validate ammo amounts and stage existing inventory/clip entitlement before callbacks. Reserve a pickup before mutation, preserving partial-pack semantics.

**Tests:** extend `SovCombatRoutingRuntimeTests.cpp`, `SovGuardRuntimeTests.cpp`, `SovMeleeRuntimeTests.cpp`, hero/defense runtime suites; add actual pickup/inventory callback integration. Inject nested damage/healing at each resource/break/status boundary, lethal recursion, start/cost/end cancellation and reactivation, tag changes inside release hooks, retired ASC owner, and overlap reentry. Cover zero/exact/full resources, denied inventory removal, negative/overflow inputs, 30/60/120 fps and a hitch crossing an active window.

**Likely failures:** a global lock accidentally drops valid death explosions; arbitrary receipt eviction allows duplicate rewards; stale teardown removes a new activation's tags; partial pickups are consumed without granting; normal revive never restarts recovery; scope-deferred cancellation publishes an obsolete payload.

**Risk:** high, because Echo, weak points, drops, death, AI and feedback observe shared results. Land central transaction repair first, then dependent lifecycle/resource changes, and rerun affected integration suites rather than retuning balance to hide a bad delta.

**Definition of done:** accepted contributions match final resources and receipts; death resolves once; broken/canceled old actions cannot dispatch; replacement ownership survives old callbacks; clip and reserve conserve ammunition; collected quantity never exceeds a pack; no retired timer/tag/task/delegate remains. An actual input-to-draw-to-paid-hit-to-sustain-to-reload fixture passes for each protagonist.

## Slice 3. Recoverable checkpoints, encounters and cross-map travel

**Playable improvement:** M01/M02 progression survives deferred AI work, low storage, missing participants, unavailable users and failed travel without a forced restart or silent progress loss.

**Findings:** SP-01/02/03/05, C01–C04. **Depends on:** Slices 1–2, verified native identity mapping and current save serializer. The raw frame change needs an explicit compatibility/migration decision before external saves ship.

**Systems/files:** `P/Private/Save/SovSaveSubsystem.cpp`, `SovCampaignSaveGame.cpp` and headers; `P/Private/Platform/SovPlatformServicesSubsystem.cpp`, `SovOnlinePlatformServicesAdapter.cpp`; `P/Private/Framework/SovPlayerController.cpp`, `SovCampaignGameMode.cpp`, `SovApplicationLifecycleComponent.cpp`, `SovApplicationLifecycleSubsystem.cpp`; `P/Private/UI/SovApplicationInterruptionMenu.cpp`; `P/Private/Campaign/SovEncounterDirector.cpp`, `SovEncounterDirectorMass.cpp`, `SovEncounterCoordinationComponent.cpp`; `P/Private/Cinematics/SovCampaignCinematicComponent.cpp`; existing Narrative save capture/read APIs.

**Implementation:** add an operation-wide storage owner/generation token and revalidate after fallible/callback-producing work, immediately before mutation and before success publication. Add bounded raw save framing/class admission ahead of UObject decode with required migration/golden fixtures. Centralize boundary acknowledgement use. Reevaluate encounter completion after promotion/mutation stabilization; classify sanctioned actor replacement versus unexpected loss and recover without awarding false kills. Keep one persistent travel transaction with origin checkpoint, destination and ownership identity; handle delayed failures and bounded recovery. Add deliberate return-to-title recovery for unavailable owners. Keep source gameplay held during interruption while allowing narrowly defined initialization orchestration.

**Tests:** extend `SovSaveRuntimeTests.cpp`, `SovPlatformServicesRuntimeTests.cpp`, `SovLifecycleRuntimeTests.cpp`, `SovNarrativeTravelTests.cpp`, `SovCampaignMassRuntimeTests.cpp` and cinematic/encounter runtime suites. Fake storage revokes/suspends at reads, writes and readback; real serialization fixtures exercise allowed class boundaries. Fuzz framing/counts and retain corrupt bytes. Kill final required A during optional B promotion; destroy required actor without death; fail accepted travel after source controller teardown; fail origin recovery; acknowledge a failed CanonGate write and retry the exact scene. Test all transition states with a real viewport, input and damage hazard.

**Likely failures:** wrong-account recovery, false save-success after revocation, reentrant completion rewards twice, legitimate Mass teardown counted as loss, old travel failure restoring over a newer session, newly framed saves treating genuine old saves as corrupt, pausing initialization forever, title transition retaining old pause leases.

**Risk:** high. Use existing SaveSubsystem, encounter attempt IDs and named pause owners. Do not weaken account selection to escape the modal; do not count destroyed actors as defeated.

**Definition of done:** exactly one success/failure/recovery for each owned operation; every failure is actionable; durable banks survive rejected work; no unauthorized subsequent write or success; all required actors reconcile; the exact acknowledged boundary can proceed without claiming a fresh save; old travel failures cannot affect new sessions. A packaged M01→M02 route passes with injected failure and legal checkpoint recovery at each boundary.

## Slice 4. Trustworthy weapon payloads, boss outcomes and companion commands

**Playable improvement:** weapon collision matches presentation, boss finishers deliver their committed phase result, and focus-target companions move into a useful attack position.

**Findings:** PC-04/05/07/08, ED-04, C05. **Depends on:** Slice 2's action/transaction contract and Slice 3's durable recovery.

**Systems/files:** `P/Private/Abilities/SovGameplayAbility_TarrikEcho.cpp`, `SovGameplayAbility_TarrikCinderlineRequiem.cpp`, Selene payload ability files and `Public/Abilities/SovGameplayAbility_SeleneEcho.h`; `P/Private/Combat/SovSelenePayload.cpp`, `SovNativeDamageReceipt.cpp`; `P/Private/Abilities/SovGameplayAbility_Finisher.cpp`, `P/Private/Combat/SovFinisherTargetComponent.cpp`; `N/Private/Items/RangedWeaponItem.cpp`; `P/Private/Companions/SovCompanionCommands.cpp` and existing Narrative activity/attack selection.

**Implementation:** reuse the existing muzzle-bridge/forward-convergence policy in Judgement; snapshot release ownership for all direct/radial continuation; derive success from the existing transaction receipt. Make editable GE overrides either validated and effective or explicitly deprecated with asset migration diagnostics. Correct named recoil preset routing without silently breaking compensating content. Separate finisher outcome delivery from the remaining ability lease. Pursue a selected companion target using existing attack ranges, navigation and a leader leash.

**Tests/failure cases:** muzzle through thin cover, eye aim behind socket, close shoulder camera, reactivation on first radial hit, hit immediately healed, allowed/unsafe GE subclass, hip/aim deterministic recoil, aligned boss phase canceled during damage then restored, target phase replacement, companion out of range/partial path/moving target/command replacement. Compare real trace channel/material and reticle behavior against ordinary primary fire; do not assume the current content makes all queries equivalent.

**Risk:** medium/high, including designer data migration and committed-versus-canceled outcome semantics.

**Definition of done:** no backward/through-cover discharge; remaining release never borrows another activation; hit feedback belongs to its packet; configuration is truthful; committed finisher outcome is delivered/recoverable exactly once; focus command reaches a legal attack position or gives an actionable failure while obeying contribution limits.

## Slice 5. Readable narrative, recoverable choices and account-owned preferences

**Playable improvement:** a player using captions, large text or a different profile receives complete critical information and can always recover a legal choice presentation.

**Findings:** UI-01–07; coordinates with SP-02. **Depends on:** Slice 3's account/session ownership and pause contract. Core UI-03 pause protection can be developed alongside Slice 3.

**Systems/files:** `N/Private/Tales/Dialogue.cpp` and header; `P/Private/Narrative/SovNarrativeCueComponent.cpp`; `P/Private/UI/SovFrontendComponent.cpp`, `SovAccessibilityPresentation.cpp`, `SovAccessibleRecordMenu.cpp`; `P/Private/UI/Dialogue/SovDialoguePresentationComponent.cpp`, `SovDialogueChoiceWidget.cpp`; `P/Private/Settings/SovGameUserSettings.cpp` and header; existing platform/save facade and native CommonUI layers.

**Implementation:** use dialogue/line/cue ownership to pause and restore subtitle pages through bark interruption; separate scene history retirement from readable active text. Defer, deduplicate and replay current completion after pause. Add semantic caption priority and bounded coalescing. Re-present current valid replies after real widget retirement without stealing modal focus. Select account-owned accessibility/input/audio preferences, setup/completion and consent separately from physical display calibration; fence writes and migrate deliberately. Expose permitted unheard records through the existing Records menu with knowledge-safe eligibility.

**Tests/failure cases:** real Tales→arbiter→frontend→Slate chain, queued audio/sequence completion at pause, final short VO with several unread pages, caption burst50ms apart, removed/recreated HUD, narration in flight, two accounts in one process, corrupt settings, signout while persisting, old scene callbacks after protagonist switch, records viewed by a different protagonist. Test large localized text and remapped gamepad-only navigation in an actual viewport.

**Risk:** high around privacy/knowledge and graph completion; medium presentation changes. Keep one speech arbiter and one settings facade.

**Definition of done:** no graph progression while held; all critical speech/captions remain readable; valid current choices recover exactly once; stale widgets cannot commit; account preferences/consent/first-boot do not leak; permitted unread information is accessible without introducing knowledge spoilers.

## Slice 6. Bounded histories, discoverable cloud versions and asset residency

**Playable improvement:** long sessions and changing encounter representations remain stable; feedback stays relevant in crowded scenes; optional cloud does not hide a later save because of a bad clock.

**Findings:** PC-06, ED-05, SP-04, C06, UI-08, V04, B6; the domain reports' archive/provider and synchronous-save risks. **Depends on:** stable transactions and account ownership from Slices 2–3.

**Systems/files:** `P/Private/Components/SovTarrikEchoGenerationComponent.cpp`, `SovSeleneEchoGenerationComponent.cpp`, `SovWeakPointComponent.cpp` and headers; `P/Private/Platform/SovOnlinePlatformServicesAdapter.cpp`, existing cloud review UI; `P/Private/Campaign/SovEncounterDirectorMass.cpp`, `SovCampaignMassProxy.cpp`; `P/Private/Narrative/SovNarrativeCueComponent.cpp`; `P/Private/UI/SovAccessibilityPresentation.cpp`; `P/Private/Diagnostics/SovDiagnosticsSubsystem.cpp`; existing Narrative AssetManager/streamable manager.

**Implementation:** retire receipt generations under an explicit replay-safe bound; expose retained cloud revision history or provider-authoritative ordering instead of client-clock max; establish callback thread delivery and controlled archive retention. Add mission/encounter async preload handles and residency ownership for promotion/proxy/cue dependencies; preserve current representation when preload is unavailable. Show critical captions before optional audio completes. Replace insertion-order scans with relevant/fair bounded candidate selection and current-target reservation. Make disabled diagnostics actually inactive.

**Tests/failure cases:**100k+ real transactions with stale receipts retained by the test; repeated restore on persistent weak-point actors; clocks ahead/backward and concurrent clients; corrupt highest revision; worker-thread terminal callback; quota reached; delayed asset load followed by new mission; GC with hidden Mass participants;256 irrelevant actors before the near weak point and32 earlier anchors. Record allocations, request counts and residency lifetime, not just returned booleans.

**Risk:** high if bounds weaken replay protection, and medium/high for memory-latency tradeoffs. Never evict history or assets merely to make a count look smaller; retain the valid ownership semantics.

**Definition of done:** bounded histories still reject stale duplicates; all valid retained cloud copies are reviewable; no callback permanently poisons the provider queue; no hot-path synchronous asset load is required for an admitted transition/cue; critical target feedback has a bounded appearance deadline independent of actor order; measured long-session memory stabilizes.

## Slice 7. Executable campaign and performance qualification

**Playable improvement:** fixes are demonstrated together in actual missions and target builds, with repeatable evidence that prevents regression.

**Findings/gates:** V01–V03, C07 and unresolved device/content acceptance across every report. **Depends on:** repaired source plus full authored content, required plugins, licensed platform engine/SDKs and hardware. This slice includes engineering validation tools; Blueprint asset creation and final art/animation remain the creator/content team's work.

**Systems/files:** `Scripts/Validate-Unreal.ps1`, `Check-UnrealReport.py`, `Check-ConsoleBuild.py`; `P/Private/Validation/SovValidateCampaignCommandlet.cpp`; existing mission/encounter/native functional fixtures; profiling scopes at actual owners; target/module/platform configs; `P/Private/Campaign/SovCampaignMassProxy.cpp` and its existing Mass bridge for an animated Tier-C representation if required.

**Implementation:** wire real clean-build, native test, typed asset/world validation, Blueprint/map validation, representative cook and packaged-route stages into one evidenced candidate gate. Add malformed-asset fixtures and historical save fixtures only for formats actually supported. Supply deterministic route/soak/performance controllers and frame/memory report checking. Keep frozen pose proxies appropriate for Tier D; moving Tier C needs an explicit measured animation representation, not an assumption that assigning an asset animates a poseable component.

**Tests/failure cases:** clean boot M01 Tarrik, combat death/retry, proof-gated scenes, market/campaign completion, M01→M02 travel, Selene infiltration/shot/escape, Lyric arrival and extraction/Voss; save/reload immediately around protected beats; M12/M13 handoff and return;100 death/retry cycles; all normal/skip cinematic paths; account/suspend at every boundary; low space/process kill; source/controller/streaming replacement. Negative world fixtures must catch missing GUIDs, excessive roles, absent sequence policy and forbidden classes even in innocuously named folders.

**Performance evidence:**30/60/120 fps gameplay timing; separate Series S, Series X, PS5 and PS5 Pro captures; TDD 60fps performance/30fps quality goals, frame percentiles, cold/warm streaming, dynamic-resolution floor, shader/PSO readiness and memory across three reloads. Collect actual CPU/GPU/RHI/audio/input/accessibility output. Do not infer platform support from public allowlists or a Win64 editor build.

**Risk:** high integration risk from unavailable assets/vendor reconciliation; controlled by clear manifests, bounded capture routes and failure injection. No invented console certification rules or numbers.

**Definition of done:** the exact candidate's clean build, automation, validation, cook, packaged missions, recovery, soak and target measurements all pass their declared gates; warning waivers have an owner and expiry. Missing SDK/content/device evidence keeps only the relevant gate blocked and is visible in the final result. An overall AAA engineering acceptance cannot precede this evidence.
