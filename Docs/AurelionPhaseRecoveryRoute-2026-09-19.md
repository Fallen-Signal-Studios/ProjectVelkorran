# Phase recovery mission replay

Run: `Saved/Validation/Aurelion/PhaseRecoveryRoute-20260919-144545-43c03dbe/`, against commit `915dd490` after full build/test gate `20260919-144405-b6f7fbcb` (719 tests). No content or source was changed during the replay.

E1, E2, E3 entry, E3 rescue, E4 entry and E4A passed their input continuation checks. E4B failed with `Player retired; no recovery issued` during frost preparation. Its last sample had Tarrik at 2 health, the companion approximately 53.18 cm from the frost anchor, and no completed thermal payoff. This run does not establish encounter victory or repeatable thermal progression.

The continuous HUD retirement observer completed 969 samples with no findings. Actual cinematic restores made the legacy container Visible while its weapon child remained Collapsed, including the later Tarrik handoff. This supplies mission evidence for the nested ammo retirement fix. Live editor-window review also exposed an empty legacy minimap ring at upper right. The saved HUD export identifies the collapsed `WBP_Navigator_Map_Minimap` as a direct canvas child subject to the restore path; that separate regression remains open.

Floor presentation is not qualified by this run. Both E4A protected actors acquired the lattice at observer time 624.34 seconds. The Weaver lost its visible mesh overlay at 627.36 while still held, until release at 630.88. The Elite retained its lattice through this sampled phase and reacquired it for E4B at 636.20. A later actor instance also showed recovery from one to two overlaid meshes between 897.19 and 897.69 seconds. The surviving Weaver gap requires investigation; the controlled coexistence probe is insufficient evidence of complete mission coverage. No lethal health hit while protected was observed.

After the failed route, the retained world continued running until explicitly stopped through the editor API. Both observers finalized as `observation_finished`; their findings arrays are empty. Thus destroyed-object cleanup now completes in this fresh engine session. Post-failure observations are not progression acceptance. The creator-owned M12 map and grenade assets remain outside this work.
