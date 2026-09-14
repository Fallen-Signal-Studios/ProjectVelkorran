# Companion equipment and combat investigation

User report: Selene/Tarrik have animation problems, do not draw their weapons,
and do not attack while following.

The retained normal-order `SustainBindingsValidated-20260913-124036-262f254e`
PIE session reached the actual Selene-to-Tarrik shared handoff. A read-only
inspection found the living Selene proxy using the expected MetaHuman visual
and Narrative NPC controller, but `get_weapon()` returned None. Both authored
companion definitions had empty default inventories. The screenshot also showed
Selene standing unarmed beside Tarrik on the descent. This is evidence for the
missing equipment, not a diagnosis of every animation symptom.

`configure_aurelion_companion_equipment.py` now saves one existing Velkorran for
Tarrik and one existing Verity for Selene in their companion NPC definitions.
Both are ammunition-free melee weapons. Player inventories, progression,
resources, faction, appearance and native contribution limits are unchanged.
The full-route authoring recipe uses the same function so rebuilding content
retains these loadouts. Stopped-editor readback and asset validation passed;
`Saved/Validation/Aurelion/CompanionCombat-20260913/equipment.json` records them.

The initial source investigation found that the companion context command only
moves toward its leader/hold/defend destination. It does not request weapon draw
or move into a selected attack's native range. The mission originally curated
only defense and unarmed punch; the proxy copies only classes present in the
outgoing ASC. Simply adding an armed attack class can still fail when that
player weapon was holstered at handoff, because its item grants are then absent.
The pacifist baseline deliberately leaves contextual command ownership to the
native companion activity; replacing it with unrestricted combat AI would bypass
the intended curated kit and damage contribution policy.

The correction now recognizes allowlisted abilities supplied by a weapon in the
actual outgoing inventory even when holstered. The snapshot marks those as
item-owned grants; native weapon draw supplies them. Direct unlocked ASC grants
retain their original copy behavior. A const weapon-kit accessor reads existing
data without mutating it. M12 and M13 profiles now include the existing primary
melee attack alongside defense/punch.

The context command draws permitted equipped weapons through SetWieldState,
approaches inside native candidate range within the existing ten-metre defense
area, respects explicit holds, and stops its owned move before attacking.
Ordinary attacks finish through their native ability/montage lifecycle rather
than being forcibly cancelled after 1.5 seconds. Timed guard/deflection release
is retained. Failed defense activation no longer delays ordinary attacks, and
owned stale focus is released when no hostile focus remains. Native damage,
costs, cooldowns, tokens, protected-target checks and contribution limits remain.

UE 5.7 Development Editor rebuilt successfully. All 12 tests under
`ProjectVelkorran.Campaign.Companion` passed in
`Saved/Validation/CompanionCombatNative/20260913-131327-c74b2727`;
source/report coverage matched 12/12, with five warnings. The expanded native
inventory regression verifies holstered item recognition, exclusion of locked
or unlisted choices, no extra player grant, and distinct direct/item ownership.
The initial attempt failed because its fixture supplied a wielded item and a
non-Narrative ability; the fixture was corrected before the passing run.

Fresh normal-order runtime observation is running in
`Saved/Validation/Aurelion/CompanionCombatValidated-20260913-131444-1b27325d`.
Both actual handoffs now have runtime evidence of their correct wielded weapons:
Tarrik/Velkorran and Selene/Verity. The passive observer recorded native primary
attack candidates as available, but no attributed companion damage. This does
not establish attack animation or damage acceptance. The bug remains open.

The retained run passed E1, E2, E3 entry/rescue and E4 entry. E4A's first input
driver timed out waiting for the manual Axiom main-hand assignment; the guarded
`E4AInputRetry` passed in 20.297 seconds after clicking the real wheel action.
It earned the actual nineteen-beat journal and Tarrik handoff. No encounter,
inventory or campaign state was reset to retry the input.

E4B first timed out during FrostSetup. A diagnostic retry recorded native
interaction focus moving from the control to Elite/Linkbound NPC interactables,
with the hold countdown cancelled. A held input cannot start another hold after
that cancellation. The pilot now records the hold trace, releases/re-aims when
focus/admission is lost, and permits at most three retries per control.
`E4BFocusRetry/e4b-input-continuation.json` passed in 12.797 seconds, using one
FrostSetup reacquisition. Real native frost/heat/payoff transactions, Core
follow-up, conventional kills and the ThermalFracture victory were observed.
All seven protected participants survived at 100 health. The earlier failed
reports remain separate. This qualifies the baffles in this encounter run;
it is not packaged, audio, performance or broad visual acceptance.

The post-victory continuation passed in 74.953 seconds: quarantine, recognition,
CP6, durable travel checkpoint and actual M13 arrival retained journal, logical
companion identity, inventory and resources. See the same run's
`m13-entry-input-continuation.json` and `e4b-victory.png`.

M13 then passed in 286.734 seconds through contrary positioning, both real
handoffs, all scenes, native lift, separate departures and CP9. The report
`m13-input-continuation.json` confirms unchanged content hashes. PIE was stopped
only after the driver passed and released its gameplay references. This is a
complete normal-order route with separately recorded input retries, not an
uninterrupted first-attempt pass or packaged qualification.

The next native correction releases Selene's still-accepted frost-anchor hold
after a verified fracture payoff, returning her to Regroup. It preserves a
newer command or different leader. The previous implementation left that hold
active through conventional combat, which intentionally suppresses automatic
target selection. A regression covers release after real payoff and preservation
of a newer hold. UE 5.7 Development Editor rebuilt successfully (28.65 seconds),
and all five thermal-fracture tests passed, including that regression, in
`Saved/Validation/CompanionFrostRelease/20260913-134642-45c0e569`.
All twelve companion tests also passed against that rebuilt editor in
`Saved/Validation/CompanionFrostRelease/20260913-134838-a86f71c3`; both runs
passed source/report coverage and unchanged-source checks. The four edited/new
validation Python scripts passed compilation.
Fresh gameplay qualification is still required: this correction was not loaded
in the successful route above.

Original live report is under the retained run's
`UserData/Saved/Validation/Aurelion/CompanionCombat-20260913/live.json`.
The diagnostic script now writes subsequent reports to the project validation
folder and also captures inventory and activity/goal details.

The next retained run, `HolographicHUDRoute-20260913-141207-5e409161`,
again showed Tarrik wielding Velkorran with an available primary attack but no
attributed companion damage. Passive animation observation included the actual
CharacterMesh0/ABP_Biped driver, since the modular visual meshes use leader pose
and have no independent AnimInstance. No weapon attack montage was recorded.
The first observer attempt stopped on a protected goal property; its error
report was preserved before a corrected observer was started. The run passed
through E4 entry. E4A's second pulse paid 30 Echo without a sever; it remained a
failed run and the editor was subsequently closed cleanly for rebuilding.

Code inspection identified a separate scheduling defect: a successful defense
advanced the same `NextCommandAttack` deadline checked before both defense and
ordinary attacks. A sustained enemy attacking tag could therefore select defense
again whenever that shared deadline elapsed, indefinitely postponing offense.
Defense now uses its own four-second cadence. Ordinary attacks retain their
two-second cadence and native completion, costs, attack-token and contribution
gates. This correction still needs rendered combat qualification.

The ordinary E4A input pilot now records missed pulses and their actual spending,
releases input and allows at most two re-aim/normal-combat retries. It checks that
the native links did not change without an observed receipt, supplies no Echo,
and still requires both real sever transactions to pass. A dynamic obstruction
during the charge is a possible explanation for the observed miss, not a proven
root cause. The failed evidence is not rewritten as a passing route.

UE 5.7 Development Editor rebuilt successfully, and all 13 companion tests
passed in `Saved/Validation/CompanionDefenseCadence/20260913-143126-488462a3`.
The new scheduler regression uses real command/activity selection, a completed
defense test ability and an ordinary native bot attack. It verifies contribution
budget admission, defense completion, immediate offense eligibility, and retained
offense cadence while the enemy's attacking tag stays present. Its first fixture
placement intersected the terminal test scene; moving the combat pair into a
clear lane and explicitly asserting eligibility corrected that fixture. An earlier
compile attempt also caught a test parameter shadowing APawn::Controller. Failed
runs are retained. Source/report coverage and unchanged-source checks passed;
all three changed/new validation Python scripts compiled successfully.

The next two live attempts (`CompanionCadenceRoute-20260913-143402-6e41e7f1`
and `HUDGeometryRoute-20260913-144115-df8125cd`) ended in E1 player defeats,
before shared companion combat. Neither qualifies the cadence correction.
The latter also exposed an Eclipse observer lifetime error: calling Python
`SystemLibrary.is_valid` on an already collected actor wrapper itself raises.
The observer now retains only path strings and Python callbacks between ticks,
reacquiring current-world actors before accessing delegates. Three isolated
observer regressions pass for collection, dead-actor detachment and preserved
damage records. These are diagnostic tests, not evidence of gameplay damage.

`CompanionObserverRoute-20260913-144904-612f07d0` passed E1 (207.03 s), E2
(51.05 s), E3 entry (94.08 s), rescue (92.48 s), E4 entry (96.84 s) and E4A
(51.88 s), including the real Axiom main-hand click. The repaired Eclipse
observer recorded no errors. Selene's real CharacterMesh0 animation instance
played `AM_Sword_3P_1H_Attack_1` during E3. The subsequent distance recorder
captured Tarrik's `AM_Sword_3P_1H_Attack_2_Variation_1_Tarrik` during E4A,
including target distances of about 234–255 cm. This proves native attack
activation for both companions. The outgoing damage observer still recorded
zero companion transactions; hit connection and damage remain unqualified.

E4B stopped at 29 seconds: real frost, heat and payoff transaction IDs were
recorded, with the elite alive, its Core revealed and actual Poise broken, but
`HasCompletedFracture` subsequently returned false. Its shared context predicate
still rejected temporary hero busy/interacting/movement-lock/stagger tags after
completion, exposing a conflict with resumed companion actions. A new native
regression reproduced all eight hero/tag cases while the five existing thermal
tests passed (`ThermalCompletedAction/20260913-150303-c1fa608f`). Completed
receipt queries now omit those pending-setup interruption restrictions while
retaining actor, ownership, life, encounter, attempt, epoch and reload checks.
Pending setup and payoff still require uninterrupted heroes. Fresh live
qualification of this correction remains necessary.

The corrected UE 5.7 editor rebuilt in 32.27 seconds; all six thermal tests
passed with source/report coverage and unchanged-source validation in
`ThermalCompletedAction/20260913-150454-f90b4b48`. The failed regression run is
retained as the before-fix evidence.

The next run, `CompanionHitReview-20260913-152207-01c59eff`, initially timed
out in E1 at 421.14 seconds. Its final drone remained alive behind a railing.
The input driver's 80 cm path-point tolerance skipped two corners only 57 cm
apart and drove across collision. Combat approach now uses the same 25 cm
tolerance as route walking. Two corner regressions fail with the old tolerance;
all three tests pass with the correction, including refusal to follow a partial
path. `path-corners-before.txt` and `path-corners-after.txt` retain those results.
This fixes validation input steering, not companion AI or map navigation.

The original failure remains in `E1Continuation`. An initial fresh-start retry
correctly refused the already partially defeated roster (`E1CornerRetry`). The
guarded continuation checks the prior combat timeout, released inputs, unchanged
assets, current world/pawn/attempt/journal and participant identities. It changes
only ordinary input. `E1CornerRetryGuarded` passed in 31.34 seconds with two real
holds and the native Selene handoff; its asset integrity check passed. This is
a retained-state retry, not an uninterrupted fresh-route pass.

`inspect_companion_melee_assets.py` exports the actual weapons, visuals, attack
Blueprint inheritance and defaults. Native Blueprint fingerprints are unchanged
before/after export. Both primary melee abilities inherit a 400 cm AI range;
Verity's native damage-window hookup exists. These findings do not establish the
cause of missed damage. The first inspection used editor-only asset-data lookup
during PIE and emitted diagnostic errors; the corrected live asset-data export
completed successfully in `companion-melee-assets-152545-665956`.
`observe_companion_weapon_hit_path.py` records native collision-cache and hit
history during real montages without initiating traces, attacks or damage.

The guarded route then passed E2 (53.14 s), E3 entry (93.78 s), rescue
(79.69 s), E4 entry (97.25 s) and E4A (40.33 s). E4B failed at 0.89 s:
the inherited elite's Core was already broken before the thermal setup. The
read-only `core-failure-readonly.json` confirms a broken Core, no active reveal,
an empty fracture receipt and elite health 34.03. No reset or replacement proof
was supplied. The source of the early break remains unqualified; the previous
thermal completion correction still needs a successful fresh live exercise.

The first hit-path report captured Tarrik's real sword montage at roughly
277–285 cm from a Linkbound. The cache fields are not reflected to Python;
their access failures are recorded under `inaccessible`, not treated as empty
collision data. The repeated Verity's Wake cost messages came from candidate
availability queries (including the observer), not evidence of actual frost
activations.

After preserving that route failure, `observe_companion_after_player_shot.py`
used the ordinary weapon wheel and one bounded Cinderline trigger, then released
input. The actual player transaction `4015FB7F4378A7C176707DBE71402912` dealt
21 health damage to the WallRunner. Selene immediately started sword montages,
confirming the phase's player-contribution gate had previously withheld attacks.
Over the 100-second diagnostic, no companion damage was recorded. Native verbose
logs accepted the actual montage, 30 cached samples and one collider, but recorded
contacts with Tarrik and his weapon. This diagnostic does not upgrade E4B or
qualify mission completion. Initial attempts stopped before input because the
player weapon was holstered and the moving companion focus changed; those errors
remain in the original editor log.

The exported shared `GA_CombatAbilityBase.GetBotAttackTarget` only accepts
Narrative's `Goal_Attack`; its failed cast has no return path. Following companions
instead own `SovCompanionCommandGoal`, despite a valid controller focus. The new
`ResolveCommandAttackTarget` query exposes only the current command's owned,
active native attack target, retaining mission, activity, life, focus and attack
execution checks. It does not create a target, attack, token or damage receipt.
The scoped editor helper adds this lookup only to the legacy failure return.

The initial editor-helper build failed on `auto*` iteration over `TObjectPtr`
arrays; explicit pointer types corrected it. The next build passed in 10.35 s
and all 13 companion tests passed, including the active target, changed focus
and completed attack checks, with source/report coverage and unchanged-source
verification (`CompanionTargetBridge/20260913-160214-787307e7`).

`CompanionTargetAuthoring-20260913-160314-afd0ffce` saved the Blueprint with zero
compiler errors or warnings. Before/after T3D comparison found exactly one changed
graph section, `GetBotAttackTarget`; the original asset backup is retained beside
those exports. The fresh normal route `CompanionTargetRoute-20260913-160457-c16c1427`
is the subsequent live damage qualification attempt. Launching it is not a pass.

That fresh route passed initial entry in 51.39 s, then timed out in E1 combat
at 421.11 s. Input was released and the asset integrity check passed. The final
drone remained at 140 health; the player stood in a stair pocket at
(-8134.04, -14432.20, 90.15). A read-only comparison of 50 path destinations
found only three complete paths, all still occluded by `StaticMeshActor_161`.
The drone itself was at (-6918.81, -14620.63, 90.15), so airborne height alone
does not explain the failed path. No partial path was followed and no navigation,
actor, health or encounter state was altered. The failed report and
`drone-approach-readonly.json` remain in that run. The companion targeting fix
is built and saved, but fresh source-attributed enemy damage remains unqualified.

`observe_elite_core_lifecycle.py` now records the elite's native target damage
and Core-break delegates independently of the input driver. It was attached
during this E1 run and found the existing elite's Core initially unbroken with
no observer errors. The startup wrapper includes it for subsequent routes so
the source transaction can be identified if the premature Core break recurs.

The subsequent [Verity Twin Blade pass](VerityTwinBlades-2026-09-13.md) replaces
the ordinary third-person Verity combo with four retargeted pack attacks and
authors a weapon-specific idle/walk/run stance. It preserves the command-target
bridge and native damage path. This animation change still requires actual
Selene and companion damage qualification; it is not an additional route pass.
