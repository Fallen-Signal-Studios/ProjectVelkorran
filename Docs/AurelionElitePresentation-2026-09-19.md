# Elite presentation and thermal reach audit — 19 September

## Findings and repair

Fresh editor inventory `ThermalLayout-20260919-132133-00037b58` loaded the current NPC definition, activity configuration and granted ability CDOs. The Elite points to `AC_Abilities_AurelionElite`, which retained the humanoid `GA_Melee_Punch_Unarmed` while the compatible `GA_EclipseElite_Melee` existed in the separate Eclipse configuration. Commit `5560e414` switched the NPC configuration to supply the native boss repertoire after the earlier parasite presentation pass. The current grant list therefore lost the parasite melee presentation.

`repair_elite_parasite_melee_grant.py` changes exactly that one grant in the current configuration. It preserves grant order, all other abilities and both startup effects. It verifies the replacement attack definition, unarmed admission, montage skeleton compatibility with SK_Fat and both sweep bones before saving; the original asset is backed up in the run directory. It does not switch the NPC back to the old configuration, which would discard its current boss repertoire.

Authoring run `EliteMeleeRepair-20260919-132358-8f79c0e6` saved only `Content/Aurelion/Enemies/AC_Abilities_AurelionElite.uasset`. The recorded grants retain native Slam, Lance and Summon, and startup effects retain Elite Poise and Durability. The replacement uses `AM_EclipseElite_Attack`. The read-only configuration verifier now also rejects a missing/duplicate parasite melee grant or a returning legacy humanoid punch. Skeleton/grant validation is not proof of visible impact timing or an actual successful melee hit.

## Thermal encounter remains open

The prior actual route `E2ShotProgress-20260919-130017-3376fb3a` reached E4B but could not find a position within both the heat terminal interaction range and the native Elite heat-confirm range. Selene had moved within 125.36 cm of the frost mark. A fresh editor navigation query from the observed Elite position (241,20705,-1110) to (550,21400,-1110) and (600,21700,-1110) returned complete paths. This rules out a missing generic navigation connection at those sample points, not an agent-specific collision, live AI target-selection or movement defect.

The native Lance has a 600–4000 cm range. `GetBotCombatMovementRange` may choose an available attack's preferred range, so waiting near the terminal does not itself guarantee that the Elite will close to heat range. This is a source-based explanation to investigate with live AI state, not a verified root cause. No ranges were widened, enemies moved, map saved, or victory fabricated. The melee grant repair does not establish that E4B is fixed.

## Engineering follow-up for readable boss casts

The native Elite base `ActivateAbility` commits, calls `ExecuteBossPayload`, then ends in the same call. Its header exposes no montage or wind-up timing property on this execution path. Slam, Lance and Summon are granted directly as native classes. The content grant repair above supplies the ordinary melee montage only; it does not supply authored casts for those three abilities.

Reliable wind-up/impact/recovery and interruption timing need an engineering-supported presentation/payload boundary on this path. A cosmetic animation after an immediate hit would not satisfy the requested authored casts. Per `EngineImplementationHandoff-2026-09-18.md` (“if a task seems to [require source changes], stop and say so rather than editing C++ to fit the content”), no C++ was changed. This remains an explicit incomplete requirement, alongside E4B thermal reach and full phase-cue activation/removal/poise acceptance.

Validation baseline: `20260919-131501-62a1cec0`, build without SkipBuild and 719 passing automation tests. Fresh readback and post-change validation are recorded below when complete.

Fresh process `EliteMeleeReadback-20260919-132522-585c3d1e` passed the updated configuration verifier: one parasite melee grant, no old punch, all three boss abilities and both durability/poise effects. This verifies the saved asset after reload, not live attack timing. The original enemy seed generator still reconstructs humanoid source appearances; it was not rerun as part of this targeted repair.

Post-change full validation `20260919-132707-97d30220` passed the build without SkipBuild, all 719 matching automation tests, coverage and source-integrity checks (95 warnings). The creator's M12 map retained SHA256 `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`. No gameplay route or rendered attack was qualified in this increment; all remaining requirements above stay open.
