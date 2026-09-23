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
  both SHA-256 hashes remain at the baseline listed below. The run's earned
  CP9 banks were then loaded twice through the public save owner in separate
  M13 worlds by `FinalBuildEarnedCP9Reload-20260922-233520-65003e34`.
  Both four-second stable snapshots passed the full 35-row journal/evidence,
  identities, inventory/resources, completed lift and separate-exit checks;
  controls were released and exactly one Tarrik HUD showed 100 health. Both
  rendered frames were inspected: no duplicate HUD or cinematic overlay was
  visible, while the departure area still needs the planned fidelity pass.
  Packaged execution, repeated reliability, other campaign choices and
  full-screen presentation acceptance remain separate checks.
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
  On September 23, a second earned-checkpoint replay exposed the remaining
  montage conflict across the parasite roles: Linkbound attack samples stayed
  as much as 172 degrees from their target even when the actor had a current
  combat-facing target. Narrative's orient-to-movement setting was competing
  with the native turn when Blueprint attacks had no AI actor focus. Linkbound,
  Elite, WallRunner, and Weaver now resolve their attack turn after movement
  and mesh ticks, and use the explicit native turn instead of movement-facing
  while a directly observed combat-facing target is active. The original
  360-degree/second turn rate remains; wall traversal and sequencer control
  retain their own rotation. In `E4BExclusiveCombatTurn-20260923-010935-08dca77d`,
  the public earned-checkpoint load, native frost/heat/Poise payoff, Core break,
  and conventional E4B victory all passed. Its read-only observer recorded
  47 Elite, 46 Linkbound, 7 WallRunner, and 6 Weaver target-bearing attack
  montage samples with zero over 45 degrees; the worst Linkbound sample was
  40.7 degrees. A second independent earned-checkpoint source,
  `E4BWallRunnerLongCombatFacing-20260923-011239-db1cd5b7`, also completed
  thermal payoff and conventional victory; all 18 sampled WallRunner attack
  montage frames stayed within 8.1 degrees of their current target. These
  qualify two E4B replays, not fresh wall traversal or every enemy in the
  campaign. The UE 5.7 editor build and all 14
  `ProjectVelkorran.Campaign.Threat` native automation tests passed in
  `EnemyFacingNativeTests-20260923-011705`.
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

## September 23 fresh-route follow-up

- **Enemy attack facing:** `5b2dc6d4` moves the Linkbound, Weaver, WallRunner and Elite's combat-facing turn after movement/mesh ticks and prevents Narrative's orient-to-movement setting from counter-turning an active target. Two earned E4B PIE replays completed the encounter; the longer replay sampled 18 WallRunner attack frames with a maximum 8.1-degree target-bearing error. The 14 native threat tests passed.
- **E2 Enforcer facing follow-up:** A fresh ordinary-input route reached and completed E1 and E2 in `E2EnforcerFacingReplay-20260923-030228-c85d6e38`. A passive PIE observer sampled all four Enforcers 303 times during E2. Their authored Blueprint does not enable the native hard-lock combat turn, but Narrative's controller focus kept 288 focused samples within 11.1 degrees of the live target (95th percentile below 0.5 degrees; none over 45). Fifteen samples had no focus; the seven more than 90 degrees from the player were hit reactions, not attack montages. The 39 observed montages were hit reactions, rifle reloads or unholster, so this run does not certify a dedicated Enforcer firing montage or every payload frame. The earlier first fresh E1→E2 pass also completed, but its passive observer failed on an unsupported Python mesh accessor and is excluded from facing conclusions. No Enforcer rotation was changed without an attack-facing failure. This narrows the reported issue to roles or frames not represented by a focused Enforcer in this build; the existing drone and Eclipse fixes above remain in place.
- **E2 Enforcer held retry:** `E2EnforcerPassiveFacing-20260923-034236-b96a9af2` publicly loaded the same earned E2 entry and used ordinary retry input, then held the arena Active for 30 seconds. After the restored world became active, four Enforcers provided 306 body-facing samples, 272 with controller focus. Their maximum body-to-focus error was 1.99 degrees (95th percentile 0.45; none over 45). The 36 montage frames in this active window were rifle reload or unholster, with no sampled character firing montage. This corroborates facing during target focus but leaves the visible rifle firing pose and shot-release frame unqualified; inspect the weapon/upper-body animation path before treating that absence as a missing montage. The observer and checkpoint runner both completed without errors.
- **E2 Enforcer rifle-fire presentation:** `E2EnforcerWeaponFireVisible-20260923-035532-f6f99fb2` observed 37 actual rifle attack timestamp advances in an earned E2 retry, but zero character fire-montage frames. The Enforcer now plays an owned 0.233-second upper-body rifle montage, retargeted from the Fab sci-fi rifle clip, when its wielded weapon's real attack timestamp advances. A fresh normal-input E1→E2 clear (`E2EnforcerFireFresh-20260923-041311-c68a3e2f`) retained four attacks and four sampled fire-montage frames; E2 passed with 19 player damage receipts and 100 Selene health. The longer earned E2 retry `E2EnforcerPlayerFireFrame-20260923-043046-be06c2b8` recorded 98 attack advances, 138 fire-montage samples, one real local-player viewport capture, and zero observer errors. Of 123 focused montage samples, the maximum body-to-focus bearing error was 4.38 degrees. The stationary 60-second pressure probe included a player death and retry; it is not a clean encounter victory. Client replication and close-range pose quality still need multiplayer/visual review.
- **M12 enemy-facing coverage audit:** Read-only map inventory `M12EnemyFacingInventory-20260923-045120-081ee439` found six Security Drones, two Contaminated Drones, four Enforcers, seven Linkbound, two WallRunners, two Weavers and one Elite. Every listed combat role except the stock Narrative Enforcer enables native hard-lock facing; the Enforcer has the shot-driven fire montage above and retained controller focus during its attack samples. The earlier earned E4B report `E4BWallRunnerLongCombatFacing-20260923-011239-db1cd5b7` sampled active attack montages from every Eclipse role: Linkbound 2 frames, Weaver 5, WallRunner 18, Elite 13. Their largest body-to-live-target bearing errors were 0.01 degrees for Linkbound/Weaver/Elite and 8.08 degrees for WallRunner. This is sampled attack coverage, not a guarantee for every release frame or all player movement patterns. `M13EnemyFacingInventory-20260923-044958-ad383530` found no placed combat NPCs in the M13 wrapper.
- **M12 world objective cards:** The real player back-buffer capture from that E2 retry showed the relay-overlook instruction as enormous white text clipped across the top of combat. A read-only inventory identified `/Game/Aurelion/Maps/L_Aurelion_M12` `TextRenderActor_117`, a world-space graybox objective card with `hidden_in_game=false`; the native corner HUD was not the source. The five equivalent graybox objective cards (`TextRenderActor_116` through `_120`) are now hidden in game in the current M12 map, while the existing HUD objective and interactable waypoints remain. `RetireObjectiveCards-20260923-043936-9747ce0b` saved this exact five-actor change after verifying the pre-save map hash and preserving a backup. The next earned E2 retry, `E2AfterObjectiveCardRetire-20260923-044140-9486ffb8`, captured the same local player camera with the large white sentence gone, 98 rifle attacks, 140 sampled fire-montage frames and zero observer errors. Its 60-second passive review completed with an ordinary retry; this is a presentation validation, not an E2 victory. The map also contains many visible authoring labels; they need a separate, authored signage pass rather than an indiscriminate hide.
- **True HUD screenshot validation:** The first direct player back-buffer captures intentionally excluded Slate HUD layers. An editor-only capture of the sole local player's `SViewport` now includes those layers. `PlayerSlateCapture-20260923-045653-c57eabf6` passed at fresh CP0 with both scene and Slate-inclusive PNGs. `E2SlateHUDTimed-20260923-050119-43c1cbc1` passed an earned E2 retry and captured a live Selene combat frame with health/shield bars, ammo, radar, arc, objective panel, receiver marker, interactable outline and a directional damage caption; it also observed 46 attacks, 62 rifle-fire montage samples and zero observer errors. The five graybox cards remain absent from that HUD-inclusive capture. The caption's large black block was then restyled as protagonist-tinted translucent glass with regular-weight lettering and small accent corners in normal HUD mode; high-contrast bold/black presentation and the user's caption scale/opacity remain. All 52 UI tests passed, and `E2CaptionGlassReview-20260923-050655-18546179` captured the revised damage caption in an active earned E2 retry with no observer errors. It did not produce an Enforcer shot opportunity, so the earlier montage evidence remains the combat-fire qualification.
- **Retry interaction prompt:** The E2 combat HUD capture also framed Selene with a large `Interact: Aurelion` outline even while E2 was active. Map inventory identified that surface as the nearby E2 retry request actor; its authored action is `Retry encounter`, and `CanUse` rejects it until E2 is Failed. The HUD now suppresses outlines for unusable Aurelion request surfaces and uses each usable request's authored action text. `E2RequestPromptReview-20260923-051327-0463ac2f` captured active E2 with the false outline absent and the receiver marker intact. `E2FailedPromptReview-20260923-051558-a9d35a37` captured the earned Failed state with a clear `Retry encounter` outline and waypoint, then passed the ordinary 15-frame retry input. This changes only request-surface presentation, not admission rules or interaction routing.
- **E2 contaminated-drone facing and attack opportunity:** `E2ContaminatedFacing-20260923-031135-9d61c282` passed a fresh ordinary-input E1→E2 route, but its passive observer saw no contaminated-drone gunshot, rocket or target focus while the player cleared E2. That is an attack-opportunity concern, not evidence that a fired attack pointed away from Selene. `E2ContaminatedPassiveFinal-20260923-033515-24727188` publicly loaded the earned E2 checkpoint, completed the physical retry input and held the encounter active for 30 seconds with the player stationary. The read-only observer sampled 950 live drone frames, including 112 with controller focus or a native combat-facing target. Maximum target-bearing error was 0.000012 degrees, with none over 45. The drone played 26 sampled gun-fire and 18 rocket-fire montage frames, launched six gunshots and one rocket from its muzzle area, and four gunshots damaged Selene. The observer had no errors. This proves that the current contaminated drone can perceive, turn and execute both attack types in an earned encounter; the difference between the fast fresh clear and the held retry may reflect exposure or attack cadence and warrants a moving-player replay. One captured PIE viewport shows the E2 encounter, but these samples do not certify every enemy mesh, muzzle pose or visual effect. No rotation or AI asset was changed on this evidence.
- **Enforcer sideways fire reproduced and repaired:** the later 60-second earned E2 retry `E2EnforcerLostFocusLong-20260923-053809-61cb162e` recorded 21 real Enforcer rifle attack timestamp advances with no controller focus; 18 occurred more than 45 degrees away from the directly visible Selene, peaking at 148.75 degrees. This supersedes the earlier focused-only facing conclusion above. The M12 Enforcer Blueprint now enables the shared native combat-facing target and runs its actor turn in PostPhysics after movement and mesh updates, as the Eclipse roles already do. Enabling the target alone left six of 28 shot samples more than 45 degrees off because Narrative's strafe rotation still counter-turned the body. After the tick-order change, `E2EnforcerLateTurnShotAudit-20260923-055130-13dcbb75` passed a public earned-checkpoint load, normal retry input, and 60-second active PIE observation: 97 rifle attacks, 93 with a native target, 21 with no controller focus, 137 sampled rifle-fire montage frames, zero shot frames over 45 degrees, and a maximum body-to-Selene bearing error of 0.035 degrees. The stationary player died during the pressure soak; it is not an encounter-victory claim. `FreshE1E2EnforcerLateTurnFull-20260923-055627-67f4d88a` then used ordinary game input to clear E1 after two native recoveries, hand off to Selene, defeat all four Enforcers and both contaminated drones, and commit both E2 receiver interactions. The fast E2 clear produced no Enforcer fire samples, so shot-facing evidence comes from the earned retry. A separate E4B mesh/actor audit `E4BMeshFacingVerified-20260923-052846-5e21817a` passed the earned arena encounter and found 116/118 target-bearing Eclipse attack-montage samples within 45 degrees; two Linkbound samples lagged a rapidly changing target, one at 154.7 degrees. That brief Linkbound target-switch case and close-range visual pose quality remain open.
- **Failed E1 checkpoint retry:** the public checkpoint load and physical retry hold exposed a formation validation that ran against the defeated world's absent command source. Retry now checks saved entry identities before reconstruction and validates live formation/protection after replacement links are restored, before player release. The loaded-failure and corrupted-future-source regressions, plus all 15 coordination tests, passed. `E1RetryAfterFormationRepair-20260923-012818-0a01059a` then passed the actual saved E1 checkpoint load and normal-input retry in PIE.
- **Fresh M12→M13 traversal:** `FreshWallRunnerAfterRetryFix-20260923-012946-d03b5d40` earned E1, E2, E3, E4A and E4B through ordinary input, committed the closing M12 scenes, traveled into M13, and earned all 13 M13 receipts including the paired lift and CP7/8/9. Both E3 and E4 WallRunners completed their authored physical routes. The passive observer recorded 11/15 traversal samples respectively, but the parasite gait montage was present in only 2/3 of them; capsule motion on the ledge supplied zero animation velocity. The C++ presentation change now keeps the gait playing for the entire leased traversal. All 20 enemy-role tests passed, and `WallGaitFreshPIEAfterBuild-20260923-015150-dfee13ab` observed the rebuilt E3 WallRunner complete its route with the parasite gait in all 10 sampled traversal frames.
- **Ammo-sensitive replay:** that second fresh run spent almost all Cinderline rounds clearing E1, even after collecting a 63-round pickup. The E3 firearm-only validation pilot entered with one round and no reserve, then stopped when it found no reachable matching pickup. `E3AmmoMeleeFallbackMainhand-20260923-022525-3f2e54b9` publicly reloaded that earned E3 checkpoint, completed the normal retry-terminal hold, selected Velkorran through the ordinary weapon wheel and main-hand click, and verified it became wielded with Tarrik's light and heavy melee grants. This establishes a player-operable melee fallback, though it does not prove an E3 melee victory or rule out ammo supply tuning. Do not count the second replay as an E3 victory. The first fresh run above did earn E3 and M13.
- **Companion attack path:** `CurrentE4ATarrikContactEarned-20260923-023621-42e0778f` publicly restored an earned E4A checkpoint, used the authored retry, and passed the ordinary Selene Axiom/link/handoff route. Tarrik drew Velkorran but stayed outside its 180 cm attack range through this roughly nine-second phase; no attack or damage is claimed from that short window. `CurrentE4BSeleneCompanionContact-20260923-024029-d64b829a` then restored an earned E4B checkpoint and used the normal retry, weapon wheel and player shot. Tarrik dealt 21 health damage to a living WallRunner; 0.796 seconds later Selene's Verity Twin Blade montage was active and her native source receipt dealt 30 health damage to that same target. The passive observer had no errors. This proves a current-build draw→attack→health-damage path for Selene as companion, while sustained 15–25% encounter contribution and repeat Tarrik contact remain open. Both map hashes stayed at the baseline above.
- **Tarrik autonomous follow/attack replay:** `E2TarrikCompanionFollowCombat-20260923-060728-c7ea83a3` publicly restored E2 and used the normal retry, but its two companion observers found zero visible living companion samples over 60 seconds; E2 is therefore the wrong phase for this qualification. `E4ATarrikFocusCombatAudit-20260923-061207-8393936f` restored earned E4A and sampled Tarrik closing from 30.1 m to 0.86 m, drawing Velkorran, playing the authored sword light montage, and dealing 43.48 nonfatal health damage to a Linkbound after 7.56 seconds. That replay requested FocusTarget, but its command-state readout remained Idle and the request has no accepted-command receipt, so it is not proof of a successful explicit command. `E4ATarrikAutonomousCombatAudit-20260923-061443-2b49bcea` repeated the public load/retry with **no player or companion combat input**. Tarrik autonomously focused a Linkbound at 3.77 seconds, closed from 30.1 m to melee range, drew Velkorran, played the same light montage at 5.17 seconds, and dealt 43.48 nonfatal health damage at 5.56 seconds. Both passive animation/hit-path observers had zero errors. This establishes a current-build autonomous follow→draw→melee→actual damage path; it does not establish sustained 15–25% contribution, repeat-hit cadence, or the validity of a FocusTarget command in this room.
- **30 fps checkpoint reliability:** `CheckpointSoak100x30Current-20260923-024342-3748f439` completed 100/100 fresh M12 PIE starts and public CP0 save loads in one editor process at `t.MaxFPS 30`, with no failed cycles, ensures, fatal errors or Python errors. Load time was 4.27 seconds median, 4.50 seconds at p95 and 4.72 seconds maximum; the first cold PIE readiness took 31.56 seconds, while subsequent readiness was 1.08 seconds median. The editor exited normally. This qualifies repeat CP0 reload at a second frame profile; earned late checkpoints, long-session memory behavior and packaged-build reliability still need separate evidence. Neither map hash changed.
