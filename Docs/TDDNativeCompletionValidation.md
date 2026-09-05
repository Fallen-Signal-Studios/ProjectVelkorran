# Native completion validation

This document records validation for the source pass following `fe31c66e69226d73d92a3dea38ed6a6fef3aa1dd`. It does not reuse the preceding branch's test execution as proof of these changes.

## Executed here

**Final combined result: PASS, 27 portable suites, process exit 0.** No compiler warning promoted to an error and no undefined-behavior sanitizer failure occurred. This includes production evidence/graph, save-admission, resource, Resonance/contribution, world movement, carry/rescue, field recovery, accessibility, exertion, melee and cue policies.

The final portable execution log is [TDDNativeCompletionPortableTests.txt](TDDNativeCompletionPortableTests.txt). The runner compiles the production-used policy headers with C++17, `-Wall -Wextra -Werror -pedantic`, undefined-behavior sanitizer and nonrecovering sanitizer failures. These are actual compiled C++ tests of the shared policy/math, not a mocked Unreal build.

Subsystem documents retain intermediate test counts from their individual review checkpoints. The combined log linked above is the final execution record. New test registrations were normalized under `ProjectVelkorran` so the provided runner selects field recovery, settings, targeting and coordination as well as the existing campaign and new world suites.

```sh
python3 Scripts/Test-NativePolicies.py
git diff --check
```

The [final source consistency log](TDDNativeCompletionSourceChecks.txt) checks 254 changed/new C++ files, 94 reflected-header include orders, repository-owned includes, module dependency direction, all 144 native automation registrations and patch whitespace. All checks passed. The [environment preflight](TDDNativeCompletionEnvironment.txt) records the unavailable engine tools/content. Those checks catch specific mechanical mistakes; they cannot validate UHT, Unreal APIs, module linkage, actor physics, UObject ownership, Blueprint migration or packaged content.

## Unreal gate was blocked

The [environment preflight](TDDNativeCompletionEnvironment.txt) identifies UE 5.7 in `ProjectVelkorran.uproject` but has no UnrealEditor, UnrealEditor-Cmd, UnrealBuildTool, .NET or PowerShell executable. The common local engine installation roots are absent. There are zero `.uasset` and zero `.umap` files in this checkout. Therefore no Unreal compilation, UHT/UBT, engine automation, Blueprint compilation, commandlet execution, cook, packaged playthrough or performance run is reported as passed.

The repository's real Windows runner remains the next gate. Its default filter now includes **all `ProjectVelkorran` tests**, including the new World tests outside the older Campaign prefix:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7'
```

It builds the actual editor target, starts a fresh automation process/report, and rejects missing reports, no matching tests, failed/incomplete tests, timeout or nonzero exit. `-BuildOnly` is available when validating source before restoring content. `-SkipBuild` explicitly leaves freshness of binaries to the operator. A successful policy runner does not satisfy this gate.

Run native campaign validation against the **complete shipping mission manifest** and explicitly include assets loaded only by dynamic strings:

```text
UnrealEditor-Cmd ProjectVelkorran.uproject -run=SovValidateCampaign -Missions=<all mission object paths> -AdditionalAssets=<dynamic-only asset paths> -ShippingValidation
```

The commandlet validates declared native contracts and dependency closure. It cannot prove arbitrary Blueprint event semantics, correct cinematic causality, caption readability or asset playability.

## Required engine regression matrix

| Area | Required behavioral evidence |
|---|---|
| Input/exertion/melee | Both heroes at 30/60/120 fps; affordability, delayed regeneration, combat/traversal sprint, charged release, explicit buffer windows, pressed/released toggle and same-ASC reentry, wall/capsule sweeps, stale node task and cancel cleanup. |
| Damage/finisher/projectile | Weighted mixed channels, global and channel immunity, control-only results, exact resource costs, target claims, interruption protection, reentrant ability end, absorbed/reflected Drone rockets, original expiry and no duplicate explosion. |
| Recovery | Canonical/environmental exclusions, reachable living companion, once-per-attempt rescue, no unreachable rescue, input/effect ownership after callbacks, safe respawn floor/capsule, required companion failure, fallback and 100 death/retry cycles. |
| Save | Actual serialization, configured Narrative subclass, write/readback denial and torn bank, corrupt preservation, schema/account/header/journal mismatch, required class missing, optional record omitted, restore phases and readiness barrier. |
| Convergence | Both lead configurations; one uncontrolled protagonist; copied unlocked kit and contribution bound; four Resonance setup/payoff loops; stale offer, failed spawn/init/commit and rollback; both hero snapshots across full load and encounter retry. |
| Encounter/corruption | A/B promotion and role quotas, existing attack leases, companion/objective targeting, offscreen cue acknowledgement and lead time, bounded pressure relief, wave reset, typed corruption producer/remedy/band/clock and save restore. |
| Narrative | Independent evidence provenance, critical evidence and cross-mission consequence dominance, per-hero knowledge, graph exit/fallback/speaker validation, one cue at a time, same-instance pause/resume, critical preemption/save and callback-driven teardown. |
| World | Repeated traversal starts and low-fps corner crossings, obstruction, safe destination, moving lift/rider sweeps, power/damage, streaming timeout, stable endpoint save and failed critical checkpoint continuation. |
| Field recovery and augments | Saved charge counts per protagonist, exact/early consumption, actual damage/montage interruption, full-health/no-charge rejection, physical supply refill; purchased versus selected perks, no stacked alternatives, unchanged core spec, replay/respec ownership and failed rollback recovery. |
| Cinematics | Same-generation tag/viewer ownership across pause/resume/stop/destroy, missing/late participants, bounded setup/blend failure, native preload/equipment/exit contracts, skip admission, exactly-once journal commitment and failed postcondition recovery. |
| Settings/platform | Persistence and decomposed difficulty; actual native assist consumers; invalid target/LOS/owner loss; comfort settings; diagnostics privacy; real devices and platform account/storage/accessibility behavior. |

Subsystem documents list the exact Unreal automation groups added. They are test sources pending execution, not success records. Actual editor content integration and perceptual review remain with the complete project; any native defect those tests reveal remains engineering work.
