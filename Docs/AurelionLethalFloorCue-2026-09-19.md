# Aurelion phase-protection cue

## Actual route: protection presentation still fails in combat

Fresh visible run `PhaseCueRoute-20260919-135454-4075817c` passed entry, E1, E2, E3 entry/rescue, E4 entry and E4A through ordinary injected gameplay inputs. The read-only observer now attaches before PIE, waits for a real world, and records native target damage receipts alongside floor and visual-mesh overlay transitions. It does not spawn, toggle protection, deal damage, move actors or drive input.

The route exposes a gap that the isolated activation/removal tests missed. At observer elapsed 553.157 seconds, the phase activated both E4 Weaver and Elite protection. Both had lattice overlays at 554.204. The Weaver overlay was absent again at 557.219 while its floor remained held; the Elite overlay was absent at 559.235 while still held. Both floors cleared at 563.391 after the actual link resolution. E4B reactivated the Elite at 567.422, its lattice appeared at 568.438, and it disappeared at 582.844 while protection persisted until 604.922. The exact overwriting/clearing owner is not established. The current stop-after-first-assignment retry does not maintain the presentation through combat. Task 4 is **not accepted**; do not characterize these transitions as successful visual lifecycle coverage.

This run did progress beyond the earlier E4B reach stall. Native thermal receipt `3C275E704324270A0E2E84A2FBC78857` recorded 99 poise damage and an actual poise break while the Elite floor was held, following heat receipt `FA5B191D42D10AC711E548AE9CA80D8E` with 1 poise damage. Both dealt zero health damage, with Elite health 532. The native Core was revealed; the subsequent ordinary Core shot produced `5A82AC4A4CE2A0580105DB995C7FC21A`. Protection then cleared. This verifies the thermal/poise/Core chain in this run, not lethal-floor damage clamping: no lethal health hit while held was observed. Selene reached within 52.34 cm of the frost anchor through the native partner request. E4B ultimately failed at 51.531 seconds because the player retired during subsequent combat. No mission victory or repeatable reach fix is claimed.

The observer's original teardown tried to marshal already-destroyed ASC wrappers through IsValid, causing a Python error after PIE ended and leaving its JSON status at `observing`. Its recorded transitions and receipts were written during gameplay; the terminal status is not a pass. Cleanup now tolerates that specific null ObjectInstance error, continues removing live delegates, and writes the report in a finally block. A focused Python regression with a dead wrapper followed by a live delegate passed; fresh engine teardown coverage is still pending. Preserve the original report without rewriting its status.

The creator-owned map and grenade assets were not saved. M12 SHA256 remained `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`. The full pre-change baseline was `20260919-135030-3413be81` (build without SkipBuild, 719 tests, 95 warnings). Post-change validation `20260919-140839-d6658baf` passed the build without SkipBuild, all 719 automation tests, coverage and source integrity. No tracked files were edited while validation ran. This gate does not qualify the outstanding live presentation defects or the revised observer teardown in Unreal.

## Deferred startup application

The content graph now starts a looping 0.05-second `TryApplyFloorOverlay` timer from WhileActive. That parameterless function casts the cue owner to NarrativeCharacter and guards `GetCharacterVisual` before calling GetMeshes. It assigns overlays to each returned mesh and clears the retry only after an assignment executes. An empty mesh array leaves the retry running. Loop completion returns without clearing it. Auto Destroy on Remove is enabled with zero delay; the engine's cue recycling cancels object timers with the default `AbilitySystem.ClearCueNotifyTimers=1`.

The normal Blueprint editor compiled and saved this graph in `LethalCueMeshes-20260919-110822-eebeb48e`. Its native T3D export confirms looping and Max Once Per Frame are enabled and that the timer is cleared from the mesh-assignment path. The previous unreachable WhileActive mesh chain remains disconnected; the actual entry starts the timer.

The first early-start probe (`LethalCueStartup-20260919-110659-e1bdd6f4`) activated all four actors before their visuals existed, but found a missing Weaver overlay. That intermediate graph cleared the timer on loop completion even with no meshes. The final graph fixes that case. The probe also now allows 0.2 seconds after meshes first appear before checking the 0.05-second retry, avoiding an assertion in the loading tick itself. The preserved startup probe tests held and immediately cancelled cases for both classes, then a second activation/removal cycle for all four actors.

Fresh process `LethalCueStartup-20260919-111152-0bb59ffd` passed. All four actors were activated at game time 0.4 seconds with absent character visuals. Both held actors later received the lattice; both cancelled actors remained clear. All four then passed reactivation and removal. No Python errors, Accessed None messages or ensures appeared. This is isolated lifecycle evidence, not visual, damage, poise, multiplayer or campaign-phase acceptance.

Full post-change validation `20260919-111327-f7c31141` passed the build invocation without SkipBuild, all 719 automation tests, coverage and source integrity. No tracked files changed during validation. The pre-change full baseline was `20260919-104827-e58010bd`.

## Runtime findings and material correction

`LethalCueRuntime-20260919-104249-81ddb385` assigned the existing Elite and Weaver NPC definitions through `AuthoredPlacedDefinition` before simulation. Unlike raw spawns, these actors initialize their Narrative character visuals. The test exposed a real startup race: activating the floor at four seconds entered `GC_AurelionLethalFloor.GetMeshes` before the Weaver visual existed, logged `Accessed None`, and left the Weaver without its overlay. At that point the cue was not campaign-ready: its graph needed deferred application with cancellation on removal. The timer-based content fix and limited runtime evidence are recorded above.

The same run reported a missing skeletal-mesh usage flag on the lattice material. `LethalCueUsage-20260919-104546-9fcb87ed` explicitly enabled and saved that flag, recompiled the material, and passed configuration verification. The authoring and verification scripts now require the flag. The copied cue graph's initialization race was still unfixed at that point; the deferred startup pass above supersedes it.

`LethalCueReadyRuntime-20260919-104652-451dddfc` then waited for both initialized visuals before activation. Both classes received the correct lattice material and cleared all visual mesh overlays through two activation/removal cycles. No missing skeletal usage warning or Python error was reported. `Scripts/Validation/Aurelion/probe_lethal_floor_cue_lifecycle.py` preserves this limited simulation probe. It does not test the startup race, visual appearance, phase resolution, damage or poise.

Full post-correction validation `20260919-104827-e58010bd` passed the build invocation without SkipBuild, 719 automation tests, coverage and source integrity. No tracked files changed while that validation ran. These gates do not cover the startup race described above.

## Original authoring pass

Authored `GameplayCue.Aurelion.LethalFloor` with a translucent violet lattice material and an actor cue copied from the inspected project overlay implementation. The cue applies only to character visuals, not weapons. `BP_AurelionElite` and `BP_AurelionWeaver` now contain configured `SovLethalFloorComponent` templates. Existing phase director code finds these components and activates/removes the cue through `SetFloorHeld`; no C++ or map changes were made in this pass.

Read-only M12 inspection found the link phase's Elite participant is E4.Elite and both required anchor links belong to E4.Weaver. This is why the Weaver, rather than the Enforcer, receives the carrier cue configuration.

Authoring run `LethalCueAuthor-20260919-103159-3667a97a` saved the material, cue and both Blueprints. Fresh process `LethalCueVerify-20260919-103337-6ee139d6` verified the saved cue tag, material reference, character/weapon flags and one configured floor component on each class. The initial author attempt failed before asset changes because it used an unavailable Python tag library; the corrected script verifies tag export text.

Full validation `20260919-103443-44619d02` passed the build invocation without SkipBuild, 719 matching automation tests, report coverage and source integrity. It reported 95 automation warnings; no packaged build ran. The pre-change full baseline was `20260919-102235-58ede5ae`.

Isolated runtime probes did not qualify lifecycle behavior. `LethalCueRuntime-20260919-103846-40d4020e` incorrectly inspected meshes on the NPC itself; Narrative uses a separate character visual. The corrected probe in `LethalCueRuntime-20260919-104005-61dca6d7` found that raw Blueprint spawns in the isolated Chaos scene have no character visual. The next check must use campaign-initialized actors or the actual NPC definition initialization flow. Neither run is a runtime pass.

Visual acceptance, real encounter phase resolution, damage/poise behavior with the cue, and interaction with other overlay effects remain pending. The copied removal graph clears the overlay rather than restoring a previous overlay, so coexistence must be checked in combat. This is not full handoff Task 4 acceptance.

The creator-owned M12 map and Cinder Sticky Grenade assets are excluded from this change.
