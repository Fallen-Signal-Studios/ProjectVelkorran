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
