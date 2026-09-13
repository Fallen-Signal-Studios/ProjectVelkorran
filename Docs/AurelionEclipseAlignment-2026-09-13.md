# Aurelion Eclipse implementation — 13 September 2026

Work in progress. **90% visual/gameplay alignment is not established.** This pass targets the
M12/M13 slice against the 14 August TDD; it does not redefine the full campaign as the slice.

## Current Git basis

Fetched origin. The latest engineering branch ends at `0f7fb6e9`; `main` and `origin/main`
remain at `9bfb44e1`. The implementation branch is `codex/aurelion-tdd-content-20260913`.
The latest commits close C1 protagonist state partitioning and PC01–PC03 native payload work.
Those source systems are retained. PC04 remains an evidence/harness issue; the current
reconciliation does not establish a new source defect to rewrite.

The existing user-edited M12 map, enabled Aura plugin and untracked Cues were present before
this pass. Generated validation data is under `Saved/Validation/Aurelion`, outside version control.

## Implemented presentation

| Role | Mesh | Animation |
|---|---|---|
| Linkbound | Existing Parasites `SK_Parasit` | Existing `ABP_EclipseLinkbound` |
| WallRunner | Parasites `SK_Parasite_Spider` | Copied native character AnimBP with matching spider sequences |
| Weaver | Parasites `SK_Alfa` | Matching alpha sequences |
| Elite | Parasites `SK_Fat` | Matching heavy parasite sequences |

The three adapted AnimBPs compiled with zero errors and warnings. Each uses a matching skeleton
and copied blend space, retaining the existing locomotion graph and montage slot. An editor-only
helper performs reference replacement; gameplay ownership remains native. Appearance
and mesh transforms were saved with backups. Elite Core retains its native zone settings and
uses a verified torso bone from the new skeleton. Physics-hit and consequence qualification
still requires live combat.

A 90-second real PIE observation recorded all 12 Eclipse instances, loaded matching meshes,
and changing bone poses for the three changed roles. Ejected-camera review showed the spider,
alpha and heavy parasite visibly on the floor. This proves loaded/animating presentation,
**not movement, attack, death or mission completion**. No player/enemy actor was teleported.

Evidence: `Saved/Validation/Aurelion/EclipseAnimation/prepare-presentation.json`,
`assign-presentation.json`, `visual-pie.json` and the `VisualPIE-20260913-0500/Editor.log`.

Four Parasites attack montages, four native `SovMeleeAttackDefinition` assets and four
`SovGameplayAbility_Melee` Blueprint subclasses are now saved. Per-role ability configurations
replace only the audited legacy punch grant; other grants and Elite startup effects are retained.
Native definition validation succeeded. Timing remains provisional pending visible strikes and
accepted damage. Linkbound now uses its visible authoritative mesh, so montage playback and
native sweep bones refer to the same mesh instead of independently animated duplicate bodies.

Eight existing Z06/Z07/Z08 rect lights were set to 1,800 lumens without moving or recoloring them.
The exact pre-edit user map is backed up under `EclipseAnimation/before-readability`.
Ejected-camera inspection is recorded below; player-route visual qualification remains pending.

The `EclipseRoute-20260913-051706-556fbc0b` fresh entry passed at 51.5 seconds: CP0,
ordinary arrival hold, Cinderline selection and E1 activation (four active enemies plus two
reserved drones). Its E1 combat continuation failed at 157.9 seconds when Tarrik died,
after defeating four of six drones. The native failure/recovery presentation was observed;
no retry or healing was issued by the driver. The editor then exited cleanly. This is not
a complete-route pass.

That retained run also exposed a modal `ExecutePythonScript` window that prevents normal
editor UI input. Retained launches now use Unreal's deferred `PY` console command;
non-retained process execution is unchanged. The repeat run will verify UI interaction.

`EclipseRouteRepeat-20260913-052301-20a79e73` verified the non-modal launcher and actual
console interaction. Its entry inspection stopped on four still-loading E2 Enforcers at
39 seconds (no structural failures). The driver now records bounded full-roster readiness
before walking into the active E1 encounter; subsequent passing entries are recorded below.

The successful physics export audit found matching montage skeletons for all four roles.
It also found that `SK_Fat_PhysicsAsset` has a torso body on `CATRigSpine1`, not Spine2.
Elite Core was corrected to that actual physics body and recompiled/saved, preserving all
other zone fields. The original and corrected values are in `EclipseAnimation/core-body.json`.
This is an authored collision-binding check, not a successful live Core hit.

## Failures retained

- The visual session later crashed in background texture compilation. The log reports a failed
  768 MiB allocation and an insufficient Windows paging file. Clean PIE exit is unqualified.
- The first native melee authoring attempt correctly refused an unexpected Linkbound grant.
  The actual grant is `GA_Melee_Punch_Linkbound`; the other roles use `GA_Melee_Punch_Unarmed`.
  No definition was saved by that failed attempt. A second attempt stopped on the Python
  binding of the native validation result, also before saving. Both reports are retained.
- The read-only lighting audit initially used an unavailable Python accessor. It was corrected
  to `get_component_by_class`; the successful audit records actual grants and fixture values.
- Earlier weapon selection failures occurred before any mouse input while Narrative was still
  loading. The entry driver now waits for that native gate before starting the unchanged
  wheel timer, within the original bounded entry stage. A new route pass is still required.
- The roster's strict placement tolerance previously failed on about 1 cm of E3 movement;
  the separate final startup census passed. Neither result is relabeled.

## Formation bootstrap correction

`EclipseRosterReadyRoute-20260913-052941-8479f8c4` reached full observable roster readiness
at 57.7 seconds, but E1 rejected entry with "Formation gates require an exact registered link
and an earlier-wave required command source." The native fresh-link bootstrap stopped polling
after 30 game seconds. This could permanently abandon a formation whose members were still loading.

A focused automation regression keeps a member unready beyond 31 game seconds, then requires
activation through the existing timer after readiness, without calling activation manually.
Its initial version failed before and after the proposed 180-second window because the fixture
did not advance `GFrameCounter`; Unreal's timer manager consequently ticked only once. Those
initial results are retained but are **not valid timer-regression evidence**. A subsequent
time-only clock version also skipped timer execution. The other 15 enemy-role tests passed.
The fixture now follows the scoped-frame pattern, explicitly ticks the timer manager, and
requires a separate counter to prove at least 310 actual timer callbacks before evaluating
the link. With that check, `FormationTimerBefore` failed the two activation/source expectations
on the original 30-second runtime; `FormationTimerAfter` passed **16/16 enemy-role tests**
with the 180-second window, including the delayed-readiness regression. The matching Win64
Editor build succeeded (`FormationTimerFixBuild.log`).
The exported test report records nine successes and seven successes with warnings,
zero failures and zero unrun tests; this is not a warning-free native test run.

The bounded bootstrap window is now 180 seconds. Native admission, restoration and used-link
identity checks remain unchanged. This is the only runtime source correction in this content
pass, motivated by the observed in-engine failure.

`EclipseFormationFixedRoute-20260913-055212-7390928b` then passed fresh entry
in 41.828 seconds after the entire roster became ready. E1 combat failed after
215.297 seconds: Tarrik died after defeating four drones, with Drone4/Drone6
still alive. Authored asset hashes remained unchanged. The pilot changed targets
60 times as the drones moved. Its next revision retains its living visible target
instead of re-ranking distance every frame; this is an input-driver correction,
not an enemy balance change or a gameplay pass.

`EclipseStableTargetRoute-20260913-060300-e53cdbb1` passed entry in 46.046 seconds.
Target retention reduced selection to two targets, but standing fire still ended
with Tarrik's death at 41.735 seconds. Asset hashes were unchanged. The next pilot
revision alternates ordinary lateral movement during clear, in-range firing and
reloads; it does not move transforms, change damage or grant recovery/resources.

`EclipseMovingFireRoute-20260913-060620-e197f3f7` passed entry in 50.344 seconds,
then failed at 77.156 seconds on Tarrik's death. The pilot visibly strafed, but
its capped mouse delta lagged behind orbiting targets (7.14 degrees error at the
last sample). The next revision increases ordinary look-input responsiveness
and allows firing within three degrees. Native spread, collision and hit acceptance
still decide every shot. This is pilot calibration, not physical mouse qualification.

`EclipseResponsiveAimRoute-20260913-060959-e0155103` reached combat and defeated
Drone2 and Drone1, but failed on death at 32.829 seconds. The pilot had never used
the authored `/Game/Input/IA_Evade`. The next revision requests that normal action
briefly every three seconds while moving; native stamina, cancellation and ability
admission remain authoritative. Its report also records the pilot source hash.

`EclipseEvadeRoute-20260913-061357-cd0c657b` defeated five drones and reduced
Drone6 to 46 health, then failed on Tarrik's death at 110.922 seconds. One manual
G press was attempted when the HUD showed 41 Echo and the ability ready; the later
HUD showed 45 Echo with no confirmed spend. `manual-input.json` records this as
an attempted input, not validated activation/payload. All failed attempts remain
separate. A same-pilot repeat follows without the manual G input.

`EclipseEvadeRepeat-20260913-061755-e9fb5a73` used the same pilot without manual
input and failed at 62.391 seconds after three drone defeats. These attempts do
not demonstrate a reliable E1 pilot, a fresh route pass, or an unambiguous source
damage defect. Further work needs combat observation that distinguishes pilot
limitations from actual encounter tuning; enemy damage and victory gates have not
been weakened to obtain a pass. The retained editor was closed normally.

Second fetch confirmed `origin/main` and the reviewed engineering head unchanged.
The edited Python scripts and PowerShell launcher parse successfully; `git diff
--check` has no whitespace errors. Work remains uncommitted on the implementation
branch. The pre-existing Aura descriptor change and Cues files remain separate.

After that failed attempt, the ejected editor camera visually reviewed WallRunner,
Weaver and Elite with the revised lighting. The spider silhouette and feet were
readable against the breach wall; Weaver and Elite had distinct Parasites silhouettes
and visible floor contact, but their front surfaces remained dark in places. This
was camera-only inspection, not player traversal or proof of moving foot contact.

The final regression processes explicitly excluded the editor-only Aura plugin. It had spawned
a separate headless Unreal process that locked the project DLLs and caused a linker failure;
that exact background process was stopped and the build then succeeded. The project descriptor
was not changed. The launcher now has an explicit `-DisableAura` option and records that choice.

## Remaining qualification

### Cinderline channel correction found by live observation

`AurelionDamageObserved-20260913-062359-f6c3d508` added a read-only native
damage/exertion observer. It recorded actual 24-stamina evade spends and Kinetic
12-damage drone fire. It also recorded Cinderline hits on **Edge**, contrary to
the TDD section 6.4 damage-channel table (Cinderline is Thermal). Exporting the
actual weapon/ability/effect chain identified the shared `GE_WeaponDamage` Edge
asset tag. This is evidence of an authored mismatch, not a damage-code defect.

The project now has `GE_CinderlineThermal` and `GA_CinderlinePrimary` under
`/Game/Abilities/Tarrik`. They copy the existing effect/attack behavior and change
the effect's asset channel to Thermal. Only the Cinderline regular/mainhand primary
grant changes. Damage values, other grants and shared Narrative assets are retained.
The exact weapon backup and successful authoring report are in that run's
`cinderline-authoring` directory. Two preflight Python reflection failures saved
nothing and remain recorded there. The kit setup script preserves this owned
primary when present. Live evidence follows below.

`CinderlineThermalRoute-20260913-063521-93203939` passed fresh entry in 67.453
seconds and confirmed 88 actual outgoing Thermal hits with zero Edge hits and
zero observer errors. E1 still failed on Tarrik's death at 83.813 seconds. The
observer captured the fatal 55-damage Heavy Kinetic/Thermal rocket after shield
break; Evade stamina spends were genuine. The next pilot adds normal Evade requests
for a nearby closing rocket with clear line of sight, rather than relying only on
its periodic evade. No projectile, damage, resource, or encounter state is written.

The Git ignore file now permits exactly the owned Cinderline weapon, effect and
ability assets; surrounding marketplace/template assets remain ignored.

`AurelionRocketReaction-20260913-064145-40255ca8` exercised reactive ordinary
Evade requests against nearby closing rockets with a clear trace. E1 failed at
48.625 seconds. Its final native damage transaction was a standard 12-damage
Kinetic burst after shield depletion, not the earlier Heavy rocket. The observer
retains exact damage transactions, actual stamina spending and the pilot's rocket
reaction samples. This remains failed gameplay qualification and does not support
changing damage or encounter rules. Further pilot work needs effective use of
cover and sustain, or a player-operated combat pass, before claiming E1 reliability.

The same run became stationary with a ramp between player and drone at 374 cm:
the pilot refused path-following inside 450 cm. It was explicitly stopped as
failed, with unchanged assets and no observer errors. The next pilot follows the
queried path down to its existing 80 cm arrival tolerance, still using ordinary
movement and capsule collision. This does not remove cover or allow shots through it.

### Outstanding live checks

`AurelionDynamicCover-20260913-065957-73078fcd` passed fresh entry in 57.407
seconds, E1 victory/secure approach/native Selene handoff in 137.390 seconds,
and the E2 continuation in 53.594 seconds. E1 reports unchanged assets and no
direct gameplay/resource/transform writes. This is the first successful opening
route with the current Thermal and Eclipse content. It subsequently passed E3
entry, E3 rescue/victory with both protected characters alive, and E4 entry.
E4A failed at 37.782 seconds because no eligible living enemy had a usable native
weak point. M13 remains unqualified, and one pass is not reliability.

The actual E4A audit found inherited mannequin matcher names (`head`, `spine_03`,
`spine_02`, `spine_01`) absent from the Parasites meshes. After stopping PIE,
the three owned Blueprint weak-point components were remapped as follows:

| Role | Existing head zone matcher | Existing torso zone matchers |
|---|---|---|
| Linkbound | CATRigHub002 | CATRigSpine2, CATRigSpine1 |
| WallRunner | CATRigHub002 | CATRigSpine |
| Weaver | CATRigHub005 | CATRigHub004, CATRigSpine |

Current component-space hub/spine positions and actual physics-body exports
support those bindings. Zone IDs, descendant matching, reveal settings and native
consequences are preserved; Elite Core remains unchanged. All three Blueprints
compiled and saved. Backups and before/after values are in the run's
`weakpoint-authoring` directory. Native hit/Echo qualification requires a new run.
Fresh startup inspection now rejects mesh-incompatible Eclipse weak-point bones
before the route begins.
`AurelionParasiteWeakpoints-20260913-071815-d00732a4` passed the new complete
startup contract with zero contract/inspection errors and fresh entry at 48.500
seconds. E1 passed again at 116.125 seconds, followed by E2, E3 entry/rescue and
E4 entry. The editor crashed at E4A startup before precision-hit qualification.
`route-termination.json` preserves hashes of the unchanged running report and log.
The log records a PythonScriptPlugin/CoreUObject MTAccessDetector ensure followed
by an access violation during garbage collection. The new damage observer had
retained borrowed delegate-property wrappers across defeated-NPC cleanup. It now
retains UObject owners, reacquires valid delegate properties and unbinds dead NPCs
promptly. This is an instrumentation correction requiring a repeat, not a proven
native gameplay defect. `AurelionObserverLifetime-20260913-072904-a2e8126b` is the
fresh repeat with that correction.

That repeat passed E1 at 150.813 seconds and E2/E3 rescue/E4 entry. The
automatic chain stopped on the E4A wheel's 60-second click timeout (operator
missed the click); its failed report is preserved. A separate `E4A-WheelRetry`
continued from the unchanged native encounter, selected Axiom with a visible
left click and passed at 23.015 seconds: five real weak-point awards, two native
Weaver sever receipts, WallRunner release, Tarrik handoff and ArenaEntry save.
Both spiders completed their assigned wall routes (12 and 13 traversing samples).
The corrected damage observer completed without errors through E4B, recording
15 Linkbound, 5 Elite and 2 WallRunner outgoing transactions; the WallRunner
record includes 18 applied shield damage and 10 applied poise damage.

`E4B-Continuation` failed at 42.000 seconds. MovePartner was accepted, but the
elite moved outside the overlapping heat/control reach intervals while the
pilot waited to set up frost. The log then records a Dominion stretcher patient
death and the encounter leaving its active state. No Thermal Fracture, Core
follow-up or M13 pass is claimed. The idle interval around manual continuation
also prevents treating this as an uninterrupted route qualification. Remaining
work includes keeping the protected roster safe while executing the actual
frost/heat sequence; no range, health or proof bypass was introduced.

The read-only damage observer installed after E3 recorded **22 Linkbound and
8 Elite outgoing native damage transactions**, including applied shield damage
and poise damage. It completed without errors when PIE ended. The pose observer
recorded Linkbound/WallRunner movement and Linkbound/Elite attack montages.
This supplies narrow movement and accepted-hit evidence; WallRunner/Weaver
attacks, visible timing, precise floor contact and Core-hit consequences remain open.
The Axiom main-hand choice was made with one real left click in the visible wheel.

Player-view observations also retain visual issues: the nearby E4 monsters can be
too dark to read against the crucible floor, and the native subtitle panel wraps
some words across lines. These are not qualified as final presentation.
Three attempted read-only Python subtitle geometry inspections hit unavailable or
protected widget properties. Their failed reports and final attempted script are
preserved in the interrupted run. No widget or subtitle source was changed.

Subsequent native investigation reproduced the narrow dialogue panel: Slate's
automatic wrapping takes the smaller of explicit wrap width and last painted
width, while these HUD panels size themselves to their contents. A short line
therefore trapped later text in a narrow column. The isolated correction disables
automatic wrapping on subtitle/caption text and retains the existing explicit
safe-area limits. `ProjectVelkorran.UI.Accessibility.DialogueWidthRecovery` paints
a short line on both actual HUD text widgets and then checks long-line expansion,
width bounds and height. It fails on the original implementation in
`SubtitleBefore/Report` and passes without warnings in `SubtitleAfter/Report`.
`SubtitleFixedBuild.log` records a successful UE5.7 Editor build. This does not
qualify pagination, all viewport sizes, localization or final cinematic layout.
The runtime correction and regression are isolated in `02926f96`.

`DialogueWidthPIE-20260913-075130-933524d5` did not reach the HUD preview:
entry failed while placed Enforcers and an E2 drone were still loading. The
entry-only path had skipped the roster readiness wait used by combat runs.
The validator now uses the same bounded native readiness phase for both paths;
syntax validation passed, but its PIE repeat remains pending. The desktop tool
could capture the stopped editor but repeatedly failed to activate it, including
an attempt to dismiss an unrelated old crash dialog without sending a report.
`preview_dialogue_width.py` is an explicit synthetic HUD-only preview, not yet
executed; it supplies no story/gameplay qualification and writes no content.

That rebuild also exposed a C4459 variable shadow in the Eclipse editor helper
when compiled in its normal unity group. Commit `bf72cea7` uses a distinct local
name and was verified with that grouping, preserving authoring behavior.

`AurelionControlledBursts-20260913-065420-beae6009` failed at 80.828 seconds
on player death after two drone defeats. The final pilot state was stationary in
cover with 32 rounds loaded and 122 in reserve. Cover selection did not recheck
exposure as flying enemies moved. The next validation driver rechecks from the
pawn before waiting for shield recovery and resumes ordinary movement/fire when
exposed. It also permits approaching an existing matching pickup when ammunition
is empty. Neither change supplies resources or modifies gameplay state.

The isolated native bootstrap correction and timer regression are committed as
`265b6a63`; presentation and route qualification remain separate work.

`AurelionCoverSustain-20260913-064824-6d187350` used queried reachable cover
and attempted ordinary approach to matching ammo pickups. Tarrik survived at
100 health, but E1 failed at 200.547 seconds on ammunition exhaustion after four
drone defeats. The observer recorded 110 outgoing hits, 32 incoming damage events,
and zero errors. There were no matching reachable pickup approaches. No ammo was
granted or pickup spawned by the pilot. This run proves neither E1 nor sustain
collection. The next pilot tightens firing tolerance to 1.2 degrees with faster
ordinary look response and avoids re-querying cover while already waiting there.

1. Compatible native melee data/grants, visible strike timing and actual accepted damage.
2. Native chase/wall route with moving poses, no floor penetration or sliding; death and Core hits.
3. Player-view breach/crucible readability during combat (ejected-camera review completed).
4. Fresh M12-to-M13 route, Selene Null Pulse/link severs, handoff and checkpoint repeat.
5. Win64 package, performance/memory and audio/accessibility testing.

The older full-TDD planning estimate was about 37%, with a 31–43% range. This pass supplies
new narrow in-engine evidence; it does not justify replacing that estimate with 90%.
