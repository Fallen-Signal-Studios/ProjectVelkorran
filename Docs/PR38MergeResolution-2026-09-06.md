# PR #38 integration resolution, 2026-09-06

Resolved the combined #38/#39 head `1378a21bfe0542b49981960fe31252d1f0991e98` against main `392148342eef6bbbc38a1c48340f038da8e8317d` (including #35 and the #36/#37 integration in #40).

## Resolution decisions

- Keep main's per-operation storage authorization fences and retained GameInstance mission-recovery owner. Adapt #38's recovery UI to that owner, not a second travel state machine.
- Preserve #38's durable pre-travel checkpoint, serialized operation/origin-generation identity and managed pawn/ASC restoration lease. Distinguish mission and slot-load URL phases and validate the actual destination map. Automatic origin recovery remains single-attempt; explicit same-account retry retains the decoded origin.
- Preserve main's combat actor-info/life ownership, drone continuation and Cinder geometry protections. Use #38's native damage receipt for Judgement acceptance, including damage immediately repaired by a callback.
- Integrate #38's resource schema v2 base/current handling with main's callback ownership and generation-specific passive restore barriers. Retain fail-closed legacy modifier admission and status restoration. Preserve same-pointer readiness-generation protection.
- Retain objective lifecycles, HUD/full-text review, handoff knowledge filters, critical captions/dialogue, finisher receipts, source manifests and Mac/Windows qualification tooling from #38/#39.
- Keep one Editor-only test module implementation and main's non-Editor build guard. Adapt overlapping tests to the canonical APIs without dropping registrations.

The older slice documents describe their original branches. This record supersedes their conflicting recovery API and test-module integration details.

## Validation and limits

- 78 Python tests passed.
- 42 portable C++17 production-policy suites passed with GCC, warnings as errors and UndefinedBehaviorSanitizer.
- Source isolation audit passed: 163 reflected fixture classes and 400 native registrations in the Editor module. Regression checks require unique native test classes/names and exactly one module implementation. Renamed an overlapping drone test class while preserving both test registrations.
- Conflict-marker and Git whitespace checks passed.

These are source/host results, not Unreal execution. UHT/UBT, the 400 native registrations, cooking, packaging and the authored M12-M13 route still require UE 5.7 qualification. No authored binary assets were changed. Test counts are not TDD completion percentages.

Sources: [PR #38](https://github.com/Fallen-Signal-Studios/ProjectVelkorran/pull/38), [PR #39](https://github.com/Fallen-Signal-Studios/ProjectVelkorran/pull/39), [integration #40](https://github.com/Fallen-Signal-Studios/ProjectVelkorran/pull/40).
