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
  A fresh September 23 E1 pressure run,
  `FreshE1PressureCurrent-20260923-153057-9481f3f9`, showed the remaining
  consistency risk: three native fatal-recovery retries succeeded, then Tarrik
  died a fourth time and the bounded ordinary-input driver stopped at 201.6 s.
  The passive observer completed without error and recorded 192 gunshots and
  81 rockets; the driver logged 83 player damage receipts and 11 cover
  attempts. Its first life almost cleared the initial wave, so this one failed
  pilot is not evidence that enemy damage must be reduced. The same E1 pilot
  source hash passed in `Z11WindowFreshM12M13-20260923-122626-8a9dad2e`
  after one retry; the compared M12 map and E1 drone asset hashes were equal.
  Compare cover use,
  exposure and player movement against the successful fresh route before any
  difficulty tuning; the saved E1 route is still playable but repeat
  reliability is not established.
  The [two-height cover pilot replay](Validation/E1CoverPilotTwoHeight-2026-09-23.md)
  found a validation defect: cover search used one 140 cm sightline, while
  arrival required torso and head shelter and misread some non-blocking Unreal
  trace tuples. After using the same blocking two-height rule at selection and
  arrival, a fresh ordinary-input E1→Selene handoff passed with two native
  recoveries and only one of two cover choices rejected, versus three native
  recoveries and 19 of 21 rejected in the preceding run. Both passive enemy
  observers completed without error. This corrects validation behavior, not
  enemy pressure; E1's repeated-death risk remains open.
  The [capsule and return-fire follow-up](Validation/E1CoverPilotCapsuleReturnFire-2026-09-23.md)
  measured Tarrik's actual 88 cm half-height, moved cover samples inside his
  capsule, and let the input pilot traverse reachable partial nav paths. A
  fresh replay still failed on its fourth death; its pilot had stopped firing
  during exposed travel to cover. After returning fire during those moves,
  another visible E1-to-Selene replay passed with one native recovery, 41
  incoming damage receipts and a zero-error enemy observer. No gameplay
  pressure settings changed. Repeat reliability and physical-control feel
  remain open.
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
- **Archetype and ability coverage:** the [September 23 player ability audit](PlayerAbilityLiveAudit-2026-09-23.md)
  exercised the current ten-ability roster in each M12 weapon context twice in
  PIE, observing A/B cast playback, Echo spend, Niagara, projectile classes where
  applicable, and real target hits in focused replays. Its synthetic Echo refill
  and stationary target make it a focused validation rather than full encounter
  proof. Interrupted actions, death during casts, checkpoint/handoff recovery,
  and each enemy archetype's attack impact still need the broader normal-play
  matrix.
- **Player ability resource gates:** optional live PIE negative-case runs
  exercised all 12 Tarrik and 14 Selene weapon-context repetitions at zero
  Echo, followed immediately by their normal A/B casts after refill. The
  rejected inputs caused no debit, cast montage, projectile, or damage to the
  aimed validation target; all 26 normal follow-ups met their cast/Echo/FX
  smoke checks. An earlier Stillpoint field hit other E2 enemies during three
  Selene negative windows, so those ambient receipts remain recorded and
  separate. See [the player ability audit](PlayerAbilityLiveAudit-2026-09-23.md).
  Natural cast interruption, death, prediction and every combat impact angle
  remain open.
- **Player Echo cancellation:** direct live-instance cancellation during
  Tarrik's delayed Hunger cast and Selene's held Axiom charge passed twice
  each in visible PIE. No unreleased payload, duplicate debit or lingering
  Busy tag remained; both ordinary B recasts damaged a validation target.
  Wake releases synchronously and provided no active post-input window for
  that probe. Natural enemy-hit, equipment-change and death interruption
  remain unqualified. See [the interruption evidence](Validation/PlayerEchoInterruptPIE-2026-09-23.md).

## Readability and polish

- **Companion camera collision:** both companion Blueprint capsules blocked
  Camera in their saved defaults. They now ignore Camera while retaining Pawn
  Block; a fresh editor reload confirmed both saved defaults. An earned E4B
  PIE replay passed frost/heat/Core and conventional victory with 13 Selene
  damage receipts and 248 live samples showing Camera Ignore. The terminal
  view was still obscured where Tarrik stood near level geometry, so close
  camera/wall readability stays open. See
  [the camera and E4B validation](Validation/CompanionCameraAndE4B-2026-09-23.md).

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
- **Environment:** the departure lounge has custom walls, seats, and paving.
  The M13 map now also has two framed/glazed dock views and the authored Z12
  canopy kit in both berths. Two warm/cool berth lights make the shuttles and
  coffers readable through those views. The canopy's 102 visual actors and
  two lights survived editor save/reload; two public earned CP9 loads after
  each saved step preserved the 35-entry mission state and a single working
  HUD. See [the canopy and light evidence](Validation/AurelionZ12CanopyPlacement-2026-09-23.md).
  The simple shuttle forms, grading and target-hardware performance remain
  open. Preserve native collision and the current saved-map baselines.
  A subsequent [custom concourse ceiling pass](Validation/AurelionZ12ConcourseCeiling-2026-09-23.md)
  replaced the short 22-panel, noncolliding roof strip with 28 measured 6 m
  custom bays across the full 42 × 24 m room. Saved editor views show the
  star-field gap closed without hiding either berth view. The map save and
  reload retained all other actor transforms and collision. Two earned CP9
  PIE loads preserved the mission state, player control and one HUD. Final
  player movement, lighting and performance checks remain open.
  A [custom Z08 containment pylon source and isolated Unreal review](Validation/AurelionZ08ContainmentPylon-2026-09-23.md)
  replace the unusable Higgsfield GLB as the production direction. The clean
  structure and removable Eclipse growth imported with UVs, material slots,
  Nanite and separate collision rules; the M12 map was unchanged. The lit
  engine capture still needs a stronger stone/organic finish and real Z08
  player-eye, collision, navigation and combat testing before campaign use.
  The [paired shuttle refinement](Validation/AurelionM13ShuttleRefinement-2026-09-23.md)
  adds tapered prows, forward glazing and panel/mechanical detail to both
  static scenic exteriors after a new image reference. It reimported the two
  meshes and corrected two shuttle-only armor palettes without saving M13.
  Close player-eye and dock captures were reviewed; from the concourse the
  protected mullion and berth distance still hide most detail, so the ships
  remain short of hero-asset quality. The full visible CP0→separate-departures
  route `FreshM12M13PostZ12Roof-20260923-194506-6461e714` passed all eight
  continuation stages on the saved roof map. After the shuttle revision,
  `PostRoofShuttleEarnedCP9-20260923-203111-696c1276` passed two public loads
  from that exact route's earned banks, preserving 35 receipts and one HUD.
  A full post-shuttle route and target-PC combat performance remain open.

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

- **Later current-map route and custom Z08 source:** `FreshM12M13CurrentMap-20260923-172049-7cc5e863` passed another visible CP0-to-M13 separate-departures route through all eight continuation drivers without an E1 retry. `CurrentMapCP9Reload-20260923-174039-4c08d0e4` then passed two loads from that run's earned CP9 banks; both captured HUD frames were reviewed and remained single/stable. The departure room still looks sparse. Separately, the Z08 custom containment pylon gained carved circuits, annular indices and finer removable Eclipse attachment; Blender round-trip and isolated Unreal reimport passed, but the engine image is still short of AAA material/tissue fidelity and no campaign fit is claimed. See [route/reload evidence](Validation/AurelionFreshM12M13RouteAndCP9-2026-09-23.md) and [Z08 art review](Validation/AurelionZ08ContainmentPylon-2026-09-23.md). The local M12 map remained at `64A4517BB88719694793A46AE859F0EB6FBC861EEF10E956E0574D8962B844A4`; its pre-existing changes were not staged.

- **Latest full-route and CP9 gate:** the [fresh route and earned reload report](Validation/AurelionFreshM12M13RouteAndCP9-2026-09-23.md) records a new visible CP0→M13 separate-departures pass through all eight ordinary-input continuation stages, followed by two public loads from its exact earned CP9 banks. Both reloads preserved 35 journal entries, evidence, equipment, resources and separate exits; the rendered HUD remained single and stable. An earlier E1 retry run died three times behind intentional cover, while this repeat passed E1 without retries or exercising the new occluded-target switch. E1 consistency and physical-device feel remain open. The live E3 frame still shows oversized world-space graybox signage during combat; replace it as part of the authored architecture/wayfinding pass.
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
- **Eclipse target-switch facing:** the M12 Linkbound and WallRunner Blueprint defaults now turn toward a live combat target at 720 rather than 360 degrees per second during grounded combat. The existing native wall-traversal path skips this turn. With only the Linkbound change, `E4BLinkboundFastTurnValidation-20260923-062135-c10c126f` publicly restored E4B, used normal retry and combat input, earned a native ThermalFracture victory, and sampled 103 Linkbound attack-montage frames with zero over 45 degrees and a maximum target error of 11.86 degrees; a WallRunner target switch still reached 53.72 degrees. A subsequent run with both faster turn rates caught one WallRunner target-switch frame at 104.2 degrees, so a higher rate alone does not guarantee perfect first-frame alignment. After removing the separate asteroid obstruction below, `E4BAsteroidRetireStableReplay-20260923-064315-62528c9e` passed the same earned retry and native victory with zero observer errors: 132 Linkbound, 65 WallRunner and 51 Elite target-bearing attack-montage frames all stayed within 45 degrees; one of 22 Weaver frames reached 70.37 degrees during a target change. These are sampled body angles, not impact-time hit alignment. A Python damage-delegate observer crashed its isolated editor run and was removed; no gameplay pass is claimed from that diagnostic attempt. Brief target-switch windup/pose quality, especially the Weaver, still needs direct visual and impact-time review.
- **E4B decorative asteroid blocking the route:** `E4BParasiteFastTurnValidation-20260923-062728-742319f6` failed the normal walk toward the MovePartner/heat controls at `(340.9, 20698.4)` while a capsule trace hit `BP_AsteroidField_Globular_C_2.ISM_Asteroid_4`; the terminal PIE screenshot showed a giant asteroid filling the chamber. Read-only editor inventory `M12AsteroidFieldInventory-20260923-063250-c27595a3` identified six backdrop fields and this exact placed field's 84-instance components as `BlockAllDynamic` against Pawn. A scoped editor save hid only `BP_AsteroidField_Globular2` in game and disabled its actor collision, after backing up the exact pre-edit M12 map (SHA-256 `BE71EC7724DE609809F880DA147FD7920DCEC6AD03AE9F2F0E306D1AD1BBAC53`). The resulting local map SHA-256 is `23555516889B0078EA29323F375B1E2D43E5EDF1132AD903E68989BB51DA61F1`; earlier uncommitted map work remains in that file. `E4BAsteroidRetireStableReplay-20260923-064315-62528c9e` then passed ordinary E4B retry, Selene's frost mark, Tarrik's heat/Poise payoff, Core follow-up and conventional combat victory with all protected people alive. The traversal observer recorded zero asteroid hits, and its terminal screenshot shows the chamber without the intruding rock. This is an earned-checkpoint replay, not a fresh uninterrupted M12 completion. The map asset is still local WIP and is not included in the pushed branch commit until its pre-existing map changes can be integrated intentionally.
- **Failed E1 checkpoint retry:** the public checkpoint load and physical retry hold exposed a formation validation that ran against the defeated world's absent command source. Retry now checks saved entry identities before reconstruction and validates live formation/protection after replacement links are restored, before player release. The loaded-failure and corrupted-future-source regressions, plus all 15 coordination tests, passed. `E1RetryAfterFormationRepair-20260923-012818-0a01059a` then passed the actual saved E1 checkpoint load and normal-input retry in PIE.
- **Fresh M12→M13 traversal:** `FreshWallRunnerAfterRetryFix-20260923-012946-d03b5d40` earned E1, E2, E3, E4A and E4B through ordinary input, committed the closing M12 scenes, traveled into M13, and earned all 13 M13 receipts including the paired lift and CP7/8/9. Both E3 and E4 WallRunners completed their authored physical routes. The passive observer recorded 11/15 traversal samples respectively, but the parasite gait montage was present in only 2/3 of them; capsule motion on the ledge supplied zero animation velocity. The C++ presentation change now keeps the gait playing for the entire leased traversal. All 20 enemy-role tests passed, and `WallGaitFreshPIEAfterBuild-20260923-015150-dfee13ab` observed the rebuilt E3 WallRunner complete its route with the parasite gait in all 10 sampled traversal frames.
- **Fresh regression after facing and E4B map edits:** `FreshM12M13FacingAndAsteroidRegression-20260923-064908-89cfe06f` started at CP0 in visible retained PIE and passed entry, E1 without a death, E2, E3 entry/rescue, E4 entry, E4A, E4B, actual M12→M13 travel, M13 entry and M13 separate departures through ordinary synthetic application input. The E4B report records a native frost, heat/Poise payoff, Core follow-up and conventional victory on the edited map; the route chain reports all eight continuation drivers passed. M13 earned all 13 receipts, the paired physical lift and CP7/8/9, while preserving M12's 22 receipts. The E3 WallRunner completed its physical route with the parasite gait in all 11 sampled traversal frames; the E4 runner's attempt was cancelled in this run, so it is not a second wall-route completion claim. This is one fresh regression pass; physical keyboard/mouse, sound, packaged execution, explicit final-checkpoint reload and rendered quality still require separate evidence. It does not change the 68.75-point slice estimate by itself.
- **Ammo-sensitive replay:** that second fresh run spent almost all Cinderline rounds clearing E1, even after collecting a 63-round pickup. The E3 firearm-only validation pilot entered with one round and no reserve, then stopped when it found no reachable matching pickup. `E3AmmoMeleeFallbackMainhand-20260923-022525-3f2e54b9` publicly reloaded that earned E3 checkpoint, completed the normal retry-terminal hold, selected Velkorran through the ordinary weapon wheel and main-hand click, and verified it became wielded with Tarrik's light and heavy melee grants. This establishes a player-operable melee fallback, though it does not prove an E3 melee victory or rule out ammo supply tuning. Do not count the second replay as an E3 victory. The first fresh run above did earn E3 and M13.
- **Companion attack path:** `CurrentE4ATarrikContactEarned-20260923-023621-42e0778f` publicly restored an earned E4A checkpoint, used the authored retry, and passed the ordinary Selene Axiom/link/handoff route. Tarrik drew Velkorran but stayed outside its 180 cm attack range through this roughly nine-second phase; no attack or damage is claimed from that short window. `CurrentE4BSeleneCompanionContact-20260923-024029-d64b829a` then restored an earned E4B checkpoint and used the normal retry, weapon wheel and player shot. Tarrik dealt 21 health damage to a living WallRunner; 0.796 seconds later Selene's Verity Twin Blade montage was active and her native source receipt dealt 30 health damage to that same target. The passive observer had no errors. This proves a current-build draw→attack→health-damage path for Selene as companion, while sustained 15–25% encounter contribution and repeat Tarrik contact remain open. Both map hashes stayed at the baseline above.
- **Tarrik autonomous follow/attack replay:** `E2TarrikCompanionFollowCombat-20260923-060728-c7ea83a3` publicly restored E2 and used the normal retry, but its two companion observers found zero visible living companion samples over 60 seconds; E2 is therefore the wrong phase for this qualification. `E4ATarrikFocusCombatAudit-20260923-061207-8393936f` restored earned E4A and sampled Tarrik closing from 30.1 m to 0.86 m, drawing Velkorran, playing the authored sword light montage, and dealing 43.48 nonfatal health damage to a Linkbound after 7.56 seconds. That replay requested FocusTarget, but its command-state readout remained Idle and the request has no accepted-command receipt, so it is not proof of a successful explicit command. `E4ATarrikAutonomousCombatAudit-20260923-061443-2b49bcea` repeated the public load/retry with **no player or companion combat input**. Tarrik autonomously focused a Linkbound at 3.77 seconds, closed from 30.1 m to melee range, drew Velkorran, played the same light montage at 5.17 seconds, and dealt 43.48 nonfatal health damage at 5.56 seconds. Both passive animation/hit-path observers had zero errors. This establishes a current-build autonomous follow→draw→melee→actual damage path; it does not establish sustained 15–25% contribution, repeat-hit cadence, or the validity of a FocusTarget command in this room.
- **Sustained Tarrik companion correction:** The earlier passive probe stopped at its first damage receipt, concealing later stalls. `E4ATarrikSustainedAutonomous-20260923-091446-31314fbf` held earned E4A for 45.13 seconds with no combat input: Tarrik drew and played one light sword montage but spent 253/391 samples focused on the encounter-held Elite, made no damage receipt, then died. After autonomous focus was changed to prefer ordinary hostiles over encounter-held targets, `E4ATarrikPreferredTargetReplay-20260923-092143-0caa4de8` spent 599/668 samples on Linkbounds and only 13 on the Elite, but recorded no swing or damage. Its nearest Linkbound stayed beyond the 180 cm sword range until 9.80 seconds, after the eight-second scope-start allowance had closed; the no-player-damage contribution budget then blocked all attacks. The opening allowance now starts at first attackable combat focus rather than being consumed while the companion traverses from spawn. The UE 5.7 editor build and all 17 `ProjectVelkorran.Campaign.Companion` tests passed (`CompanionTargetWindowIsolated-20260923-092939`), including a new native focus/fallback/window regression. On the rebuilt editor, `E4ATarrikOpeningCombatReplay-20260923-093136-d2ea6f4a` publicly loaded the same earned checkpoint, used ordinary retry input, and provided **no player or companion combat input**. Tarrik drew, played the authored sword montage for 40 sampled frames, dealt 43.48 health to Linkbound 0 and 32.26 collateral health to the Elite at 8.34–8.36 seconds, then hit Linkbound 0 again for 9.72 fatal health at 11.33 seconds. The allowance closed at 13.05 seconds and no further damage was recorded before Tarrik died at 34.8 seconds. All three passive observers had zero errors; M12 stayed at SHA-256 `23555516889B0078EA29323F375B1E2D43E5EDF1132AD903E68989BB51DA61F1`. This proves repeat autonomous authored contact in the opening phase, while 15–25% contribution during active player combat, Selene repeat cadence, companion survival tuning, and a completed E4A encounter remain open.
- **Selene sustained-contact boundary:** `E4BSeleneSustainedContact-20260923-093627-b111b15b` publicly restored earned E4B, used its ordinary retry and one normal Tarrik firearm shot, then released all combat input while observing for 75 seconds. The player shot dealt 21 health to WallRunner 0 at 1.77 seconds; Selene drew Verity, played 11 sampled Twin Blade attack-montage frames, and dealt 30 health to that target at 7.20 seconds. The opening allowance closed at 8.36 seconds. No second Selene hit occurred, but the single player shot cannot fund another meaningful companion attack under the authored 20% contribution cap after her 30 opening damage; the WallRunner also stayed outside her sword range for the rest of the observation. The WallRunner survived at 2.2 health, and Selene remained alive and focused without an observer error. This pass confirms her draw→attack→damage path after the shared native change, but does **not** qualify repeat Selene contact during sustained player combat. The M12 hash remained unchanged.
- **E4A progression after the companion change:** `E4ACompanionNormalSeverRegression-20260923-094306-356db79f` publicly restored the earned E4A checkpoint, used ordinary retry, then ran the normal Selene Axiom/precision driver while the passive companion observer remained active through handoff. Both native Weaver sever receipts, the WallRunner release, protected-survivor check, Tarrik handoff, and active E4B entry passed. Tarrik dealt one native companion hit in the 9.22-second observed phase; the player dealt three, and the observer recorded zero errors. M12 remained at SHA-256 `23555516889B0078EA29323F375B1E2D43E5EDF1132AD903E68989BB51DA61F1`. This checks the ordinary route after the change; it does not prove the later E4B or M13 route, nor the case where the player waits until after a companion has killed a Linkbound before attempting the sever.
- **Selene sustained E4B contribution:** Three later earned E4B retries passed native frost/heat/Core progression and conventional victory with protected people alive. Selene dealt 114.55, 100.75 and 120.91 health damage across 6, 9 and 10 native hits, representing 17.2%, 15.2% and 18.2% of combined protagonist damage; Verity Twin Blade appeared in 44, 63 and 43 passive montage samples. All observers had zero errors. This closes the specific sustained-active-combat contribution gap for these earned E4B runs, while physical-device feel, close-up pose, non-E4B cadence and client replication remain open. A normal 10.84-second E4A replay passed progression but Tarrik drew Velkorran and landed no hit before handoff, so his short-phase contribution remains variable. See [the detailed companion report](Validation/AurelionSeleneCompanionCombat-2026-09-23.md).
- **Z12 berth-wall source and rejected placement:** A new generated reference preceded two Blender-authored 4.15 m service-wall bay meshes, both verified by clean FBX round trip and imported into Unreal. Three unsaved M13 previews tested rear/side placements and a wall wash. The full rear enclosure reduced the open remnant/Wound vista and read too flat from the saved concourse, so no new map actors or lights were saved. The meshes remain reusable prototypes; the M13 map hash stayed `CE18E403...27250`. See [the source/engine review](Validation/AurelionZ12BerthBulkheadPrototype-2026-09-23.md).
- **Z12 open vista correction:** After dropping Higgsfield, a new [reference and custom Blender spire/bridge kit](Validation/AurelionZ12OpenVista-2026-09-23.md) restored the open-remnant design direction. Three meshes passed Blender round-trip and were imported into Unreal; their first player-window placement was too obscured by shuttles/canopies, so none was saved to M13. Two noncolliding red/white scenic plates were saved instead and passed a fresh M13 map load. Two public earned CP9 loads preserved the 35-entry route, inventory/resources, companion state, released input and one HUD. The window captures show improved faction color separation, but vista occlusion, ship quality, target-PC performance and final art acceptance remain open. M12 stayed at `64A4517B...2B844A4`; M13 now hashes `F50D390F...1726503`.
- **Fresh-route reliability after the vista save:** `PostVistaFreshRoute-20260923-222413-f988be3a` started from an ordinary CP0 M12 entry and failed in E1 after a fourth fatal hit exceeded the pilot's three native retries. The three observed recoveries succeeded and resumed new E1 attempts; the deaths occurred at 61.3, 115.6, 147.8 and 176.6 seconds. The read-only report records 79 incoming damage receipts, 52 rocket reactions, two cover attempts and one occluded-target switch. The M12 map, security-drone asset and E1 input-driver SHA-256 values exactly match the earlier `FreshM12M13PostZ12Roof-20260923-194506-6461e714` run, which passed E1 after three recoveries and had 11 cover attempts; this is a repeatability gap, not evidence that the M13 vista edit changed E1. No enemy pressure was retuned from this single failure. Compare cover selection/exposure and a physical-input E1 run before attributing the cause; the subsequent full-route pass on the saved register map is recorded below.
- **Custom Z12 service-register wall variation:** [Blender source and engine review](Validation/AurelionZ12ServiceRegister-2026-09-23.md) replaced eight lower visual side-wall coffers beside the two M13 berth windows with a rare 28,488-triangle Aurelion service register. Clean FBX round trip, guarded map save/reload, independent fresh-map load and two public earned CP9 recoveries passed; native wall collision and the red/white views remain. The dark register material and room lighting still need a higher-fidelity finish, player movement and target-PC performance coverage. The saved M13 map is now `63B3EE34...94B3814B`.
- **Fresh route on the saved register map:** `PostRegisterFreshRoute-20260923-224828-a8194366` passed fresh M12 entry and E1 without death, E2, E3 entry/rescue, E4 entry/A/B, actual M12→M13 travel, M13 entry and separate departures through ordinary synthetic application input. The M13 report retains 35 receipts and all ten scenes, with both map hashes unchanged. This is one end-to-end pass after the new Z12 wall placement; the earlier E1 four-death failure remains a repeatability problem. Physical keyboard/mouse, audio, packaged execution, target-PC performance and close player movement along the new wall remain open.
- **Exact-route CP9 recovery:** `PostRegisterEarnedCP9-20260923-230808-e9aace50` loaded both banks earned by the fresh register-map route through public checkpoint requests. Both M13 loads retained 35 receipts, Tarrik/Selene state and separate departure positions, with released input, a single visible Tarrik HUD, two checkpoint refreshes and unchanged map hashes. Both frames were visually inspected without a duplicate HUD or cinematic overlay. See [the Z12 service-register review](Validation/AurelionZ12ServiceRegister-2026-09-23.md).
- **Current E1 repeatability probe:** `E1PressureRepeatCurrent-20260923-231926-81a856e4` started fresh visible M12 PIE with Aura disabled and ordinary Enhanced Input. E1 failed on a fourth death at 113.7 seconds after three successful native recoveries. The passive observer completed without errors and recorded 113 enemy gunshots (40 reported damage), 42 rockets and 111 one-second samples. The driver recorded 58 incoming damage receipts, three cover choices and three exposures after reaching them. Both mission asset sets remained byte-identical. This reproduces the E1 reliability gap from `PostVistaFreshRoute` on the current build; it does not isolate an enemy defect from the pilot's aim, rocket reactions or cover choices. Do not retune pressure from this result alone. Compare a physical-device run and a projectile-windup/dodge timeline before changing the authored four-active-drone wave.
- **Z12 berth composition gate:** A [six-frame unsaved UE comparison](Validation/AurelionZ12BerthSightline-2026-09-23.md) tested current, recessed and nose-out positions for both scenic shuttles. Simple movement exposes slightly more remnant color but degrades craft scale and does not clear the canopy/mullion obstruction. Neither ship transform was saved; both map hashes remained unchanged. The next custom mesh needs to revise the architectural view opening rather than rely on ship placement alone.
- **Z12 raised edge rejection and custom oculus:** A measured eight-bay Blender raised roof-edge preview was rejected after UE player-height views showed only a dark band over the berths; it did not recover the remnant vistas. The prototype was archived outside shipped content. A separate [Blender-authored sovereign oculus](Validation/AurelionZ12SovereignOculus-2026-09-24.md) replaced two visual-only concourse roof bays over the Dominion/Reformation approaches, using the existing Aurelion material family. Clean FBX round trip, reviewed before/after engine frames, guarded save/reload and independent fresh M13 inspection passed. Native collision and navigation stayed unchanged; M13 is now `32D6FCAF...DF576E`, M12 remained `64A4517B...2B844A4`. A fresh ordinary-input CP0→M13 route on that exact map passed all eight continuation stages, ten M13 scenes and 35 receipts without an E1 retry. Two public CP9 reloads from the banks earned by this exact route preserved the final state and one HUD; both frames were visually reviewed. The motif reads when looking up but does not solve the exterior vista or the broader 90% art goal. E1 repeatability, physical-device play, audio, packaged execution and target-PC performance remain open.
- **30 fps checkpoint reliability:** `CheckpointSoak100x30Current-20260923-024342-3748f439` completed 100/100 fresh M12 PIE starts and public CP0 save loads in one editor process at `t.MaxFPS 30`, with no failed cycles, ensures, fatal errors or Python errors. Load time was 4.27 seconds median, 4.50 seconds at p95 and 4.72 seconds maximum; the first cold PIE readiness took 31.56 seconds, while subsequent readiness was 1.08 seconds median. The editor exited normally. This qualifies repeat CP0 reload at a second frame profile; earned late checkpoints, long-session memory behavior and packaged-build reliability still need separate evidence. Neither map hash changed.
- **Current-build earned CP9 recovery:** `CurrentEarnedCP9ReloadFromM12-20260923-090541-0a894308` copied the two exact CP9 banks from the September 23 fresh M12-to-M13 route into an isolated PIE user directory, then requested two ordinary `LoadSlot(CHECKPOINT, 0)` operations. Both completed in new native M13 worlds with all 35 journal entries and earned evidence unchanged, stable player/companion identities, inventory/resources, lift state and separate exit positions. Each settled for four seconds with movement/look released, one visible Tarrik HUD at 100 health and no paused state. The two rendered `reload-0.png` / `reload-1.png` frames were reviewed: neither showed a duplicate HUD or cinematic overlay. Both map hashes were unchanged. A first attempt started directly in M13 and was stopped before loading any bank because initial convergence requires the previously played kit and companion entry anchor; the successful run bootstrapped M12 and let the public save load travel to M13. This is earned checkpoint recovery evidence, not another uninterrupted route, packaged-build or full art sign-off.

## September 24 Z11 gallery follow-up

- **Gallery wall clarity:** [The authored Z11 wall pass](Validation/AurelionZ11QuietWall-2026-09-24.md) replaces 146 overlapping/closely repeated noncolliding stock panels with twelve custom bays and hides a visual-only bench that contradicted the manuscript's one table and six chairs. Before/after Unreal views and save/reload checks preserve the native structural wall, central opening, west observation view and gameplay actors. A fresh CP0 M12→M13 normal-input route on the final east-wall map passed all eight continuation stages and 35 total receipts; its same-session public CP9 reload preserved final state in a new world. The pale structural pier, freestanding kiosks, dark ceiling, physical-device feel, audio, target-PC performance and broader 90% visual alignment remain open.
- **Gallery ceiling clarity:** [The custom Z11 ceiling pass](Validation/AurelionZ11ObservationCeiling-2026-09-24.md) replaces 24 noncolliding stock roof tiles with a measured 6 × 4 Blender coffer kit and leaves separate floor art, collision, one table/six chairs and the west view intact. Fixed-camera before/after Unreal captures and guarded save/reload passed. A fresh visible CP0→M13 normal-input route on the exact saved map passed all eight continuation stages and 35 receipts after two legitimate E1 retries; one same-session public CP9 reload preserved the completed departure state in a new world. The dark recess lighting, pale structural pier, white kiosks, physical-device feel, audio, target-PC performance and 90% alignment goal remain open.
- **Companion immune-target swing:** [The E4A Tarrik replay](Validation/AurelionTarrikInvulnerableFocus-2026-09-24.md) reproduced a Velkorran swing on an Elite carrying `Narrative.State.Invulnerable` with no companion damage. Native targeting now defers fully immune hostiles behind ordinary targets and prevents curated attacks on an immune fallback; all 18 companion automation tests and a same-checkpoint visible PIE replay passed. Tarrik still drew Velkorran and focused the Elite for defense, but no longer played the wasted montage. The short replay recorded no Tarrik damage, so the 15–25% contribution target still needs broader live coverage.
