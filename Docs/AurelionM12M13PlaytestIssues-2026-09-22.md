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
- **End-to-end confidence:** prior unpaced M12→M13 and CP9 route runs passed,
  but a final uninterrupted fresh playthrough after the current changes is
  missing. Next: replay both missions without forced progression, then death,
  checkpoint retry, restart, and transition variants.

## Player and enemy combat

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
