# Adversarial engineering repairs — 5 September 2026

This change implements repairs against all 45 findings in the [adversarial audit](../AdversarialAudit-2026-09-05/README.md), with the partial dispositions below retained explicitly. It preserves the existing Narrative ASC, inventory, save, campaign, input and Mass owners. It does not certify compilation readiness or an airtight runtime: UE 5.7, UHT/UBT, game assets, console SDKs and target hardware are absent from this workspace. No editor validation was performed.

The largest changes close callback lifetime failures: an earlier action must not resume after cancellation, an earlier hit must not own a newly restored life, and an earlier account operation must not write under replacement authorization. Each system retains its own concrete commit and completion rules. There is no new universal transaction manager or parallel gameplay system.

## Review and evidence

- Audit parent tree: `d8fd0407f0d9bb310715526ec46660e16ca19d85`; remote audit commit `7c970ff28428c8165bc695c3ffc887e9cacaa8d1`. The audited runtime was integrated at `30c404fd9dae1a5a8fcc5d78f563ac9f56c45a4d`.
- Source target: TDD v2.0, revised 14 August 2026, with the approved exceptions in [CampaignV2ChangeLog](../CampaignV2ChangeLog.md). The December attachment is an older design revision.
- [Validation results](Validation.md) distinguish executed host/portable checks from authored Unreal regressions and unavailable engine gates.
- [Exact changed-file inventory](FilesChanged.md) includes the Editor test-module relocation and Narrative patch paths.
- Detailed repairs, risks and native acceptance cases: [build/validation](BuildValidationRepairs.md), [player combat](PlayerCombatRepairs.md), [combat transactions](CombatTransactionRepairs.md), [enemy defense](EnemyDefenseRepairs.md), [campaign/world](CampaignWorldRepairs.md), [save/platform](SavePlatformRepairs.md), [UI/narrative](UINarrativeRepairs.md), and the additional [dialogue startup review](NarrativeStartRepairs.md).

## Finding dispositions

**Implemented** means source changes and a regression path exist; it never means Unreal compilation or runtime acceptance passed. **Partial** means a source/provider limitation remains in addition to engine validation. The domain reports identify exact classes, changes, risks and tests. All native tests now belong to `Source/ProjectVelkorranTests/Private/Tests/`.

| Finding | Disposition | Repair | Remaining acceptance or source limitation |
|---|---|---|---|
| B1 | Implemented | Unique file-local test helper names eliminate reproduced unity redefinitions. | Real Editor unity and non-unity builds. |
| B2 | Implemented | Separate Selene/protection predicate names preserve eligibility semantics across unity composition. | Native payload tests in both permutations. |
| B3 | Implemented | Generated header is the final include in the affected reflected header. | UE 5.7 UHT. |
| B4 | Implemented | Scoped COM ownership, checked DXGI queries, RHI adapter matching, bounded query cache and separate physical/budget values. | Windows compile, failed-query and hybrid/multiple-adapter tests. |
| B5 | Implemented | Reflected fixtures moved from runtime modules to one Editor module; explicit module wiring and narrow cross-module exports. | Modular Editor link and proof of absence from Game Shipping generated code/cook. |
| B6 | Implemented | Shipping does not create diagnostics; disabled development diagnostics do not tick or retain combat bindings. | Native toggle test and Shipping inspection. |
| PC-01 | Implemented | Melee interruption listeners and activation epochs retire charge, sweep, montage and Busy ownership. | Real Poise/cancel/notifies and charged attack timing. |
| PC-02 | Implemented | Echo debit and deferred End continuations require the same activation, avatar and ASC. | Real GAS scope-lock, payment-callback and target-data tests. |
| PC-03 | Implemented | Exact inventory debit, clip reservation, membership/resource revisions and callback lifetime pins. | Removal denial, reload/reentry, resource replacement and actual weapon assets. |
| PC-04 | Implemented | Eye-to-muzzle obstruction bridge and forward-convergence checks prevent thin-cover bypass. | Physics wall regression and authored muzzle geometry. |
| PC-05 | Implemented | Judgement captures release ownership and uses typed damage results rather than inferred health deltas. | Callback cancellation/restart during direct and radial hits. |
| PC-06 | Implemented | Bounded per-result native consumption receipts replace lifetime GUID ledgers; sever proofs retire on owner reset. | Reflected copies, replay, long sessions and restore regressions. |
| PC-07 | Implemented | Ignored Selene GE override fields retain serialized identity but are deprecated and validated before payment. | Migrate incompatible authored overrides; native validation test. |
| PC-08 | Implemented | Aim and hip recoil select their respective presets. | Deterministic fixture and review of assets compensating for the old inversion. |
| ED-01 | Implemented | Nested debits use current resources; explicit events follow commits; exact avatar/life epochs fence stale fatal outcomes; finite input admission and bounded arithmetic prevent overflow. | Native nested GAS attribute/event callbacks, restore-and-kill, avatar retirement and large-coefficient tests. GAS attribute writes remain individually observable. |
| ED-02 | Implemented | Pickup reserves itself before granting, pins callback-live owners and releases only failed/partial grants. | Real Echo overlap reentry, ammo partial grants, destruction and replication. |
| ED-03 | Implemented | Deflection uses exact activation/window ownership and retires native continuation before teardown callbacks. | Cancellation and same-instance replacement inside startup/window delegates. |
| ED-04 | Implemented | Target-owned saved phase outbox survives canceled animation and checkpoint capture; finisher action and reservation are fenced through team-policy callbacks. | Save/load and late-ASC readiness tests; authored phase consumers must be ready and idempotent by target/phase. Already-lost historical outcomes cannot be inferred. |
| ED-05 | Implemented | Weak points consume shared result receipts, including hits received while already broken; reset cannot replay consumed copies. | Actual damage/reset/copy and forged-result tests. |
| ED-06 | Implemented | Shield/Poise recovery retires immediately on death or owner loss and starts fresh delays after restoration. | Timers, foreign tag owners, fatal callbacks and avatar replacement. |
| ED-07 | Implemented | Drone interruption listeners, release generations and owned movement cleanup match the stronger enemy ability lifetime contract. | Interrupted release and committed self-blast tests with real AI movement. |
| C01 | Implemented | GameInstance save owner retains mission travel receipt, watchdog and bounded recovery beyond controller destruction; title escape preserves account fences. | Accepted/rejected/failed travel, missing destination controller and cooked frontend. |
| C02 | Implemented | Confirmed defeat survives Mass mutation and completion reconciles after promotions settle. | Final required kill during optional promotion; exactly one victory/reward. |
| C03 | Implemented | Required participant loss fails the encounter and routes owned living-player recovery without fabricating a kill. | Destroy/stream out actor, lose entity, demote intentionally and retry. |
| C04 | Implemented | Cinematic startup consumes the exact one-use save-failure acknowledgement before another write. | Real low-space modal to cinematic path and callback ownership. |
| C05 | Implemented | Focus command approaches the selected target using curated attack reach and the existing leader leash. | Native MoveTo identity/cadence plus real navmesh, obstacles and moving targets. |
| C06 | Implemented | Director-owned strong residency and bounded asynchronous admission retain Mass transfer assets independently of source/proxy actors. | Forced GC, cold load, timeout and indirect Narrative restoration profiling. |
| C07 | Implemented; content required | Existing Tier C proxy supports compatible in-place animation profiles and refuses unsupported skeletal demotion while preserving the source. | Authored loops, modular alignment, promotion pose and hardware crowd budgets. |
| SP-01 | Implemented | Immutable account-generation receipts fence storage, load, profile selection and cloud review across every callback/I/O boundary. | Real native account changes, same-account ABA, suspension and provider callbacks. |
| SP-02 | Implemented | Native Return to title abandons session authority and offers retryable failure feedback. | Separate cooked frontend and failed asynchronous title travel. |
| SP-03 | Implemented | Travelling source worlds retain the application interruption pause. | Packaged overlay/suspend/travel sequence. |
| SP-04 | Implemented | Enumerated immutable cloud revisions and explicit selection replace client-clock latest ordering. | Worker-thread transport, multiple devices, quota/deletion and SDK provider contracts. |
| SP-05 | Partial | Bounded raw frame, checksum, genuine GVAS header/class admission and known-object decode precede campaign restore; current unframed saves migrate through the alternate bank. | Generic backend allocation occurs before the project byte cap. Deep legacy reflected allocation needs UE fuzzing; CRC is accidental-corruption detection, not authentication. Unknown engine layouts fail closed and retain original bytes. |
| UI-01 | Implemented | A bark interrupting suspended dialogue owns its subtitle independently. | Dialogue/bark interruption with real widget and audio timing. |
| UI-02 | Implemented | Normal dialogue end retains unread pages under their speech receipt. | Multi-page normal completion and later line replacement. |
| UI-03 | Implemented | Paused completions defer under dialogue/node/revision ownership; startup continuations also reject reentered lines. | Actual timer/audio callbacks during pause, exit and same-node replacement. |
| UI-04 | Implemented | Caption priority/coalescing protects critical feedback from low-priority damage spam. | Real viewport timing and controller/TV readability. |
| UI-05 | Implemented | Verified account ownership controls bounded, checksummed preference banks and Enhanced Input persistence; account changes reset/apply the matching profile. | UE 5.7 profile serialization, callback reentry, physical user routing and migration behavior. |
| UI-06 | Implemented | Removed choice widgets rebuild from the still-owned live dialogue. | Remove/recreate/focus restoration in the actual viewport. |
| UI-07 | Implemented | Native archive review/replay consumer preserves critical-record knowledge boundaries. | Record availability and replay presentation across account/mission changes. |
| UI-08 | Implemented | Nearby physics candidates and focus-first ordering precede the marker cap. | Collision configuration, dense encounters and real focus/occlusion behavior. |
| V01 | Implemented gate framework | Staged build/automation/world/Blueprint/cook/package runner records current source and package identity and rejects skipped/stale/partial qualification. | Real licensed worker execution and completion of required coverage categories; no workflow was dispatched. |
| V02 | Partial | Existing commandlet now checks loaded placed-world identities, encounters, cinematic/anchor contracts and native ability defaults. | World Partition/streaming enumeration, generalized status/cleanup declarations and exhaustive prohibited-type coverage remain source work. Missing coverage must not qualify a candidate. |
| V03 | Partial | Runtime CPU scopes and strict capture checker enforce combat workload, frame, reload/memory, promotion, PSO and latency evidence contracts. | Content-specific route driver, profiler normalization and actual cold-cache hardware captures remain required. Synthetic checker tests do not measure the game. |
| V04 | Implemented | Accepted bark captions appear immediately; asynchronous audio/residency completes only for the captured witness, cue and mission context. | Cold/warm assets, delayed/failing requests and late context replacement. |

## Integration risks and migration

1. **Combat ordering:** cancellation and nested damage behavior is deliberately stricter. A committed resource debit is not refunded merely because its presentation is canceled. A committed phase outcome remains owned by its target. GAS attribute callbacks can observe individual attribute writes; this change does not pretend a multi-attribute setter is atomic.
2. **Saved state:** new framing preserves the existing serializer body and uses alternate-bank migration. Unknown header layouts, invalid owners and ambiguous provider callbacks fail closed. Preserve the original files when investigating decode failures. Completely corrupt banks with full preservation capacity require verified recovery before cleanup can proceed.
3. **Content compatibility:** migrate retired Selene overrides, review recoil compensation, supply compatible Mass loops and retain a cooked frontend. No Blueprint assets were rewritten. Missing or incompatible assets are reported or refused before destructive transition. Legacy unheard-record saves lack witness provenance; their bytes are retained, but unsafe inferred disclosure/replay is withheld.
4. **Narrative maintenance:** the plugin is intentionally patched at its existing ownership boundaries. Review the exact plugin paths in the file inventory when reconciling any vendor update, then rerun the adverse callback tests. Do not replace the patched plugin wholesale without reconciling those changes.
5. **Console readiness:** platform allowlists, missing third-party dependencies and licensed SDK implementation are not bypassed. Host success is not Xbox Series S/X or PlayStation 5/Pro qualification.

## Next three acceptance slices

| Order | Work | Definition of done |
|---|---|---|
| 1 — UE 5.7 compile and native repair acceptance | Restore exact required plugins; run clean Editor unity/non-unity and Game Development/Shipping builds using `Scripts/Validate-Unreal.ps1`; fix UHT/API/export errors; run the full current native test inventory; fuzz framed and genuine compatibility saves. Complete generalized validator declarations and required map enumeration against the real engine. | Every required build and native test passes, reflected fixtures are absent from Shipping, save migration/owner rejection is proven, and coverage omissions remain explicit failures. |
| 2 — Packaged Level 1/Level 2 recovery route | Bind a reviewed deterministic driver to authored mission content; complete Blueprint/world/cook gates; drive combat, rewards, final-kill promotion, participant loss, handoff, cinematic save refusal, account loss and failed travel with repeated reloads. Normalize retained profiler output for the checker. | Both levels complete end to end; each injected failure reaches one recoverable state; no duplicate resource/progression outcome; current immutable package and raw route evidence are retained. |
| 3 — Console workload and lifecycle qualification | Build with licensed Series S/X and PS5/Pro workers; measure cold/warm combat, Tier C animation/residency, save latency, frame-rate behavior, three-reload memory and PSO readiness; exercise users/controllers, low space, suspend/resume and provider failure. | Each intended target independently meets reviewed TDD budgets and platform lifecycle requirements with retained measurements; no desktop result is substituted for target evidence. |

The source repair definition of done is a reviewable change set, meaningful regression coverage, passing available host checks and explicit unresolved limitations. Shipping acceptance remains blocked until the required engine, content and target evidence exists.
