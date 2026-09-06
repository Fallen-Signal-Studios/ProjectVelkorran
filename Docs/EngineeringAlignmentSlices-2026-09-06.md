# Three engineering alignment slices

Baseline: merged PR #34, remote main `93bf5c2c6e292e7ca0df05c3fe3ae677cadca57e`.
Design authority: August 2026 TDD v2 plus `CampaignV2ChangeLog.md` exceptions.

## Delivered source

1. **Save/travel ownership** — [PR #35](https://github.com/Fallen-Signal-Studios/ProjectVelkorran/pull/35). The prior source had unowned callback gaps at storage/serialization boundaries and no GameInstance origin-recovery transaction after accepted mission travel. Extended the existing save subsystem, controller/GameMode integration and lifecycle pause ownership. Retained checkpoints, per-attempt Narrative copies, bounded recovery, explicit original-account retry and 15 native regressions. See `MissionTravelRecovery-2026-09-06.md` and `SaveOperationOwnership-2026-09-06.md`.
2. **Combat and resource continuations** — [PR #36](https://github.com/Fallen-Signal-Studios/ProjectVelkorran/pull/36). Drone and Judgement callbacks could continue an old action; passive defenses could write through reused ASCs; resource restores could consume mutated input or leave stale holds. Repaired the existing abilities/components/snapshot library and added 42 native regressions. Independent review also repaired ordinary revive ordering, spec-construction callbacks, recursive Poise refill and fatal explosion suppression. See the drone, Judgement and passive-defense reliability documents.
3. **Editor-only test isolation** — moved 137 existing test/fixture files into `ProjectVelkorranTests`, promoted three existing production helper contracts, corrected the faction Actor fixture construction, and added one real-DroneNPC death-suppression regression. Added source and supplied packaged-evidence checks and 13 host negative/positive tests. See `EditorTestIsolation-2026-09-06.md`.

The PRs are stacked: save/travel → combat → isolation. Review and merge in that order, retargeting the next PR to main after its dependency is merged. None was merged by this work. Draft status records outstanding UE validation, not unfinished publication.

## Validation and remaining gates

- Host: 43 Python checks and 39 portable C++17/GCC/UBSan policy suites pass; clean whitespace checks.
- Inventory: 352 ProjectVelkorran native registrations (294 baseline + 58 added), plus the preserved Narrative faction test. All pre-relocation registrations survived. 158 reflected fixtures live exclusively in the Editor test module.
- UE 5.7 UHT/UBT, modular linking, native automation, cooked content, actual multiworld recovery, packaged launches and console hardware/SDK testing are **not run** in this workspace. The packaged checker reports missing evidence as missing, not a pass.
- Source fixes stop invalid continuations; they do not roll back arbitrary already-published Blueprint/GAS mutations. Existing suspended/retry owners remain essential. No engineering-completion percentage or engine-ready guarantee is claimed.

## Next three engineering slices

1. **Coordinated resource snapshot schema**: define and migrate base/current/max semantics across protagonist, encounter and Narrative ASC saves; then support persistent continuous status resource modifiers without double application. Preserve PR #34's rejection until round-trip/migration and callback tests prove the replacement contract.
2. **Bounded save decoding and recovery corpus**: validate a bounded outer envelope before UObject/archive allocation, retain existing bank authority and compatibility rules, and add truncated/oversized/invalid-class/archive fuzz cases. This is the still-open SP-05 work, not covered by ownership fences.
3. **Durable finisher phase outcomes**: resolve the save/reentry window between a committed phase and its emitted outcome, including cancellation, target destruction and save/load. Extend the existing target phase ledger and receipt architecture with exact once-only semantics; avoid a parallel event or boss-phase system.

Cloud revision conflict policy (SP-04), authored recovery UI, real navigation/presentation behavior and console qualification remain separately documented dependencies. The next source work should be accompanied by the actual UE test run as soon as the engine host is available.
