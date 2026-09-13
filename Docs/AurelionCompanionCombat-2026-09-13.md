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
Both actual handoffs, animated draw/holster and attributed companion damage
remain unqualified until observed. The user-reported bug remains open.

Original live report is under the retained run's
`UserData/Saved/Validation/Aurelion/CompanionCombat-20260913/live.json`.
The diagnostic script now writes subsequent reports to the project validation
folder and also captures inventory and activity/goal details.
