# Technical review route: implementation and qualification

This is a small integration step toward August TDD sections 3, 9, 11 and 17.2-17.3. It connects the existing playable protagonists to real authored objectives, checkpoints and native mission travel. It supplies no canonical dialogue, evidence revelation, faction outcome, cinematic receipt, companion proof or campaign-completion flag. The baseline planning assessment remains approximately 35% of the full TDD (30-40%), with engineering foundations approximately 70-80%; this increment does not re-score the full game.

## Current qualification

- Seven review assets are authored and saved. Both mission definitions validate, and copied GameModes compile with zero errors or warnings. Actual geometry and source Blueprint/CDO fingerprints and disk hashes were checked. Clean authoring attempt 04 saved all seven packages, removed one copied pack coordinator from each map, confirmed zero remaining encounter spawners/coordinators, and preserved source memory and disk. It used the normal renderer with RenderOffscreen.
- UE 5.7.4 Editor and Development Game builds passed. The final full suite passed **424/424: 398 clean and 26 with warnings**, including all five terminal regressions and the input-list parser regression. No matching tests were omitted. Evidence: `Saved/Validation/TDDAlignmentFinal/20260906-133710-2ca8c98d`; source fingerprints were unchanged during builds and automation.
- Rendered PIE at 13:41 local on 6 September confirmed the first Tarrik checkpoint through Enhanced Input dispatch, with the authored 0.35-second hold and tap interactions false. One journal fact was appended and the terminal reported "Objective complete. Checkpoint saved".
- Rendered PIE at 13:45 confirmed the second terminal caused actual native Tarrik-to-Selene travel. Selene was ready and alive, her checkpoint objective was active, and both journal facts survived with the original event GUID first. Evidence: `work/tdd-review-route-timeline-20260906-134141.json` and `work/tdd-review-route-timeline-20260906-134502.json`. This qualifies action dispatch, not a physical E key press.
- The corrected Development package passed with zero cook errors and 944 retained warnings in 126.50 seconds. Both packaged maps then loaded their intended GameModes and self-exited normally after 20 seconds, with zero startup errors or missing-package diagnostics. The old orphan-coordinator error is gone. The 30/38 headless map warnings remain retained. Container inventory and UFS records include all seven review packages, and the archived executable matches the validated Game build.
- A fresh offscreen Editor PIE run completed all three ordinary objectives through movement, focus and positive native hold countdowns, then loaded the saved Selene checkpoint using the public native slot API. It observed `LoadStarted`, `OnLoadCompleted(Success)`, ready/Idle Selene, and the exact three ordered journal GUIDs after reload. All inspected assets were unchanged. Evidence: `TDDReviewCommandLine-20260906-141253-c887f66e`. This is one successful integration round trip, not a soak, physical-key test or standalone visual playthrough.
- Shipping preflight admitted both mission definitions but failed with 85 errors across 91 effective production AlwaysCook roots and 9,622 examined dependencies. The errors comprise 4 prohibited legacy systems, 1 missing canonical opening, 32 missing packages, 32 invalid demo dialogue graphs and 16 unloadable old demo actor assets. This is a retained diagnostic, not a shipping pass.

## Authored structure

| Segment | Mission | Map | Required interactions |
| --- | --- | --- | --- |
| Tarrik | `M12_TarrikEntryReview` | `/Game/Maps/Development/TDDReview/L_TarrikReview` | `ReviewCheckpoint`, then `TravelToSelene` |
| Selene | `M12_SeleneEntryReview` | `/Game/Maps/Development/TDDReview/L_SeleneReview` | `ReviewCheckpoint` |

Under `/Game/Campaign/Development/TDDReview`, `DA_TarrikReview` and `DA_SeleneReview` contain the missions, `BP_TarrikReviewGameMode` and `BP_SeleneReviewGameMode` set their exact `InitialMission`, and `ST_ReviewObjectives` supplies localized text. Tarrik explicitly permits the Selene review as its successor. Neither mission enables alternate protagonists, resonance, canon writes or epilogue completion.

Only the copied review PlayerStarts face yaw 0, pitch 0 and roll 0 along the actual bridge. Their original locations remain unchanged. With entry `(0,750,102)`, the checkpoint is `(230,750,77)` and Tarrik's travel terminal is `(500,910,77)`, inside the fence lines at Y=480 and Y=1020. Walk around the first terminal through the clear lane near Y=600. The original combat-map entry orientation stays unchanged.

The prototype sign sits 850 cm forward and 250 cm left of entry, with world text size 12. Terminal labels use size 8. The sign says "Interact with the terminal"; Narrative's live prompt supplies the actual remapped binding and hold/tap behavior. Reports record requested and actual terminal/sign locations, rotations and the deliberate copied-map entry orientation.

## Reproduction

1. Install the documented WorkPC content baseline/overlay and run the framework, kit and project-UI setup in their established order. Compile `SovCampaignInteractionTerminal` and the editor-only `SovReviewRouteAuthoringLibrary` before authoring this route.
2. Save or explicitly discard dirty maps and stop PIE. Run `Scripts/Editor/setup_tdd_review_route.py` in the editor. Set `VELKORRAN_SETUP_OUTPUT` to an optional report directory. By default it writes `Saved/Validation/TDDReviewSetup/tdd-review-route-setup.json` inside the project and creates the report directory.
3. Inspect the report before play. The script saves only the two TDDReview folders and removes copied NPC spawners, their pack coordinators and prior script-owned decorations only in the exact review maps. It records removed names/classes/counts and checks zero remaining encounter owners. Full source-memory fingerprints and disk hashes protect inputs before every asset save. Exact current-world checks protect actor mutation and map saves; an initially dirty map is refused.
4. Source warmup is read-only. The script records disk hashes before loading defaults/components, then runs synchronous `unreal.collect_garbage()` before the strict memory baseline so detached load/compiler nodes are retired first. It retains a full pre-collection snapshot and exact diff. All subsequent memory differences still abort further saves. This path passed in fresh authoring attempt 04. Use normal rendering for authoring; a previous NullRHI attempt crashed inside engine actor creation. Check report freshness because an engine crash can bypass Python's final report write.
5. Fresh-load `/Game/Maps/Development/TDDReview/L_TarrikReview` after the final successful authoring run. On this PC, `outputs/Open-TDD-Review-in-Unreal.cmd` opens the full project with isolated `Saved/TDDReviewUser` saves. Do not qualify an older map still resident in memory. These quiet maps exercise mission/save/travel integration; they do not represent encounter pacing or completed Aurelion production content.
6. `Scripts/Cook-TDDReview.ps1` packages both explicit maps through the documented playtest profile. The default archive launcher is `F:\ProjectVelkorran\Saved\TDDReviewPlaytest\Windows\ProjectVelkorran.exe`. Preserve production cook settings and diagnostic failures; invocation-scoped MetaHuman authoring-data exclusions do not qualify the full campaign.

The native terminal retains Narrative's real focus/reach/hold path and the authored 0.35-second hold. After input completion and ordinary interaction callbacks, deferred work rechecks the same ready epoch, pawn/controller/ASC, component identity, range, visibility and objective ownership. Ordinary completion calls `USovCampaignStateComponent::CompleteBeat`. Checkpoints call `USovSaveSubsystem::WriteCheckpoint`; travel calls `ASovPlayerController::TravelToMission`, retaining verified origin storage, Narrative snapshot and recovery ownership. Retrying a failed boundary does not append the completed objective twice. Messages distinguish a saved checkpoint, requested travel and reached destination.

## Remaining live acceptance

Physical keyboard/controller interaction and a rendered standalone playthrough remain unqualified. Enhanced Input action delivery exercises the actual focus/hold/interaction path, but it does not establish hardware dispatch. The retained 30/38 packaged headless warnings need separate presentation review. A single successful checkpoint round trip does not establish the TDD's soak threshold or every failure/recovery path.

The first offscreen validation reached the correct restored state but missed its Python notification because its delegate wrapper was not retained. Installed Unreal documents that wrapper lifetime. The corrected test retained the GameInstance-owned delegate wrapper and required the actual success callback; native gameplay code and the acceptance gate were unchanged. Earlier authoring attempts and the graphics-free actor-creation crash are retained separately from the successful final authoring run.

## Delivery

This change includes the supplied TDD reference, the full-TDD baseline assessment, native runtime/editor changes, regressions, the portable authoring script and the two-map cook wrapper. The baseline is `9bfcf9da213d40c07c446d101e347ada8e69cd3c`.

`Content` is ignored by Git. The separate `ProjectVelkorran-TDDReview-Content-2026-09-06.zip` overlay contains exactly seven packages plus its README and manifest. The delivery manifest identifies the required source commit and every package SHA-256/size; CRC and individual member hashes are verified. It requires the existing full project, WorkPC content baseline and licensed dependencies. It is not a standalone project. The verification ZIP and user handoff accompany the overlay on the work PC.

## Regression scope and TDD limits

`ProjectVelkorran.Campaign.Terminal.*` covers real native tap dispatch, normal holding and early release, authored callback preservation, callback-deactivated terminal rejection, physical/special-proof admission, readiness/possession retirement and idempotent retry when the save subsystem is unavailable. That last case deliberately performs no disk write; it does not itself qualify write/readback faults. Existing save/travel regressions and the actual route tests provide separate evidence.

The 50-60-minute Aurelion slice, contrary-witness scene, Reformation/Eclipse roles, survivors/allies, same-map protagonist handoff with living companion proxies, joint Resonance, meaningful local decision, quiet aftermath, progression choice and all section 17.5 research/performance/soak gates remain separate work. This route must not be represented as their completion.
