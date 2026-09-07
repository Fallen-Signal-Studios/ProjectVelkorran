# Aurelion morning handoff

This branch prepares the first Aurelion encounter and the canonical M12/M13 contracts for work-PC authoring. It is based on main `8b1551e862018ec3fc8e4ed6ff90e2ad51de6c04`, the merged PR #42 review route. The reference is the August 14 TDD v2 and manuscript chapters 23–26. Chapter 23 supplies approach context; chapters 24–26 supply the M12/M13 events. See [the adaptation contract](AurelionAdaptationContract-2026-09-06.md) for the exact chapter mapping and fixed outcomes.

## What is ready in source

| Change | Result | Remaining work-PC gate |
|---|---|---|
| Native preparation and M12/M13 mission definitions | Ordered objectives, independent assent, declared scene/evidence dependencies, controlled handoffs and protected historical facts. Ordinary encounter victory cannot award canon scenes or evidence. | Compile; bind real profiles, maps, scenes and evidence. Canonical scaffolds intentionally fail validation until these exist. |
| Encounter-to-objective bridge | Entry capture and durable checkpoint precede combat. Current registered victory completes its objective once; generic terminals cannot bypass it. Late or invalidated results require entry recovery. | Exercise actual combat, save/load, reentrant callbacks and retry in Unreal. |
| Protected survivors | Survivors share the director's entry snapshot and retry ownership. Death or destruction fails an active attempt. | Stage safe entry, actual factions, navigation, a coordinated hostile roster and both survivor roles. |
| Placed NPC definition fallback | Existing project role classes can initialize an explicitly assigned Narrative definition. Existing spawn/restore definitions retain ownership. | Assign the correct definition to each placed role and confirm assets, abilities and AI initialize. |
| Editor setup and manifest | Read-only inventory first, explicit roster and placement, source preservation, repeat-run protection, separate preparation output and incomplete canonical scaffolds. | Supply real content and coordinates, author the output and reload it. |
| AI startup observation | Bounded opt-in timeline of native perception callbacks, player faction publication, ASC readiness and goals. | Record stalled and successful cold starts; this is instrumentation, not a causal fix. |

The first authored target is a three-beat Tarrik corridor: arrival, protect two survivors while defeating the required enemies, then secure the route. It is a technical foothold for the 50–60-minute slice. Selene's sensor route, final formation mechanics, full companion/Resonance integration, cinematics, local-choice consequences and aftermath remain to be authored.

## Start here on the work PC

Close Unreal. Preserve any local source edits before switching branches, and retain the existing content overlay. From the repository root:

```powershell
git status --short
git fetch origin codex/aurelion-slice-preparation-20260907
git switch --track origin/codex/aurelion-slice-preparation-20260907
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -BuildGame -NonUnity
```

Use the actual engine installation path. If the local branch already exists, switch to it and update normally without discarding local changes. Capture the exact checked-out revision and validation artifacts. Do not reuse the earlier 424-pass report to qualify this branch. Native registrations are discovered from the current source by the validation scripts.

After a successful build and native run, follow [AurelionAuthoringSetup.md](AurelionAuthoringSetup.md). Start in `inventory` mode; the checked-in example deliberately cannot pass apply validation. Create a dedicated source template, assign actual NPC definitions, place at least one required hostile and two protected survivors, then supply every participant and position explicitly. Use `apply` only when that template is ready. The preparation-only generated manifest excludes incomplete M12/M13 assets.

Before expanding the map, qualify these boundaries:

1. Fresh load, player readiness, arrival interaction and entry checkpoint before hostilities.
2. Protected-survivor loss fails; surviving-player retry and player-death recovery restore the correct entry.
3. Required enemy deaths with survivors alive complete the hold exactly once; the secure terminal cannot skip it.
4. Secure-route checkpoint saves and reloads correctly; a stale victory never becomes a new player's success.
5. Packaged keyboard/controller play and a selected-route cook/preflight with retained reports.

Use [the AI startup procedure](AurelionAIStartupTrace-2026-09-06.md) for the existing intermittent stall. Arm `sov.AIStartupTrace 1` before creating a fresh PIE world. Keep the first stalled run untouched and extract its complete log for comparison with a successful cold run.

`Content/` and Unreal binaries are excluded from this source repository. Back up the authored output and reports through the existing work-PC content process. Pulling the branch alone cannot recreate or qualify binary assets.

## Validation performed here

- Python host suite: **98/98 passed**, including 12 authoring-configuration and 8 trace-extractor tests.
- Portable C++ policy suite: **42/42 passed**, compiled with warnings as errors and undefined-behavior sanitization by the repository harness.
- Reflected test-module isolation source check: passed. The `ProjectVelkorran` native selection contains **437** tests, including **13 new** tests. This is an inventory, not an Unreal pass count.
- Whitespace/error checks: `git diff --check` passed.
- UE 5.7 UHT/UBT, native automation, Editor execution, cooking and gameplay: **not run in this environment**. New native tests and asset-authoring calls require the work PC.

The branch should remain a draft until engine validation and the first encounter acceptance checks are recorded. Main remains the prior validated baseline.

## Alignment and next development priorities

Retain the latest reviewed estimate of **about 35% alignment with the full TDD (30–40% planning range)** and **70–80% for engineering foundations**, as explained in [TDDAlignment-2026-09-06.md](TDDAlignment-2026-09-06.md). These are judgmental scope estimates. This source increment improves preparation but does not establish a measurable whole-game percentage increase. Previous figures near 84% covered a different engineering scope.

After qualifying the first encounter, prioritize Selene's sensor/command-network segment and controlled protagonist handoff; then companion positioning and Eclipse escalation; then real independent-assent/Fifth-Witness scenes, evidence custody and the quiet consequence scene. Keep containment closed and release withheld throughout this material. Final slice acceptance still needs the TDD's player research, representative performance, production-cost and checkpoint-soak evidence.
