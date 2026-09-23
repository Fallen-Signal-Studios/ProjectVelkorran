# M12/M13 playtest issues

The September 22 goal prioritizes progression, player mechanics, enemy combat,
mission clarity, then visual polish. This list records observed failures and
their next runtime check. A passing build alone does not close any gameplay row.

## Progression and recovery

- **M13 CP9 companion hold, intermittent:** prior earned reloads moved Selene
  8.4 cm or 30.2 cm toward Tarrik before holding; two later passive traces
  passed. Source order activated Regroup before releasing the staged proxy and
  the separate departure actor requested Hold on a later tick. The September
  22 native repair establishes the completed-departure hold before release;
  its extended test and five earned CP9 loads pass. This specific saved-pose
  regression is provisionally closed; other companion follow and combat
  behavior remains open. See
  `AurelionDepartureRestoreMovement-2026-09-21.md`.
- **End-to-end confidence:** `FreshM12M13Regression-20260922-174507-774a59ee`
  passed a fresh, uninterrupted rendered PIE route using ordinary movement,
  weapon, ability, and interaction input. E1, E2, E3, both E4 phases, M12
  completion/travel, and all thirteen M13 receipts through the physical lift
  and separate departures passed. The 22 M12 receipts persisted. A final CP9
  public save load then restored all 35 journal rows, evidence, player and
  companion identities, equipment/resources, lift state, and exits in a new
  M13 world. Both mission map hashes were unchanged and the dedicated editor
  exited cleanly. This pass preceded the E1 drone targeting change and is not
  current-build end-to-end acceptance. Death, restart, other choices, and
  repeated reliability still need coverage. With the updated drone attacks,
  `FreshM12M13CheckpointRetest-20260922-194206-7b4483c2` passed E1, E2, E3
  entry/rescue and E4A. In E4B, frost/heat payoff completed and the Elite died,
  but Tarrik died while cleaning up the remaining hostiles. Native recovery
  restored him about two seconds later; the E4B driver had already stopped on
  death, so M12 completion and M13 were not retested under this build. The
  earned ArenaEntry checkpoint from that run was replayed in
  `E4BAuthoredBodyAimRetest-20260922-201008-99c2607f`: the authored retry
  hold, restored thermal bindings, companion positioning, frost/heat/Core
  receipts and encounter victory all passed with ordinary input. Tarrik ended
  at 100 health after 57 spaced trigger presses, 154 reserve rounds remained,
  and every protected person survived. This proves E4B checkpoint recovery;
  it does not replace an uninterrupted current-build M12-to-M13 pass.
  `FreshM12M13FinalThermalChain-20260922-231542-7cf76772` now supplies that
  pass on the final heat range, frost-ground query and pilot. Rendered PIE
  used ordinary controls from a fresh E1 start through E1 with no retry, E2,
  E3 rescue, both E4 phases, native M12 completion/travel, and all thirteen
  M13 receipts including the physical lift, CP7/8/9 and separate departures.
  All eight follow-on drivers passed; E4B ended at 100 health with protected
  people alive. The full native build and 730-test gate had passed in
  `20260922-230932-fb3dcf50`. The editor exited without saving either map;
  both SHA-256 hashes remain at the baseline listed below. Current-build
  explicit CP9 reload, packaged execution, repeated reliability, other
  campaign choices and presentation acceptance remain separate checks.
- **E4B retry:** `E4BRetryRegression-20260922-180448-2aadb797` loaded an
  older earned ArenaEntry checkpoint into Failed, used the authored retry
  interaction, restored the Elite's thermal bindings, then passed frost,
  heat/Poise, Core follow-up and encounter victory with protected people
  alive. The screenshot needs visual review; this is one checkpoint/retry
  replay, not a death or mission-restart test. Neither map was saved.
- **E4B thermal usability:** the uninterrupted
  `FreshM12M13CompanionReconciliation-20260922-220907-80105bb3` passed E1
  without retry, E2, E3 rescue and E4A, then stopped at E4B because the
  moving Elite left no player standing point inside both HeatConfirm and the
  control's reach once modest safety margins were applied. The native heat
  reach was first raised to 450 cm from 350 cm, leaving a practical
  overlap around the unchanged 300 cm interaction. The 730-test gate
  `20260922-222110-f6ceabc7` passed. From that run's unchanged earned
  ArenaEntry checkpoint, `E4BHeatReachCombatRetest-20260922-222629-3e3332fd`
  read back 450 cm in live PIE and passed frost/heat/Poise, Core and ordinary
  combat victory with all protected people alive and Tarrik at 100 health.
  The first replay completed thermal/Core but Tarrik died during cleanup,
  so conventional-combat reliability remains open.
  A second uninterrupted run, `FreshM12M13HeatReachRetest-20260922-223009-f5d6a4a0`,
  passed E1 after two native retries, E2, E3 and E4A. At E4B, Selene stayed
  about 54 cm from the clean mark but the visibility-channel ground trace
  intermittently struck crossing combat collision instead of the floor.
  Grounding now queries the structural world-static walking surface while
  retaining the slope and distance checks. A regression puts Pawn collision
  across the old downward ray. Full build and 730 tests passed in
  `20260922-224509-0ef03b7d`. The earned checkpoint replay
  `E4BFrostGroundReplay-20260922-224645-7679ddc4` passed after one bounded
  frost retry, including actual heat/Poise/Core proof and conventional victory
  at 100 health with protected people alive. Neither checkpoint replay is a
  current-build uninterrupted M12-to-M13 acceptance pass.
  The next fresh chain, `FreshM12M13ThermalAcceptance-20260922-225058-f048b5a3`,
  passed E1 after three native retries, E2, E3, E4A and E4B thermal/Core. It
  then stalled with one WallRunner at 26 health behind cover: the validation
  pilot calculated a valid navigation approach for blocked sight but overwrote
  it with a 550 cm retreat. This is a pilot defect, not proof of failed enemy
  movement. The pilot now preserves the approach until sight is clear.
  An earned-checkpoint replay of that same save found the roaming Elite 718 cm
  from the heat control. The former 450 cm reach left only about 32 cm of
  native control overlap, so the final authored heat reach is 550 cm. Line of
  sight, control interaction and the three-second frost window still apply.
  The full 730-test native gate passed in `20260922-230932-fb3dcf50`, and
  `E4BMobileEliteWallRunnerRetest-20260922-231119-a6b1d7b4` read 550 cm in
  PIE, earned the thermal/Core receipts and defeated the whole encounter by
  ordinary input at 100 health with all protected people alive. The fresh
  uninterrupted M12-to-M13 proof on this final range and pilot also passed
  in `FreshM12M13FinalThermalChain-20260922-231542-7cf76772` as above.

## Player and enemy combat

- **E1 drone pressure and aim:** in the first fresh normal-input PIE probe,
  drones played their attack montages and emitted 48 gunshot packets, but none
  damaged Tarrik; 39 trajectories missed his direction by at least 60 degrees.
  Their Blueprint-triggered weapon abilities had no selector lease or actor
  focus even though AI perception held direct sight. The native weapon now
  retains the selector's exact target when present, otherwise uses the authored
  attack key or strongest current direct-sight hostile memory at payload
  release. Blueprint-triggered shots also reserve the encounter's attacker
  slot and an attack token, releasing both on ability end. This restored real
  incoming damage, so the gun's native spread rose to 8 degrees to reduce
  medium-range precision. `E1SpreadBalanceRetest-20260922-193121-5f9a7f36`
  then cleared all six drones with ordinary input after one native checkpoint
  retry, opened the pressure gate, completed both holds and handed off to a
  ready Selene. Its passive observer saw 43 damaging packets among 95 gunshots
  and 48 rockets, with no observer errors; 49 player damage receipts included
  one fatal hit and successful recovery. This proves one playable E1 run with
  real attack pressure, not final difficulty tuning or repeated reliability.
  The 729-test native gate passed in
  `Saved/Validation/20260922-192946-97b55b92`. A second fresh E1 run killed
  five drones before Tarrik died; native recovery traveled to its saved M12
  checkpoint, and the then-current driver rejected the new PIE world. The
  driver now rebinds its world, input and damage observers across that authored
  checkpoint load, but that branch still needs a completed rerun. The next
  fresh E1 run cleared the wave after one same-world rescue and continued to
  E4B as described above.
- **Enemy attack facing:** read-only PIE samples exposed a separate presentation
  defect: active drones could fire while over 90 degrees off Tarrik despite a
  valid damage target. The combat NPC base now turns toward an active attack's
  committed target, and direct drone abilities capture their sight target at
  activation. Drone payloads wait briefly for their visible body to align;
  an attack that cannot align cancels instead of firing sideways. The final E1
  normal-input replay `E1AttackFacingPrefireFinal-20260922-205454-c5464e04`
  passed, with 78 gunshots, 33 rockets and no observer errors; 48 sampled
  target-bearing firing montages averaged 7 degrees of body-facing error.
  A later shared-facing build made E1 too lethal: the normal-input pilot
  exhausted three retries with 74 damaging shots among 143 gunshots. The
  drone gun now fires 8-point shots in a 14-degree full cone, while rockets
  retain their authored impact. The final
  `E1FacingGun8Retest-20260922-214544-8e0ae28e` cleared the wave, opened
  the pressure gate, completed both holds and handed off after two native
  checkpoint retries. Its observer saw 35 damaging shots among 129 gunshots,
  64 rockets, no errors, and 124 target-bearing montage samples averaging
  0.3 degrees of facing error with none over 90 degrees. The cover regression
  now verifies that two 8-point shots retire a 12-health cover piece without
  damaging the character behind it. This is one playable balanced E1 route,
  not a repeated difficulty or packaged-build acceptance result.
  In E4B, the authored Elite and WallRunner attack montages usually had no
  controller focus at all. While attacking, the shared combat turn now uses
  their authored attack key or strongest directly observed hostile; wall
  traversal and cinematic control keep their own rotation. Before the change,
  51/66 Elite and 50/83 WallRunner attack-montage samples faced more than
  90 degrees away from Tarrik. The final earned-checkpoint replay
  `E4BEnemyFacingMobilePilot-20260922-211704-1f898a23` completed thermal
  payoff and conventional victory with Tarrik at 100 health and no observer
  errors. All 25 sampled Elite attack montages had a target and none were over
  90 degrees off; 22/25 WallRunner samples had a target, with four large angles
  while Tarrik crossed around its moving lunge. Two stationary pilot attempts
  died after enemies began facing and hitting correctly. The read-only pilot
  now retreats through ordinary movement input during conventional combat;
  this demonstrates a survivable play route, not final difficulty tuning.
- **Selene shot after weapon wheel:** the September 21 E4 contact test sent
  fire while BlockFiring, Equipping, and Reloading were present. Ammo stayed at
  one; no player damage receipt was recorded. The diagnostic now waits for those
  tags to clear. In a fresh September 22 route, the first armed shot missed and
  the second dealt 77.419357 nonfatal health damage to the Elite. This clears
  the suspected Staccato input/hit-registration failure for that encounter,
  though other weapons and abilities remain untested.
- **Tarrik companion contact:** in the latest fresh E4 run, Tarrik wielded his
  sword, closed to about 68 cm, played the native 1H sword montage, and dealt
  43.478264 Edge health damage to a living Linkbound 2.58 seconds after Selene
  damaged the Elite. This establishes one meaningful player-funded companion
  follow-up. An earlier run missed a moving Linkbound and later stopped outside
  attack range when no player damage funded contribution; repeat pressure,
  general following, and weapon presentation across protagonist swaps remain
  unqualified.
  Earned E4A repeat probes on September 22 narrow this: one Selene health hit
  funded a nonfatal 19.35-damage Tarrik strike, after which he stayed within
  68 cm and had a clear target but the contribution budget was exhausted. A
  second probe gave Selene three ordinary health hits; her third killed the
  Elite before Tarrik reached attack range, ending the encounter. Neither run
  proves a repeat-attack defect or sustained companion pressure. The test
  harness' optional repeat-hit experiment was discarded; only the saved PIE
  reports remain.
  An older earned E4A checkpoint had a separate restore defect: its saved
  companion list omitted the current sword primary even though the inactive
  Tarrik snapshot still owned Velkorran. Native staging now reconciles only
  validated saved weapon ownership and direct grants against the mission
  allowlist, for initial convergence, full load and encounter retry. The full
  build and 730-test gate passed in `20260922-215944-27bf58cf`.
  `RestoredGrantReconciliation-20260922-220541-cab59dd6` publicly loaded that
  original checkpoint and read back Guard, unarmed punch **and**
  `GA_Tarrik_MeleeLight` on the live proxy; the previous readback had only the
  first two. `RestoredCompanionKitReplay-20260922-220326-09c97341` passed the
  ordinary-input E4A route; Tarrik drew Velkorran and played 38 sampled light
  attack frames. No companion damage hit landed in that short replay, so
  sustained restored-companion contact remains unqualified.
- **Archetype and ability coverage:** each mission enemy and each available
  protagonist ability still need the complete input→effect→recovery check under
  normal play. Prior component tests and isolated authoring checks do not cover
  that matrix. Next: inventory mission grants and exercise each in PIE.

## Readability and polish

- **Eclipse blood in combat:** the first-hit failure was a native readiness
  gate: loaded black Niagara systems were not ready while compiling in PIE,
  and the blood component discarded the hit. The September 22 native repair
  spawns valid systems and prewarms them in the editor after async load. An
  earned E4A retry now shows a ready black Slash system on Tarrik's actual
  Linkbound hit and a distinct black spray against the light floor in the live
  viewport. This closes that specific first-hit defect. Red blood, other
  roles, reduced-effects quality, and sustained combat readability remain
  open. See
  `AurelionCombatBloodCapture-2026-09-20.md`.
- **Elite protection cue shader:** earned E4A PIE logged a failed compile for
  `M_AurelionPhaseLattice` and substituted Unreal's default material. The
  custom HLSL declared `line`, a reserved modifier. The authored material and
  its source script now use `bandLine`; an isolated editor pass saved only the
  material, and a second earned E4A PIE replay showed the purple protection
  lattice with no material compile failure in its log. Both map hashes stayed
  unchanged. Other lethal-floor cue states still need visual coverage.
- **Objective HUD capture:** the apparent top-left overlap in one live editor
  screenshot is the transient `Preparing Niagara System` compile message over
  the objective panel. A read-only PIE probe found one presentation, one
  objective row, and a clean subsequent frame. No gameplay HUD defect is
  established by that capture. The later E4B retry terminal frame still shows
  a plain white, multi-line objective status panel in the top-left of the
  viewport, visually out of step with the amber holographic Tarrik bars and
  radar. The native panel now separates a small protagonist-colored status
  line from the readable objective text and uses a stronger dark glass veil;
  the world marker uses smaller regular type on a lighter translucent plate.
  All 729 automation tests pass. Tarrik's revised panel was inspected in an
  E4B PIE terminal frame, and
  `SeleneGlassMarkerFinalReview-20260922-202941-8565c161` passed the restored
  checkpoint HUD inspection with four rendered captures and no presentation
  errors. The Selene frame shows blue status and marker lines clear of the
  central plate. Full-screen, high-contrast and packaged visual acceptance
  remain open.
- **Selene face shading:** an intermittent black face is proven in BaseColor,
  including a held static camera, with identical configured material paths in
  healthy and faulty runs. Root cause remains unknown. Next: compare actual
  face material inputs/VT sampling during a failing process.
- **Environment:** the departure lounge has custom walls, seats, and paving;
  shuttles and open canopy still need an authored fidelity and performance pass.
  Preserve native collision and the current saved-map baselines.

Current worktree contains user or other-session changes to M12, Tarrik's
grenade, drone appearances, camera assets, and new architecture source. Treat
these as in progress; inspect each before modifying or saving them. The last
verified M12 hash from the previous pass was
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`;
it is a historical reference, not an assertion about the present worktree.
At the September 22 baseline check the actual worktree map SHA-256 values were
M12 `F60912F6C87926FC2EC7D9CE4DD34BEC4CC69CE486FA22AC6C7062F9F3690CBE`
and M13 `F3E53457363A0AAB88BA81225287E39F36F49693798B98873BED9984A539DB22`.
The M12 asset is modified; none of the tests in this pass saved either map.
