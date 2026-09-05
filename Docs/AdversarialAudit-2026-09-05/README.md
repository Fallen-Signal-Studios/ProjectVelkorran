# Project Velkorran: adversarial engineering audit

5 September 2026. **Verdict: do not sign off this revision as compilation-ready or as an airtight gameplay layer.** The game has substantial native systems worth preserving, but there are concrete compilation-permutation hazards, combat transaction defects, progression dead ends and accessibility failures. Some previously described closure items are implemented but not robust at their boundaries. Passing portable policies did not catch these failures.

This is a source audit and corrective roadmap. Runtime source, configuration and existing tests were left unchanged so every finding remains attributable to the reviewed revision. This audit does not claim to have repaired the findings. Only reports, source inventory, executed host logs and isolated reproduction artifacts were added.

## Source authority and scope

| Item | Audited value |
|---|---|
| Integrated revision | `30c404fd9dae1a5a8fcc5d78f563ac9f56c45a4d`, merge of PR #30 into `codex/audit-axiom-null-pulse` |
| Published source commit | `362276627311e380af47adb4d8556e1aa0dad860` |
| Local equivalent commit | `6225c68d50fbea605d7c7dfc8d61d5569805e28f` |
| Exact audited source tree | `049edf431aa27a57f6d1ef1c00add8b10fe90de0` |
| TDD | Sovereign Call: Origins v2.0, revised 14 August 2026, `Sovereign_Call_Origins_Technical_Design_Document_v2(3).md` |
| Accepted design changes | [CampaignV2ChangeLog.md](../CampaignV2ChangeLog.md): revised hero rosters, delayed player Health recharge, finite Cinderline ammo, deterministic ordinary damage, transient sustain drops |
| Inventory | 426 project C++/header files, 786 plugin C++/header files; 176,477 combined lines, including tests |
| Engine/content access | No UE5.7/UHT/UBT, console SDKs, devkits, `.uasset` or `.umap` files in this checkout |

GitHub comparison confirmed no source differences between the console PR head and its merged integration state. The December attachment is superseded and was not used to demand removed classes, loot, vendors, crafting, co-op or free protagonist switching.

Six independent domain reviews examined actual runtime code, plugin integration and tests; the lead review cross-checked the high-impact chains, build evidence and verification gates. The inventory is not a claim that every line has been proven correct. This is comprehensive domain coverage with adversarial traces, not exhaustive state-space verification.

The [detailed reports](#detailed-reports) give each principal issue's existing implementation, TDD requirement, failure sequence, exact source locations, preserve/refactor/extend decision, dependencies, change risk, implementation order and regression/acceptance criteria. IDs remain stable within those reports.

## The most consequential findings

| Priority / IDs | Failure and consequence | Required correction |
|---|---|---|
| P1, B1–B3 | Two test suites define identical helpers that collide when unified. Two production `Alive` helpers form a different overload set in a combined file, changing source eligibility. One reflected public header includes debugger headers after its generated include. | Isolate and qualify helper namespaces, make the intended eligibility contract explicit, repair generated-header ordering; compile unity/non-unity and debugger permutations. Host evidence confirms the C++ hazards; actual UBT grouping/UHT diagnostics remain pending. |
| P1, ED-01 | The damage resolver snapshots Health, broadcasts Shield break, then writes the old Health minus outer damage. A nested accepted hit can be erased; fatal state and resource state can disagree. | Serialize per-target damage transactions or stage them with an equally explicit commit contract. Protect implicit attribute callbacks as well as explicit broadcasts. Preserve routing and receipts. |
| P1, PC-01/02, ED-03/07 | Active melee ignores a new poise break. Echo payment and Deflection startup can continue after callback-driven cancellation. Drone release does not consistently recheck disabling state after its hook. | Put activation ownership in the shared base where it is needed, retire owned work on interruption, and revalidate after callbacks. Exact Echo orphan-delegate/K2 outcomes depend on UE cancellation timing; the missing fence is visible in source. |
| P1, PC-03, ED-02 | Ammo accepts invalid quantities and removes inventory before reducing the clip. Pickups grant before reserving their contents. Reentry can authorize extra shots or duplicate resources. | Validate positive quantities; reserve the existing clip/pickup entitlement before publishing callbacks; preserve partial packs and inventory conservation. |
| P1, C02/C03 | Killing the final required enemy while an optional NPC is promoting from Mass can lose the completion attempt forever. Destroying a required actor without a GAS death receipt can stall victory while the next wave advances. | Reevaluate completion after every stabilization boundary; distinguish sanctioned representation teardown from unexpected actor loss; recover without fabricating a kill. |
| P1, C01/C04, SP-02/03 | Accepted asynchronous travel has no source-owned origin recovery transaction. Cinematics ignore the existing continue-without-save receipt. Account loss offers only an unusable Resume action. Travelling is exempted from interruption pause despite a live source world. | Extend existing GameInstance/save coordination across worlds; use exact boundary receipts consistently; provide return-to-title recovery; narrow the initialization pause exemption. |
| P1 hardening, SP-01/05 | Save ownership is checked at entry rather than every subsequent I/O boundary. Local bytes reach Unreal deserialization before a bounded outer integrity check. | Carry an immutable owner/generation token through storage work and validate raw framing before UObject deserialization. These are source-proven missing boundaries, not observed cross-account writes or reproduced archive crashes. |
| P1, ED-04 | A finisher phase can be marked resolved and saved, then lose its outcome event when damage cancels the continuing ability. | Make a committed phase outcome durable and exactly once independently of the remaining animation lease. An actual boss stall depends on its authored event consumer. |
| P1, UI-01–05 | Legal bark interruption loses subtitles; normal dialogue exit drops unread pages; queued completion can advance a paused graph; chip damage erases critical captions; account switching retains another user's settings and setup state. | Share speech/presentation ownership, defer paused completion, arbitrate captions, and select account-owned preferences through the existing identity authority. |
| P1 exposed API, B4 | Narrative's Windows GPU query ignores failures, uses the wrong COM interface contract and leaks references. | Use checked, scoped adapter queries or Unreal renderer metrics. Actual use by an authored widget is unknown; the public native implementation is unsafe if called. |

P1 identifies a stop condition for engineering acceptance, not proof of a particular release blocker on an unseen map. P2/P3 findings still matter: finite receipt histories, truthful designer effect overrides, weapon cover/recoil consistency, companion pursuit, readable choice recovery, all cloud revisions being discoverable, test-fixture exclusion from Shipping, asset residency and efficient feedback. The reports distinguish deterministic source defects, missing integration, architecture limitations and measurements that remain outstanding.

## Why the existing architecture should be retained

The code already contains one Narrative ASC/attribute damage path, real weapon inventory and wield state, hero-specific ability loops and Echo receipts, shield and poise recovery, guard/deflection, native enemy abilities, permanent sever state, weak-point capability consequences, transient sustain, mission/knowledge journals, controlled protagonist handoff, phased save restore, encounter coordination, Mass identity transfer, cinematic postcondition transactions, platform ownership and CommonUI presentation.

The shared problem is inconsistent treatment of callbacks. Some components use receipts and epochs carefully; adjacent components publish an event before committing their own state, or continue after the event without confirming that they still own the action. A successful outer call can then overwrite a nested operation or consume a durable outcome without delivering it. Fix these contracts inside their existing owners. A second damage system, inventory, save manager, speech arbiter or input router would increase the number of boundaries requiring reconciliation.

Three standards should govern every repair:

1. **Committed state and continuing work have different lifetimes.** A released payload or resolved boss phase may remain valid after an animation ends. Its outcome must retain its own identity; it must not borrow the replacement ability's current actor info.
2. **Failure must converge to an actionable state.** Holding input and returning an error is insufficient if no owner can retry, restore the origin or return to the frontend. Encounter completion must be reevaluated after deferred work, not depend on one event arriving at a convenient time.
3. **Bounds must preserve semantics.** Arbitrarily evicting receipt IDs reopens duplicate rewards. Arbitrarily limiting actor scans can hide critical weak points forever. Use operation generations, relevance and explicit retention/residency policies.

## Coverage against the gameplay brief

| Domain | Actual source state and audit implication |
|---|---|
| Tarrik combat | Native melee, guard and revised weapon Echo payloads exist. PC-01/02/03/04/05/08 and ED-01/02 expose interruption, payment, attribution and near-cover problems. A playable loop must connect input, draw, grant, cost, release, result, sustain and reload in one real test. |
| Selene combat | Deflection, precision/disruption generation and revised weapon payloads exist. ED-03, PC-02/05/07 and B2 expose lifecycle and configuration inconsistencies. Collision/reticle/physical-material differences need an actual camera and hurtbox fixture. |
| Echo and feedback | Authority debit, reward receipts and readable-state hooks are real. Protect callback-driven cancellation and collection; bound PC-06 histories without reopening replay; verify real HUD cues rather than assuming an exposed delegate is visible. |
| Weapon state and presentation | Existing Narrative inventory/wield and transforming weapon phase/serial/montage ownership are worth preserving. Repair ammo/recoil/cover contracts and test draw/holster under cancellation, appearance replacement and protagonist handoff. Actual meshes/montages remain an editor integration dependency. |
| Health/shield/poise/guard | One damage route and separate recharge owners exist. ED-01/06 are central correctness risks. Guard's stronger ownership discipline is a useful model for Deflection, not a reason to merge their combat semantics. |
| Enemy abilities | Reformation drone and Dominion Hound/Handler implementations, threat memory, exact ability selection and attack reservations are present. ED-07 targets inconsistent interruption; C02/C03 target encounter ownership. Enemy content cannot be certified from native base classes. |
| Sever/weak points | Permanent region masks, appearance repair, detached limbs, target proof, block counts and saved consequences exist. Do not replace them with runtime slicing. Fix receipt bounds and damage/terminal contracts, then validate authored bones, physics, capability loss and save/retry. |
| Protagonist switching | Authored handoff, separate protagonist snapshots and companion staging exist; free swapping is deliberately excluded. C01/SP-03 show why same-world handoff tests do not establish cross-map travel recovery. |
| Encounters, L1/L2 | Native mission beat skeletons and proof-gated progression exist. C01–C04 can prevent end-to-end progress independently of missing assets. Actual starts, nav, kits, encounter composition and sequences remain uninspected content dependencies. |
| Narrative/UI/accessibility | Real Tales, cue, choice, settings and CommonUI consumers exist. UI-01–08 show integration failures between those producers and consumers. The native HUD layers do not prove the full combat HUD has been authored. |
| Console/platform | Native account mapping, game-instance interruption and system-managed output handling are foundations to keep. Source ownership/recovery gaps remain. Narrative allowlists, absent ZenDyn, licensed engine/platform setup, console narration and hardware validation are still open gates. |
| Architecture/QA/performance | There are valuable unit and native fixture tests. The checked-in gate does not qualify Shipping, cook, maps, actual missions or frame/memory budgets. See V01–V04 and the build report. |

## Executed evidence

| Check | Actual result | What it establishes |
|---|---|---|
| `python3 -B Scripts/Test-NativePolicies.py` | Exit 0, **39 suites passed** | Production-used portable policies compile under strict C++17, warnings-as-errors and UBSan. It does not exercise reflected engine owners. [Log](PortableTests.txt). |
| `python3 -B -m unittest discover -s Scripts/Tests -p 'Test*.py' -v` | Exit 0, **27 tests passed** | Existing host descriptor/preprocessor/report-checker tests pass. [Log](HostTests.txt). |
| `python3 -B Scripts/Check-ConsoleBuild.py` | **Exit 1, blocked** | Required Narrative modules still exclude consoles; ZenDyn and licensed engine/platform dependencies are unresolved. This is expected failure evidence, not a passing console test. [Output](ConsolePreflight.json). |
| Combined actual `Shield` helper bodies | **Compiler exit 1, redefinition** | A real C++ collision when both helpers share a translation unit; not a full Unreal build. [Source](UnityCollisionRepro.cpp), [log](BuildPermutationEvidence.txt). |
| Actual `Alive` helper bodies, isolated and combined | Both compile; results differ **1 versus 0** for the same stubbed source state | Overload behavior depends on translation-unit composition. It does not prove ordinary shipped Selene lacks required attributes. [Source](UnityOverloadRepro.cpp), [log](BuildPermutationEvidence.txt). |
| Source-derived cloud clock counterexample | Earlier publication remains lexicographically greatest after clock correction | Filename selection can hide a later valid publication. No OSS or remote storage request was performed. [Evidence](CloudClockOrderingEvidence.json). |
| Native test registration inventory | **224 unique simple-test registrations** | Tests exist, including plugin-hosted project regressions. All remain uncompiled/unrun in UE here. [Inventory](SourceInventory.json). |
| Reflected include scan | 470 generated-header users scanned; B3 found | A source-layout check with one confirmed ordering violation; not UHT execution. |

The strongest damage and Echo findings received a second independent counter-review. The damage trace survives GAS list-lock considerations; the Echo report qualifies immediate versus scope-deferred cancellation. Save SP-01 was narrowed: `bBusy` prevents namespace changes, automatic identity observation suppresses nested refresh, and production uses an exact envelope class. The remaining defect is absence of operation-wide authorization checks, with native witnesses still required. These distinctions prevent a plausible hazard from being presented as an observed exploit.

No UHT/UBT, engine automation, Blueprint compilation, cook, packaged startup, real mission traversal, proprietary SDK, physical input/output, console performance or certification was performed. A source review cannot make those outcomes true.

## Corrective plan and acceptance

The [vertical-slice roadmap](Roadmap.md) identifies concrete files/classes, implementation, dependencies, risks, failure cases, tests and definition of done. The next three slices are:

1. **Deterministic compilation and clean runtime boundaries:** repair B1–B3, harden exposed platform queries and isolate reflected test fixtures. Build the real project in unity and non-unity with explicit target evidence.
2. **Combat transaction and action-lifecycle integrity:** ED-01/02/03/06/07 and PC-01/02/03. Accepted damage is conserved; one loaded round or pickup authorizes only its entitlement; canceled actions cannot continue or mutate replacements.
3. **Recoverable saves, encounters and travel:** SP-01/02/03/05 and C01–C04. Failed writes, lost participants, deferred promotion, unavailable users and accepted-but-failed travel all reach a safe, actionable state without false success.

The source can be called ready for engine qualification only after known compile hazards and P1 contracts are repaired or explicitly dispositioned with evidence. It can be called compiled only after UE5.7 succeeds. The engineering layer can be accepted for playable integration only after real producer-to-consumer regressions and M01/M02 recovery routes pass. This audit gives a concrete path to that standard; it does not issue a zero-defect certificate.

## Detailed reports

| Report | Finding IDs / scope |
|---|---|
| [BuildAndShippingAudit.md](BuildAndShippingAudit.md) | B1–B6: unity/UHT, Windows queries, runtime fixture isolation, diagnostics overhead |
| [PlayerCombatAudit.md](PlayerCombatAudit.md) | PC-01–08: melee, Echo lifecycle, ammo, Judgement, histories, effect overrides, recoil |
| [EnemyDefenseAudit.md](EnemyDefenseAudit.md) | ED-01–07: damage, pickups, Deflection, finisher outcomes, weak points, regen, drone cancellation |
| [CampaignWorldAudit.md](CampaignWorldAudit.md) | C01–C07: travel, encounters, save acknowledgement, companions, Mass residency/animation |
| [SavePlatformAudit.md](SavePlatformAudit.md) | SP-01–05: storage ownership, account recovery, travelling pause, cloud ordering, decode hardening |
| [UINarrativeAudit.md](UINarrativeAudit.md) | UI-01–08: subtitles, pause, captions, account settings, choices, records, relevance |
| [ValidationAndArchitecture.md](ValidationAndArchitecture.md) | V01–V04: executable gates, native validation coverage, performance evidence, bark residency; architectural repair principles |

These are open audit findings and verification gaps. No count of completed features should supersede their acceptance evidence.
