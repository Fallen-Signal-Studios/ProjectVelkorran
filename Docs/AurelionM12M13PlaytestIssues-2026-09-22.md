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
  death, so M12 completion and M13 were not retested under this build.
- **E4B retry:** `E4BRetryRegression-20260922-180448-2aadb797` loaded an
  older earned ArenaEntry checkpoint into Failed, used the authored retry
  interaction, restored the Elite's thermal bindings, then passed frost,
  heat/Poise, Core follow-up and encounter victory with protected people
  alive. The screenshot needs visual review; this is one checkpoint/retry
  replay, not a death or mission-restart test. Neither map was saved.

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
  established by this capture.
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
