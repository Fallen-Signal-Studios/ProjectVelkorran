# Aurelion morning handoff

This branch now aligns the canonical Aurelion source contracts with the **7 September level layout plan**, extending the earlier PR #43 preparation package. The authority remains the August 14 TDD, Origins chapters 23–26 and the creator's approved layout adaptation. The work-PC target is the shared **Z05–Z12 skeleton first**, followed by entrances Z00–Z04. The earlier three-beat Tarrik preparation map remains a separate technical harness.

Start with [the full layout contract](AurelionLayoutContract-2026-09-07.md) and [machine-readable manifest](../Scripts/Manifests/AurelionFullLayout-2026-09-07.json). They record 13 zones, four physical encounters, all 13 checkpoint boundaries, the 50-minute main route and eight optional minutes. The geometry and timing values are adopted authoring targets, not measured results.

## What the source now supports

| Change | Source behavior | Work-PC content and execution gate |
|---|---|---|
| Revised M12/M13 contracts | Separate approaches, delayed shared protagonist companion activation, cage destruction, local priority before E4, combat before quarantine, independent assent, unavoidable grammar propagation and separate departures. | Compile; bind actual profiles, maps, scenes, evidence and handoff anchors. Inspect old canonical scaffold assets for serialized-contract drift. |
| Two E2 relay receivers | Normal nearby held interactions provide two distinct native disable receipts. Hostile clearance alone cannot stand in for receiver proof. | Place both `ASovCampaignRelayReceiver` actors and bind `RequiredReceivers`; prove close interaction remains reachable without ammunition. |
| E4 native phases | Exact link-sever proof leads to a frozen handoff boundary; the same living elite transfers to Tarrik's phase. Thermal Fracture needs actual frost/heat/payoff evidence before conventional victory. | Bind the elite, source links, both phase objectives, partner anchors and presentation. Test interruptions, retry, handoff, and the verified phase-B checkpoint. |
| Local priority and support | One acknowledged WestStretchers/EastWalkers choice; CP4b before choice and CP5 after acknowledgment. West cache and early east shutter derive from the same saved outcome. | Place the two priority terminals and support barriers. Author cache pickups, survivor staging and matching aftermath lines. No duplicate consumable grant comes from support code. |
| Remaining checkpoint thresholds | `ASovAurelionCheckpoint` captures CP0, CP2, CP3, CP6–CP9 only when native mission progress matches. It cannot complete an objective. | Place thresholds after streaming and safe staging. Arena entry and priority checkpoints keep their existing transaction owners; do not duplicate them. |
| Layout validation | Connected topology, wave/cap arithmetic, clock accounting, checkpoint and native-name references, exclusive support and shared-first build order. | This is a host authoring-contract check. Actual waves, room geometry, actors, AI and gameplay still need content and engine qualification. |
| Existing preparation and AI diagnostics | Source-preserving technical setup, placed NPC definition fallback, protected-participant recovery and bounded startup trace remain available. | Use the prep map only as a separate harness. Record both successful and stalled cold AI starts; instrumentation does not establish a causal fix. |

**Chronology is deliberate:** meeting and carrier rescue → shared breach/rescue → both cage destructions and isolated threat exchange → local priority → two-phase E4 → survivor exit/quarantine → recognition → independent assent/Witness/consequence → voluntary conversation, evidence, pact and separate departures. No combat moves into the Fifth chamber or aftermath.

## Start here on the work PC

Close Unreal. Preserve local source edits before switching branches and keep the existing content overlay. From the repository root:

```powershell
git status --short
git fetch origin codex/aurelion-slice-preparation-20260907
git switch codex/aurelion-slice-preparation-20260907
python Scripts/Validate-AurelionLayout.py --check-native-bindings
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -BuildGame -NonUnity
```

Use the actual engine installation path. For a checkout without that local branch, create a tracking branch from the fetched remote. Update an existing branch normally without discarding local work. Retain the exact revision and fresh reports. The earlier 424-pass or PR #43 host results do not qualify this revision.

After native compilation, inspect canonical M12/M13 assets created from the earlier preparation constructors. Their old serialized beat order may correctly fail validation now. Back up and explicitly reconcile or recreate only those owned canonical scaffolds. Preserve real scene/evidence assignments and keep the unchanged technical preparation assets. Running the old setup's stamp check is not an asset migration.

## Author and qualify in this order

1. Block **Z05–Z12 first**: collision, gates, phase/scene entry and exit locations, companion anchors, streaming and every checkpoint. Use temporary content to prove each boundary reaches the next.
2. Add the two independent entrances Z00–Z04. Keep them physically separate until Z05 and preserve the isolated Tarrik-to-Selene perspective cut.
3. Tune E1/E2 using production movement and camera. E1 commits six drones with four active at most. E2 has six total and two close-disable receivers. Start with zero Echo and low ammunition; do not manufacture extra enemies on sensor failure.
4. Author E3's seven committed enemies with six active at most, the trapped-marine rescue and Lyric grounding. Then both Z07 cage destructions, the isolated threat bridge and the acknowledged local priority.
5. Exercise both E4 phases in the same five-hostile arena. Preserve the live elite, phase state and partner anchors through the handoff. Test staggered partners and missed Thermal Fracture windows, then conventional exposed-core completion.
6. Finish the core and aftermath: independent assent, historical Witness, unavoidable synchronization, boundary closed, release withheld, Crownmark Five integrated, Lyric alive/not cured, reciprocal evidence and separate destinations.
7. Cook and play the route with keyboard and controller. Test every checkpoint, both support outcomes, scene skips, evidence custody and departure. Retain timing, performance, recovery and player-research results.

For a separate native recovery/input harness, follow [AurelionAuthoringSetup.md](AurelionAuthoringSetup.md). Its `inventory`/`apply` workflow remains unchanged; it does not build canonical E1–E4 or the full map. Existing Hound/Handler assets may be useful proxies but do not qualify the prescribed Aurelion roles merely by renaming them.

Use [the AI startup procedure](AurelionAIStartupTrace-2026-09-06.md) for the unresolved intermittent stall. Arm `sov.AIStartupTrace 1` before a fresh PIE world. Preserve the first stalled run and compare its complete timeline with a successful cold start.

`Content/` and Unreal binaries remain outside this source repository. Preserve authored assets and reports through the existing work-PC content process. A source pull cannot supply those binaries.

## Validation status

The full host suite passes **119 tests**, including **21 layout boundary tests** covering topology, budgets, conjunctive wave gates, optional-time accounting, native-name drift, checkpoint ordering and mutually exclusive support. All **42 portable C++ policy suites** pass. These checks do not execute Unreal.

This layout amendment adds **18 Unreal test registrations** covering receivers, priority/checkpoint boundaries, companion staging, E4 and shared Witness observation. The complete source-discovered `ProjectVelkorran` selection now contains **455 tests** (31 additions across PR #43). Test-module isolation and whitespace checks pass. These native registrations are authored test coverage, not a passing Unreal report.

UE 5.7 UHT/UBT, native automation, Editor asset authoring, cooking, actual playthroughs and platform performance are **not run in this environment**. Keep the branch reviewable until the work-PC evidence qualifies it. Previous engineering/full-TDD estimates are not revised upward solely because this source preparation exists.

Final slice acceptance still requires the TDD's player-understanding thresholds, checkpoint/reload success **above 99.5%** in soak, representative performance, measured production cost and zero surviving canon contradictions. See [the adaptation contract](AurelionAdaptationContract-2026-09-06.md) for fixed story outcomes.
