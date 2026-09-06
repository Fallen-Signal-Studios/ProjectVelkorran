# M12–M13 proving route

Status: integration work order for the full canonical M12-M13 slice. The separate technical checkpoint/travel prototype is qualified in `TDDReviewRoute-2026-09-06.md`; the full-TDD planning estimate is in `TDDAlignment-2026-09-06.md`. That prototype does not complete the six segments below. Authority: Sovereign Call: Origins TDD v2.0, revised 14 August 2026, §§9.12, 13.4 and 17.2–17.5, together with the approved project deviations in `CampaignV2ChangeLog.md`.

The next content gate is the 50–60 minute lower Aurelion terminal excerpt from M12–M13. Use real campaign assets and a temporary review save with the curated late-game ability subset. The source fixtures are regression tests; they do not supply this route.

| Segment | TDD duration | Required authored integration | Evidence to retain |
|---|---|---|---|
| Tarrik entry | 10–12 min | Protect mixed survivors and break a disciplined formation after the Dominion/Reformation battle. Connect immediate, localized goals to the campaign owner. | Capture distinct Tarrik control feel, formation behavior, objective advance and checkpoint retry. |
| Selene entry | 10–12 min | Analyze terminal systems, evade sensors, sever the command network and complete a precision encounter. Gate revealed goals on acquired knowledge. | Capture sensor/command consequences, Selene control feel and absence of future objective spoilers. |
| Contrary-witness handoff | 5–7 min | Tier A/B scene and controlled protagonist switch. Use the existing transition owner and campaign state. | Before/after save records, correct pawn/control return, subtitle continuity and no previous-protagonist objective text. |
| Eclipse escalation | 15–18 min | Synchronized enemy behavior, corruption bands, the companion protagonist and one Resonance setup/payoff. | Capture resource/finisher outcomes, readable threat feedback, companion behavior and retry from the preceding checkpoint. |
| Local decision | 3–5 min | One trust/protection decision, mutually exclusive outcomes and an immediate escape consequence. Author success/failure rules before unusual objectives can fail. | Show both alternatives on separate runs, exactly one recorded outcome per run, visible consequence and replay-safe restore. |
| Quiet aftermath | 5–8 min | Tier C conversation reflects the decision. Tarrik and Selene voluntarily remain together, preserving canon. | Show each decision's aftermath consumer, complete readable dialogue, final state and canon review. |

## Connection points

- Author mission definitions, beats, string-table objective text and optional failure-rule text in `USovCampaignDefinition`. Register the complete real mission set with the campaign preflight, including its opening-mission requirements and successor closure. An M12/M13-only manifest does not satisfy the existing full-campaign validator.
- Drive objective lifecycle and choices through `USovCampaignStateComponent`. World interactions, combat completion and dialogue must commit to that owner. The HUD only reads actionable objectives; it does not advance story state.
- Mount the native frontend through the existing local player controller. Verify the objective panel at 80% and 100% safe zones, minimum and maximum UI scale, high contrast, and with **Show objective text** disabled. Check that no hidden or future task leaks through overflow text. Use **Accessibility > Review current objectives** to read every current goal and complete failure rule when the compact panel has insufficient room.
- Use the existing mission travel/save recovery path across map boundaries. Exercise load, handoff, travel failure and retry at every segment boundary. Restore should preserve committed choices, resource bases, evidence and relationship memory without duplicating outcomes.
- Supply dynamic asset roots explicitly when they cannot be reached through static asset dependencies. The commandlet does not prove Blueprint execution, world actors, World Partition cells, runtime string loads or map interaction wiring.

## Evidence gates

1. Run the build/native/preflight workflow in `CampaignQualification-2026-09-06.md` against the exact source and restored content. Retain its source/content manifests, complete native report and stage logs. Fix actual compiler or engine failures before interpreting test counts as qualification.
2. Produce a Development package using the real map list. Launch it and play the six segments in order on each target desktop platform. Record map/mission identities, durations, checkpoints and the decision result alongside the package identity. A successful cook does not prove the route is playable.
3. Collect §17.5 acceptance evidence: at least 80% protagonist identification, the approved kit satisfaction threshold with no gap above one point, faction recognition by a majority, at least 70% Echo understanding, at least 80% corruption-remedy understanding, remembered/observed decision consequence, zero surviving canon contradictions, checkpoint/reload success above 99.5%, approved worst-case performance and measured production cost. These require actual playtests, soak, captures and review; native unit tests cannot replace them.

The engineering alignment estimate remains approximately **84%** pending these gates. This follow-up improves objective presentation and makes engine qualification reproducible. It does not establish a production-quality vertical slice or justify an engine-confidence increase by itself.

## Source verification for this follow-up

- 63 Python tests passed, including 24 qualification-runner regressions.
- All 42 portable C++ suites passed with the repository runner.
- 341 unique native test registrations are present, including ten new registrations in this follow-up. Native Unreal tests, UHT/UBT builds and package/playthrough checks were not executed here.
- `git diff --check` passed. The final source manifest contains 1,370 inputs with SHA-256 fingerprint `75f25d96858591f6be98ba921e8c1090b7c9685b80a96371e86f0146451518af`.
- The approximately 84% estimate applies to the proposed branch including PR #38 and this follow-up. The base PR is still open; this evidence does not describe merged-main runtime qualification.
