# Campaign/status merge resolution — 2026-09-05

Resolved the 14 conflicts between `main` at `7e04b64` (status/corruption prototype) and `codex/audit-axiom-null-pulse` at `0ff7b33` (campaign engineering and audit). Related automatic-merge integration errors were corrected as part of the same resolution.

## Decisions

- The newer mission-scoped `USovCorruptionComponent` is the campaign authority, including profile permissions, save records, destination restore barrier, accessibility and verified source/remedy paths.
- The prototype is retained as `USovLegacyCorruptionComponent`, with separate `ESovLegacyCorruptionBand` and `FSovLegacyCorruptionSourceHandle` identities. Its field/remedy volumes, semantic checkpoint types, tests and PlayerState compatibility attributes remain available. Existing legacy callers now explicitly reference the legacy component.
- Managed campaign restore rejects a pawn carrying the optional legacy component. Legacy writes reject an active campaign mission. This prevents two corruption systems from owning pressure on the same campaign pawn.
- Campaign controller/player-state saves, Technique progression, NPC encounter restore, generic status initialization, and all new player systems are retained. Duplicate corruption construction, getters and member declarations from the automatic merge were removed. Campaign corruption uses its mission restore barrier instead of the legacy ASC initialization/readiness API.
- Gameplay tags from both branches are retained with one declaration and registration per identity.
- The damage resolver preserves source-policy and immunity filtering, defense/control-only acceptance, and generic typed status requests. Fatal hits do not apply a new status. Native Selene control and native Tarrik payload-owned Burn mark `Sov.Status.Application.NativeOwned` to consume their damage receipt without also triggering the generic status listener. Explicit all-status and family immunities remain respected.
- Selene retains logical attacker validation, perfect-deflection replay protection and Exposed provenance, plus incoming weak-point hit, precision-chain, marked kill and bypass behavior. A status exposure and an authored mark on the same victim yield one kill reward, with damage replay consumed before mutable reward policy.

## Legacy content migration

For standalone prototype maps, explicitly add `USovLegacyCorruptionComponent` to the intended player Blueprint. The project PlayerState still supplies its required `USovCorruptionAttributeSet`. Existing `ASovCorruptionFieldVolume` and `ASovCorruptionRemedyVolume` now target the legacy component. Rebind prototype component method calls, band pins and source handles to their legacy types, preserving authored tuning, then compile/save the affected Blueprints. The renamed reflected component and types require an explicit asset migration; the source changes do not automatically rewrite binary assets.

For campaign maps, keep the inherited mission-scoped component, omit the legacy component, and migrate fields/remedies to the validated profiles and producers in [CorruptionEngineering.md](CorruptionEngineering.md). Legacy raw status exposure does not bypass campaign mission permissions. No automatic class redirect is added because the original `USovCorruptionComponent` identity now belongs to the campaign implementation.

## Validation

- `python3 Scripts/Test-NativePolicies.py`: **39 portable C++ suites passed**, using warnings as errors and undefined-behavior sanitization.
- `python3 -m unittest discover -s Scripts/Tests -p 'Test*.py'`: **27 tests passed**.
- `python3 Scripts/Test-AxiomPulseMath.py`: **34 production math checks passed**.
- Source checks: no conflict markers; 219 gameplay-tag members match 219 unique registrations; no duplicate project type definitions, duplicate includes or unmatched delimiters in the resolution files; `git diff --check` passes.
- Added Unreal regression coverage: `ProjectVelkorran.Campaign.Status.NativeAndGenericOwnership`, `ProjectVelkorran.Campaign.Echo.ExposureAndMarkSingleReward`, and `ProjectVelkorran.Campaign.Corruption.LegacyIsolation`.

**Unreal compilation, these new runtime tests, Blueprint compilation and PIE have not run.** This Mac has Unreal 5.5; the project targets 5.7. Run the actual 5.7 Development Editor build and complete automation namespace using [UnrealValidation.md](UnrealValidation.md), followed by the content migration/PIE checks. Portable tests and source checks do not establish an Unreal build result.
