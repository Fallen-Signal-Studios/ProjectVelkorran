# Aurelion workstation implementation — 13 September 2026

## Baseline

- Fresh `git fetch origin` succeeded. Local `main` and `origin/main` both point to `9bfb44e1`; ahead/behind is 0/0.
- The active checkout is `engineering/away-pass-20260911` at `0f7fb6e9`, matching its remote branch and containing 27 commits beyond main. Current local testing uses this newer engineering baseline; main was not changed.
- Pre-existing modifications: `Content/Aurelion/Maps/L_Aurelion_M12.umap`, `ProjectVelkorran.uproject` (Aura enabled), untracked `.claude/` and `Content/Cues/`. These have been preserved. Source, plugin source and Config have no working-tree changes.
- Publishing was initially blocked by automatic approval review. Following explicit user approval, both preservation branches were pushed successfully to origin and their full remote commit IDs were verified: codex/aurelion-workpc-slice-20260907 at bf97d25d57d2c139cdf055042aab71161e36e270 and codex/preserve-workpc-20260906 at c1aaa147082d7591cb8cc20bd6a2808d1195d2b1.
- Native desktop interaction is unavailable through this session's callable tools. Git CLI and the repository's Unreal launcher are used; GitHub Desktop interaction is not claimed.

## Windows compile

`ProjectVelkorranEditor Win64 Development` succeeded using UE 5.7, VS 2022 MSVC 14.44.35216 and Windows SDK 10.0.22621.0. All 32 build actions completed in 98.09 seconds. This was an incremental native build with unchanged source, not a full clean rebuild, Game build, automation-suite pass or gameplay qualification.

The initial sandboxed attempt failed accessing UnrealBuildTool's per-user configuration cache. The elevated retry succeeded. No C++ correction was made. C4996 deprecation warnings remain recorded in the build log.

Evidence: `Saved/Validation/Aurelion/Workstation-20260913/Build.log`, `initial-status.txt`, `initial-working-diff.patch`, and `prior-editor-diagnostics.txt`. The latter captures warnings/errors from the prior editor session before this session's launch, not fresh validation results.

## Content prerequisites

The local filesystem contains `SciFi_Drone_1`, including its skeleton, meshes, animations, `GA_DroneGunfire` and `GA_DroneRocketAbility`. Fourteen assets exist in the untracked `Content/Cues` tree. Presence does not establish load correctness, distribution availability, or combat operation. No dependency has been removed or substituted.

Prior editor diagnostics include the invalid `SetByCaller.Damage` tag in Narrative's `GE_Item_Instant`, Aura configuration/process warnings, and a missing skeleton compiler error in the CombatMasterBundle demo `ABP_Manny_PostProcess`. These are historical observations; their relevance to fresh Aurelion PIE must be established before changing assets.

## Live run

Launched the existing `validate_aurelion_entry_pie.py` through `run-editor-script.ps1` with `-Visible -KeepEntryOpen -ContinueE1 -ContinueRoute`, starting M12 with a fresh isolated profile. Process ID: 25844. Evidence directory: `Saved/Validation/Aurelion/WorkstationEntry-20260913-042936-2fac3a4d`.

This launcher disables audio. Its reports cannot establish physical keyboard/mouse operation, rendered presentation review or audio correctness. No gameplay pass is claimed from successful launch or process exit.

### First observed failure and bounded correction

The first run entered M12 PIE with `BP_AurelionPlayerController` possessing `BP_SovTarrik`, earned the native `TarrikArrival` receipt through the ordinary hold interaction, selected Cinderline through normal wheel input, and reached active E1. The owned weapon/ammo/Shield HUD check passed; the observed Shield was 100/100. All protected Aurelion asset hashes remained unchanged.

At `validate_aurelion_entry_pie.py:286`, the observer failed with `Actual placed-enemy startup inspection did not pass`. Its twelve pending entries all claimed `authored weapon is not present/equipped in its normal inventory` for E3/E4 Linkbound, WallRunner, Weaver and Elite actors. There were no contract failures or inspection exceptions.

Owner: validation script. `inspect_aurelion_enemies_pie.py` still demanded demo swords/pistols for these roles, contradicting the loaded empty definitions and the intentional content change in `9bfb44e1`. The minimal correction asserts that these unarmed roles retain empty authored loadouts and have no runtime weapon items. Enforcer's existing equipped ranged-weapon check and all ability/activity/readiness checks remain enforced. No assets, gameplay behavior or C++ were changed. Python AST parsing passed.

Restarted from a fresh isolated profile using the corrected observer: PID 37032, `Saved/Validation/Aurelion/WorkstationEntryLoadout-20260913-043338-1cbd2b47`. The original failed report is retained intact. This run failed before reaching the enemy gate: `Normal weapon-wheel selection exceeded its recorded bounded window`. The selector stayed in `await_narrative_load` with `actual_character_pending_load=true`, zero raw input frames and no wheel interaction. No timeout was enlarged and no equipment state was forced. All protected asset hashes remained unchanged.

Owner remains ambiguous between content loading and native lifecycle. `ANarrativeCharacter::IsCharacterPendingLoad` checks missing visuals, pending appearance handles and the definition handle. This observation does not identify the offending handle. Aura additionally reported `Server process launched but never reported a listening port after 50 poll attempts (10.0 seconds)`; causation is not established. Recommended follow-up is read-only tracing of those handles if pending load recurs, without changing native readiness rules.

### Independent real-PIE startup verification

Ran `probe_aurelion_roster_startup_timeline.py` in a third fresh profile: `Saved/Validation/Aurelion/WorkstationRoster-20260913-043634-717e9e9d`, PID 32216. It completed its 30-game-second observation and exited normally.

The final `enemy-startup-readonly.json` reports `passed_observable_startup_contract` for all 24 enemies, with empty contract failures, startup-pending entries and inspection errors. This verifies the corrected loadout observer in actual PIE and confirms native enemy startup on this run, including the installed drone dependencies. It does not establish that drone attacks damage the player.

The broader timeline remains **failed**: five E3 Linkbound actors and the E3 Weaver exceeded its 0.5 cm placement tolerance. Example: E3.Linkbound1 moved from (-800, 8500, -510) to (-799.4926, 8500.9825, -510); E3.Weaver moved from (0, 11000, -510) to (0.2979, 10999.1738, -510). Their native activity components were active. This could involve held-participant movement or expected separation adjustment; neither the tolerance nor gameplay was changed. Asset hashes remained unchanged. Preserve this failure separately from the successful final startup check.

A fourth fresh route attempt in `Saved/Validation/Aurelion/WorkstationRouteRepeat-20260913-043915-3fa0be91`, PID 15484, reproduced the same pending-player-load failure. At exactly 30.0 seconds of selector time it still reported `await_narrative_load`, `actual_character_pending_load=true`, and zero raw mouse input frames. The overall entry report failed at 54.141 seconds. Protected asset hashes remained unchanged; the driver ended PIE and the editor exited. No earlier failures have been replaced or relabeled.

## Initial handoff / remaining gate (superseded by the Eclipse pass)

The following records the initial investigation, not the current final status.
See `AurelionEclipseAlignment-2026-09-13.md` for the later bounded readiness wait,
formation bootstrap regression/fix, passing fresh entries, Parasites content and
subsequent combat failures. The original evidence below is retained.

The blocking observable is repeated failure of Tarrik's normal load readiness before weapon selection, not a proven weapon-wheel or damage defect. Recommended owner: Caelis for native lifecycle diagnosis, with content inspection of the actual appearance/definition load handles. The cause is ambiguous, so no source correction or plugin modification was made. Identify which branch of `IsCharacterPendingLoad` stays true and the corresponding requested asset/handle before altering lifecycle behavior. Preserve the two timed-out fresh runs and compare them with the first run that successfully wielded Cinderline.

Work can continue around this on asset inspection and presentation preparation, but the player-order combat route, Selene handoff/combat, command sever, Null Pulse, checkpoint/recovery, full traversal, audio, physical controls and packaged performance remain unqualified. Advancing the automated route by forcing equipment or ignoring readiness would hide the observed failure. The startup-placement drift also remains an independent failed qualification.

Delivered edits are limited to the stale loadout observer correction and this report; the user's initial map, descriptor and untracked assets are preserved. No generated files were committed and no branch was merged, reset or squashed. The two preservation branches were subsequently published after explicit user approval; remote heads match the original local commits. Current uncommitted edits were not included in that push.
