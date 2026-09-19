# Aurelion phase-protection cue

## Runtime findings and material correction

`LethalCueRuntime-20260919-104249-81ddb385` assigned the existing Elite and Weaver NPC definitions through `AuthoredPlacedDefinition` before simulation. Unlike raw spawns, these actors initialize their Narrative character visuals. The test exposed a real startup race: activating the floor at four seconds entered `GC_AurelionLethalFloor.GetMeshes` before the Weaver visual existed, logged `Accessed None`, and left the Weaver without its overlay. This cue is not campaign-ready. Its content graph needs to tolerate an absent visual and apply when `CharacterVisualInitialized` fires, with removal cancelling that deferred application.

The same run reported a missing skeletal-mesh usage flag on the lattice material. `LethalCueUsage-20260919-104546-9fcb87ed` explicitly enabled and saved that flag, recompiled the material, and passed configuration verification. The authoring and verification scripts now require the flag. The copied cue graph's initialization race remains unfixed.

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
