# Aurelion phase-protection cue

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
