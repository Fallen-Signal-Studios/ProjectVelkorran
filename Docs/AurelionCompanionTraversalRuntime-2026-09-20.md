# Companion traversal fixture rejection

The direct-NPC-spawn studio cannot qualify companion locomotion. Companion proxies intentionally remain staged until the campaign initializes and activates them. Native `bStagedForTransition` defaults true; visual initialization calls `SetProxyStaged`, which hides the actor/visual and suspends movement ticking. The public NPC subsystem spawn path does not perform the required campaign activation.

Two isolated PIE runs established this:

- `Saved/Validation/Aurelion/CompanionTraversalRuntime-20260920-211743-bb495fef` spawned both actual companion classes with their normal NPC definitions. Both accepted the native traversal request, detected the 100.25 cm obstacle and selected `AM_M_Neutral_Traversal_Mantle_1_0_stand_F_Rfoot`. Each recorded 44 warp-active samples, entered custom movement and returned to Walking after the montage; neither capsule changed position. The screenshot shows weapons/shadows without clearly visible bodies and is rejected as successful traversal evidence.
- `CompanionTraversalMotionAudit-20260920-212047-a214aecc`, under the same parent directory, repeated with initialization readbacks. Both actors and visuals were hidden; movement ticking was disabled. Both animation instances used RootMotionFromMontagesOnly, and the selected sequence had root motion enabled and force root lock enabled. Thus the stationary capsules do not demonstrate a broken root-motion animation.

Both editor processes ended. No mission map, character default or animation asset was changed. The preceding chooser assignment repair remains verified by its separate fresh-load evidence, but successful runtime movement is still unproven.

`Scripts/Validation/Aurelion/check_companion_traversal_runtime.py` now rejects a hidden or movement-disabled direct spawn before requesting traversal. This guard was added after the two runs above. It prevents repeating the invalid setup as a success claim; its next use will require a campaign-activated fixture to proceed. Do not work around staging by manually enabling movement/visibility or changing montage root-motion flags. The next meaningful live check must use a real checkpoint/handoff companion and confirm activation first.

Baseline full build/test gate: `Saved/Validation/20260920-211240-dfbb6387`, 722 tests. Final gate `Saved/Validation/20260920-212406-d1d83c83` passed the full build, 722 matching tests, coverage and source integrity. This investigation establishes a fixture limitation, not a new production gameplay defect or successful vault/climb behavior.
