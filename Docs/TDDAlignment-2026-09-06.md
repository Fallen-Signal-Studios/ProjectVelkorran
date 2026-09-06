# Project Velkorran — August TDD alignment

**Current estimate: approximately 35% alignment with the full TDD (roughly 30–40%).** The engineering foundation is approximately **70–80% aligned**. The larger number describes the underlying systems; it does not describe completed campaign content or a production-quality vertical slice.

Reference: creator-supplied **Sovereign Call: Origins TDD v2.0, revised 14 August 2026**, SHA-256 `3d13b55a90ce67204b77b43a51a6ff9f8d83cdd19491d9149fa1913f617807b2`. Baseline reviewed: main `9bfcf9da213d40c07c446d101e347ada8e69cd3c`, following [PR #41](https://github.com/Fallen-Signal-Studios/ProjectVelkorran/pull/41). The supplied document is a design reference; its internal approval/production instructions do not replace the creator's current request.

## How the estimate works

Each domain receives a judgmental range based on the implemented rules, actual authored assets and verified behavior. These domain weights favor the core combat and authored campaign experience. They are explicit planning assumptions, not prescribed TDD weights. Native regression fixtures receive engineering credit, not credit for authored missions or completed playthroughs. Requirement overlap is assessed by purpose: for example, a save contract and an authored mission's checkpoint integration are separate gates.

| Domain | Weight | Current alignment range |
|---|---:|---:|
| Combat, protagonists, movement and AI | 30% | 39.8–54.8% |
| Campaign, narrative, progression, world and companions | 35% | 18.7–26.75% |
| HUD, accessibility and localization | 10% | 30–45% |
| Art, animation, cinematics and audio | 10% | 15–30% |
| Runtime architecture and save contracts | 8% | 50–65% |
| Asset pipeline, platforms, production and QA | 7% | 15–30% |

The arithmetic gives 28.0–40.6% with a midpoint of 34.3%; the useful communication is **about 35%, roughly 30–40%**. These are planning ranges, not statistical confidence intervals, and they do not predict remaining calendar time. The separate engineering estimate summarizes system readiness; it is not added to the domain total. Earlier repository estimates near 84% described source engineering from a different scope, not full-TDD delivery.

## What is already real

- UE 5.7.4 Editor and Development game targets build. The retained baseline has **414/414 native tests passing**, with 25 passing tests reporting warnings.
- Both project-owned protagonist maps load; signature weapons, revised Echo kits, native vitals, ordinary hostile attacks, death and respawn have live evidence. The ten mapped Echo activation checks each deliberately began at 100 Echo; they do not prove every payload in ordinary combat.
- Real Cinderline fire damaged and killed a hostile. A natural Selene perfect deflection awarded the expected +10 Echo without injected damage or resources.
- Native mission/objective/consequence/evidence, protagonist ownership, save integrity, companion/co-action, encounter and accessibility foundations exist.
- A desktop Development package was cooked and both combat maps passed headless boot checks. The standalone rendered playthrough remains unqualified; package production is not gameplay acceptance.

## What most limits alignment

- **The authored campaign is largely unconnected.** At this baseline both combat GameModes have `InitialMission=None`. A read-only Unreal Asset Registry audit found 8,254 `/Game` assets but no registered native campaign, Technique, evidence or encounter data assets. The six registered Level Sequences are marketplace blood/scifi examples. These are scoped content findings, not a claim that every asset is missing or unusable.
- **The 50–60 minute lower-Aurelion slice is not built.** Its mixed-survivor formation encounter, Selene terminal/sensor route, contrary-witness scene, Eclipse escalation, companion/Resonance, local decision and quiet consequence scene need authored playable integration.
- **Combat breadth and presentation remain partial.** Current primary melee uses legacy combo assets; contact, the native attack-definition route, finisher input/grants, bespoke Verity choreography and broad faction/boss/corruption cases remain incomplete or unqualified. Intermittent hostile AI startup has retained evidence and needs causal diagnosis.
- **The campaign UI still carries template behavior.** Generic inventory and character-creation affordances conflict with the TDD's fixed-equipment campaign. Ability readiness, full contextual HUD behavior and device/accessibility/localization coverage need work.
- **Cook exclusion is incomplete.** Existing content manifests include XP, currency and multiplayer-menu assets; current checks miss some implicit AlwaysCook roots and named legacy assets. Core inventory machinery is still required internally by signature weapons, so dependency work must be precise.
- **Production gates remain open.** Representative final-quality assets, authored cinematics/VO, target-console frame-time/memory captures, full checkpoint soak, recruited accessibility/player tests and measured production cost are not established by native tests.

## Approved differences preserved

The later creator decisions recorded in `Docs/CampaignV2ChangeLog.md` take precedence where they differ from prototype tables: revised Tarrik/Selene Echo rosters, delayed player Health recharge without revival, finite Cinderline magazine/reserve, narrowly scoped short-lived ammo/Echo sustain drops, and deterministic variation/range for ordinary Cinderline hits. These are not scored as defects. They do not reopen classes, randomized gear, vendors, crafting, co-op or a loot economy.

## Implementation selected from this comparison

1. Author two clearly labeled technical review segments using real mission definitions, immediate objectives and ordinary world interactions. Exercise checkpointed Tarrik-to-Selene mission travel without forging canon facts or bypassing companion/handoff proof.
2. Move Echo to the lower right, make stamina visible while changing or below full, and retain damaged-health visibility. The complete six-second combat fade requires a reliable shared threat/participation signal and remains a separate gap.
3. Make campaign validation report forbidden legacy content from both explicit mission dependencies and effective AlwaysCook roots. Retain required Narrative framework and signature-weapon dependencies.

This report records the baseline estimate. The implementation handoff will identify what was actually built and verified afterward. A small technical review route is useful progress toward the slice; it does not justify a several-point increase in whole-game completion.

## Supporting review

The companion JSON and CSV preserve the weights and arithmetic. Detailed combat, campaign and platform/presentation audit matrices, plus the Unreal registry inventory, are retained with their SHA-256 identities in the JSON evidence list. All three specialist reviews read their assigned TDD sections and distinguish source, authored assets, live evidence and missing work.
