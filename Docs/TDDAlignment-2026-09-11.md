# Project Velkorran — 11 September 2026 TDD alignment

**Current estimate: approximately 37% alignment with the full TDD (roughly 31–43%).** The engineering
foundation is approximately **75–85% aligned**. As in the previous report, the larger number describes
underlying systems; it does not describe completed campaign content or a production-quality vertical slice.

Reference: creator-supplied **Sovereign Call: Origins TDD v2.0, revised 14 August 2026**, SHA-256
`3d13b55a90ce67204b77b43a51a6ff9f8d83cdd19491d9149fa1913f617807b2` — verified by hash against the document
supplied 11 September 2026. Baseline reviewed: main `ede889e1`. Previous baseline:
`9bfcf9da213d40c07c446d101e347ada8e69cd3c` at approximately 35%.

## Scope of this review

This is a **delta update**, not an independent re-audit. The 2026-09-06 report was backed by three
specialist reviews reading assigned TDD sections plus a read-only Unreal Asset Registry audit. This
report re-uses that report's domain weights and reasoning, and adjusts them against evidence gathered
from repository state: commit history since the baseline, native test results, source tree structure and
authored-asset inventory. **No new asset registry audit was run and no playable verification was
performed.** Where this report differs from the previous one, the previous one has the stronger evidence
base for domain internals; this one has the more current repository facts.

## What changed since the 35% baseline

- 11 commits, 196 source files changed, +24,798 / -946 lines.
- Native tests **414/414 → 590/590 passing** (+176), 80 reporting warnings (all navmesh-environmental).
- `Content/Aurelion` now holds **1,192 assets and 3 maps**, organised into Art, Characters, Cinematics,
  Data, Enemies, Environment, Evidence, Framework, Maps, Materials, Meshes, UI and VFX.
- Aurelion mission contracts, protected encounter bridge, graybox level and layout alignment landed
  (PRs #42, #43 and follow-on commits).
- TDD review route, contextual HUD and content preflight checks landed — these were items 1–3 of the
  previous report's selected implementation.
- Build integrity restored: four unity-build symbol collisions in `ProjectVelkorranEditor` fixed
  (`ede889e1`). The Editor module had not compiled under unity build prior to this.

## Updated domain estimate

| Domain | Weight | 2026-09-06 | 2026-09-11 | Basis for change |
|---|---:|---:|---:|---|
| Combat, protagonists, movement and AI | 30% | 39.8–54.8% | 42–57% | Threat memory, attack integration, +176 tests |
| Campaign, narrative, progression, world, companions | 35% | 18.7–26.75% | 22–30% | Aurelion contracts, graybox, Evidence/Data authoring |
| HUD, accessibility and localization | 10% | 30–45% | 33–48% | Contextual HUD landed |
| Art, animation, cinematics and audio | 10% | 15–30% | 15–30% | **Unchanged** — no art/VO/cinematic work |
| Runtime architecture and save contracts | 8% | 50–65% | 55–70% | Save/world coverage, build integrity |
| Asset pipeline, platforms, production and QA | 7% | 15–30% | 18–33% | Content preflight, validation harness |

Weighted arithmetic gives **30.8–43.3%, midpoint 37.0%**. These are planning ranges, not statistical
confidence intervals, and they do not predict remaining calendar time.

The movement from ~35% to ~37% is deliberately modest. Most of the work since the baseline was
engineering and slice *preparation*. The previous report's caution still applies: a technical review
route is useful progress toward the slice, but it does not justify a several-point increase in whole-game
completion.

## What 90% requires

Appendix F (Campaign Definition of Done) sets the bar: all missions, hubs, prologue and epilogue running
clean-boot-to-credits; every canon gate matching the approved adaptation; all platforms meeting
performance, stability, save, suspend/resume and certification requirements; all accessibility settings
working first-boot-to-credits; every spoken line with approved subtitle/caption and localization status;
legal, credits, privacy and support materials complete.

Closing ~53 percentage points against that bar is a **multi-discipline production programme**, not an
engineering backlog. The table below ranks blockers by the points they gate, not by effort.

| # | Blocker | Domain weight | Points gated | Can code close it? |
|---|---|---:|---:|---|
| 1 | Authored campaign: missions, hubs, prologue, epilogue | 35% | ~22 | **No** — level design, narrative authoring, scripting |
| 2 | Combat breadth and presentation: melee on legacy combo assets, native attack-definition route, finisher input/grants, Verity choreography, faction/boss/corruption cases | 30% | ~12 | **Partly** — systems yes, animation/choreography no |
| 3 | Art, animation, cinematics, audio and VO | 10% | ~7 | **No** — artists, animators, composers, voice actors |
| 4 | HUD, accessibility and localization coverage | 10% | ~5 | **Mostly yes** — plus recruited accessibility testing |
| 5 | Production and QA gates: console frame-time/memory captures, certification, checkpoint soak, recruited player testing | 7% | ~4.5 | **No** — devkits, human testers, business deliverables |
| 6 | Runtime architecture remainder | 8% | ~2.5 | **Yes** |

**Roughly 8 of the ~53 points are reachable by code alone.** The other ~45 require authored creative
content, physical target hardware, recruited human testers, or legal/business deliverables. No amount of
engineering effort substitutes for them.

## Open engineering items

These are code- or data-level and remain unresolved:

1. **`InitialMission` is unset on the campaign GameModes.** `ASovCampaignGameMode::InitialMission` is a
   `TObjectPtr<USovCampaignDefinition>` set per GameMode asset, not in `Config/*.ini`. The previous
   report found both combat GameModes at `None`; nothing in this repository state contradicts that. Until
   it is bound, the authored campaign does not boot into a mission. Binding it is mechanical, but *which*
   mission to bind is a design decision, and the asset edit requires the Unreal Editor or a Python
   commandlet — it cannot be done by hand-editing `.uasset` binaries.
2. **Cook exclusion remains incomplete.** Content manifests still include XP, currency and
   multiplayer-menu assets; checks miss some implicit AlwaysCook roots and named legacy assets. Core
   inventory machinery is still required internally by signature weapons, so dependency work must be
   precise. TDD Appendix F forbids obsolete class, loot, XP, rarity, vendor, crafting, morality and
   approval systems in the campaign cook.
3. **Campaign UI still carries template behavior.** Generic inventory and character-creation affordances
   conflict with the TDD's fixed-equipment campaign.
4. **Intermittent hostile AI startup** has retained evidence and still needs causal diagnosis.
5. **Packaged Game target is unverified in this run.** The 11 September validation ran editor build plus
   automation only (`gameBuild: "not run"`, `packagedBuild: "not run"`). Editor-green does not prove a
   shipping build.
6. **`ProjectVelkorranTests` sets `bUseUnity = false`**, which masks this class of collision in the test
   module. The Editor module's collisions were fixed at the root rather than papered over; the Tests
   module workaround remains and could hide future duplicates.

## Approved differences preserved

The creator decisions recorded in `Docs/CampaignV2ChangeLog.md` continue to take precedence where they
differ from prototype tables. They are not scored as defects and are unchanged by this review.

## Verification evidence for this report

- Build: `Validate-Unreal.ps1` against UE 5.7.4 (CL 51494982), exit 0.
- Automation: 590 matching tests, 0 failed, 0 notRun, 12.4s, `sourceIntegrity: unchanged during build and
  automation`.
- Run directory: `Saved/Validation/20260911-122709-d0503522`.
- TDD hash verified: `3d13b55a90ce…07807b2` matches the reference in `Docs/TDDAlignment-2026-09-06.md`.
