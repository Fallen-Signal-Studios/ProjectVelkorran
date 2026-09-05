# Code error pass — 5 September 2026

Reviewed merged baseline `6b754d85bfa9c78916c028fb85656dab9a633030`. Eight bounded defects are repaired in the working tree. Significant transaction and persistence liabilities remain below; the project is not yet validated for Unreal compilation or playable integration.

This pass reviewed project build layout, Narrative integration, combat/ability callbacks, campaign encounters, save/travel ownership, and the merged corruption/status systems. Earlier audit findings were checked against current source. This is not exhaustive proof of the codebase, Blueprint assets, or authored levels. The repairs, regression tests and audit notes are included in this change set.

## Repairs

Paths below are relative to the repository unless indicated otherwise.

| Defect | Change and affected source |
| --- | --- |
| B1: duplicate test helpers can collide in a unity build | Isolated and qualified `Activate<T>` and `Shield` under distinct namespaces in `Source/ProjectVelkorran/Private/Tests/SovTarrikPayloadRuntimeTests.cpp` and `SovSelenePayloadRuntimeTests.cpp`. |
| B2: source eligibility changes with unity composition | Renamed the anonymous `Alive` helpers in `Private/Combat/SovProtectionInterceptReceipt.cpp` and `SovSelenePayload.cpp` to distinct policy names. Preserved each policy's existing missing-attribute-set behavior. |
| B3: generated header is not the final include | Moved debugger includes before `NPCActivityComponent.generated.h` in NarrativeArsenal's `Public/AI/Activities/NPCActivityComponent.h`. |
| B4: Windows GPU diagnostics use unsafe COM/interface handling | NarrativeArsenal's `Private/ArsenalStatics.cpp` now uses scoped COM pointers, checked queries, the correct adapter interface, initialized outputs and bounded conversions. Monitor discovery checks Slate initialization. The function still reports adapter zero's process usage/budget; it does not establish Unreal's active render adapter or physical VRAM capacity. |
| ED-03: Deflection startup can resume after cancellation | Added activation/window generations, original avatar/ASC checks, cleanup ownership before callbacks, and valid/scope-aware teardown in `Private/Abilities/SovGameplayAbility_SeleneDeflection.cpp` and `Private/Components/SovDeflectionComponent.cpp`. A retired activation cannot continue over its replacement. |
| ED-04: committed finisher phase can lose its outcome event | `Private/Abilities/SovGameplayAbility_Finisher.cpp` captures the committed phase event's source, target and phase before damage callbacks, and delivers that outcome even when damage cancels the remaining action. Destroyed targets are excluded. Save-time reentry and target recreation still need separate validation. |
| C02: final required death during optional Mass promotion can strand victory | `Private/Campaign/SovEncounterDirector.cpp` reevaluates confirmed death receipts after pending promotion work stabilizes, with authority, mutation and teardown guards. It does not treat arbitrary actor destruction as a kill. |
| New: corruption bypasses global immunity | `Private/Components/SovCorruptionComponent.cpp:54` now respects global damage immunity, both invulnerability tags, blanket status immunity and corruption-specific immunity. Unrelated Burn immunity does not block corruption. Existing story exposure is preserved while owned combat penalties are suppressed. |

`Private/...` in this table means `Source/ProjectVelkorran/Private/...`. NarrativeArsenal is under `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal`.

## Highest-priority remaining liabilities

These are source-supported gaps, not claims that a particular authored level has already failed. P1 means resolve before engineering acceptance of the affected flow.

| Priority / ID | Evidence and consequence | Required follow-up |
| --- | --- | --- |
| P1 / ED-01 | NarrativeArsenal `Private/GAS/NarrativeAttributeSetBase.cpp:649` broadcasts Shield break after snapshotting Health; `:659` then writes Health using the old snapshot. A nested accepted hit can be overwritten. Implicit attribute-change callbacks also permit reentry. | Serialize damage per target or introduce a complete staged transaction contract. Test nested Shield/Health callbacks, fatal state, receipts and damage conservation. Merely moving one broadcast does not close all boundaries. |
| P1 / PC-02 | `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_Echo.cpp:204` assigns spend success after `TrySpendEcho` callbacks; `:306` calls Narrative activation before checking whether cancellation retired this activation. | Capture a shared-base activation/payment generation and check it after every callback boundary. Test cancellation and immediate reactivation during payment through real GAS. |
| P1 / ED-02 | `Source/ProjectVelkorran/Private/Combat/Pickups/SovCombatSustainPickup.cpp:173` calls `TryGrantTo` before reserving `bClaimed`. Echo/inventory callbacks can trigger another overlap and duplicate the grant. | Reserve collection before resource callbacks; preserve failed and partial grants. Validate nested overlaps and inventory conservation. |
| P1 / new status persistence gap | `Source/ProjectVelkorran/Public/Components/SovStatusComponent.h:125` implements no Narrative savable interface. Runtime status/checkpoint fields at `:347` and `:355` are unreflected. Capture/restore APIs in `Private/Components/SovStatusComponent.cpp:1421` have no external native callers. NarrativeSaveSystem's `Private/Subsystems/NarrativeSaveSubsystem.cpp:729` skips non-savable components. Authored `PersistFullDuration` / `PersistRemainingDuration` policies therefore do not enter native campaign saves. | Add a versioned Narrative component adapter with a SaveGame checkpoint member, capture during PrepareForSave, required definition assets and readiness-aware restore. Test actual campaign save/load and protagonist handoff; preserve the canonical mission corruption adapter separately. |
| P1 / C01, SP-03 | `Source/ProjectVelkorran/Private/Framework/SovPlayerController.cpp:656` handles synchronous `ServerTravel` rejection, but no native travel/network failure binding recovers an accepted request that fails later. `Private/Framework/SovApplicationLifecycleComponent.cpp:170` also excludes Travelling from interruption pause. | Keep recoverable transition ownership across worlds; handle asynchronous failure, restore/unlock the source when possible, and provide an actionable fallback. Test map-load failure and account/controller loss while the source world remains live. |
| P1 hardening / SP-01 | `Source/ProjectVelkorran/Private/Save/SovSaveSubsystem.cpp:453` performs storage writes/readback without an operation-wide owner/generation check at every later I/O boundary. Existing busy-state and post-capture checks reduce exposure but do not establish this contract. | Carry immutable account/user/generation authorization across serializers and storage callbacks. Add revocation/failure witnesses. No cross-account write was observed in this pass. |

The [combat note](CodeErrorPass-2026-09-05-Combat.md) also verifies active melee ignoring a new stagger, Judgement's missing eye-to-muzzle obstruction check, drone release after a disabling callback, dead/retired-avatar Shield/Poise writes, and unbounded weak-point receipt history.

Other historical findings, including unexpected encounter-participant destruction, UI/accessibility ownership and Shipping test-fixture isolation, remain tracked in the [earlier audit roadmap](AdversarialAudit-2026-09-05/Roadmap.md). Its original reports describe their original revision; use the repair table above for the status of B1–B4, ED-03/04 and C02 after this pass.

## Validation and remaining build gates

| Check | Result | Scope |
| --- | --- | --- |
| `python3 Scripts/Test-NativePolicies.py` | Exit 0; **39 suites passed** | Existing production-used portable policies, host C++17 compiler and sanitizer checks. Does not execute reflected runtime owners. |
| `python3 -B -m unittest discover -s Scripts/Tests -p 'Test*.py'` | Exit 0; **30 tests passed** | Includes three new checks in `Scripts/Tests/TestUnrealSourceLayout.py`: generated-header order across 487 reflected headers, actual extracted payload helpers in one translation unit, and living-policy behavior in isolated/both unity orders. These use minimal host stubs, not Unreal compilation. |
| `python3 Scripts/Test-AxiomPulseMath.py` | Exit 0; **34 checks passed** | Production Axiom charge/geometry math. |
| `git diff --check` | Passed | Whitespace/error-marker hygiene for the changes. |
| `python3 -B Scripts/Check-ConsoleBuild.py` | Exit 1; **blocked** | Four required Narrative modules still allow only Win64/Android/Linux. ZenDyn is enabled but unresolved in supplied plugin roots. The invocation supplies no UE 5.7 engine roots, so unresolved engine plugins in its output are environment gaps, not proof those standard plugins are absent from a correct engine install. |
| UE 5.7 UHT/UBT, Windows SDK, native automation, cook and packaged play | **Not run** | The project explicitly targets UE 5.7. This machine has UE 5.5, which is not a valid substitute. Actual compilation and content integration remain unverified. |

Five new native regressions are authored and source-reviewed, but require UE 5.7:

- `ProjectVelkorran.Campaign.Deflection.ReentrantWindowOwnership`
- `ProjectVelkorran.Campaign.Deflection.ReentrantActivationOwnership`
- `ProjectVelkorran.Campaign.Finisher.CommittedOutcomeSurvivesCancellation`
- `ProjectVelkorran.Campaign.Mass.FinalRequiredDeathDuringOptionalPromotion` (successful and failed promotion paths)
- `ProjectVelkorran.Campaign.Corruption.GlobalImmunity` (eight immunity contracts and unrelated Burn immunity)

The next validation step is a matching UE 5.7 unity/non-unity build and execution of these native tests, followed by the transaction and persistence repairs above. A passing host suite cannot certify GAS callback timing, UHT reflection, Windows COM runtime behavior, or authored campaign save/restore.
