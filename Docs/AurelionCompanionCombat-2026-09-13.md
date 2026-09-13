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

The behavior defect remains open. The existing companion context command only
moves toward its leader/hold/defend destination. It does not request weapon draw
or move into a selected attack's native range. The mission currently curates
only defense and unarmed punch; the proxy copies only classes present in the
outgoing ASC. Simply adding an armed attack class can still fail when that
player weapon was holstered at handoff, because its item grants are then absent.
The pacifist baseline deliberately leaves contextual command ownership to the
native companion activity; replacing it with unrestricted combat AI would bypass
the intended curated kit and damage contribution policy.

Next runtime qualification must exercise both real protagonist handoffs, inspect
equipped versus wielded items and native candidate rejection reasons, and verify
actual draw/holster, locomotion and attributed damage. A narrowly scoped native
correction may be needed for readiness and attack approach, preserving player
unlocks, protected-target rules, cooldowns, contribution limits, cinematic marks,
save restoration and co-action activity ownership. No such source change has
been made or claimed qualified in this pass. Equipment authoring alone does not
close the user-reported bug.

Original live report is under the retained run's
`UserData/Saved/Validation/Aurelion/CompanionCombat-20260913/live.json`.
The diagnostic script now writes subsequent reports to the project validation
folder and also captures inventory and activity/goal details.
