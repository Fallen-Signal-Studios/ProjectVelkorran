"""Remove Narrative demo weapon grants from Aurelion melee/support enemy roles.

Run with UnrealEditor-Cmd and the PythonScript commandlet.

Three Aurelion roles were authored with Narrative Pro demo weapons as placeholders.
Their approved combat identities do not use a conventional carried weapon:

  Weaver      backline support - severable armor tethers, ward, ally threat sharing
  WallRunner  traversal/flanking - inherits ASovAurelionLinkbound, which carries no
              loadout at all and fights through its NPCActivity ability path
  Elite       core weak point plus Thermal Fracture; AurelionLayoutContract-2026-09-07
              requires ThermalFractureComponent and WeakPointComponent, and its
              AC_Abilities_AurelionElite already supplies Unarmed attacks

Offense for these roles comes from the activity/GAS path, not an equipped item, so
clearing DefaultItemLoadout removes the demo weapon without removing their offense.

The Enforcer is deliberately NOT touched. It is a conventional Dominion ranged
soldier and genuinely requires a carried rifle; no authored project rifle weapon
definition exists yet. Clearing it would leave it unable to shoot, which is worse
than a placeholder. See the accompanying report for the minimal missing asset.

Narrative Pro demo content itself is never modified.
"""

import json

import unreal

TARGETS = [
    "/Game/Aurelion/Enemies/NPC_AurelionWeaver",
    "/Game/Aurelion/Enemies/NPC_AurelionWallRunner",
    "/Game/Aurelion/Enemies/NPC_AurelionElite",
]

# Never touched by this script, with the reason recorded for the report.
EXCLUDED = {
    "/Game/Aurelion/Enemies/NPC_AurelionEnforcer":
        "conventional Dominion ranged soldier; needs an authored rifle that does not exist yet",
}

PROPERTY = "default_item_loadout"

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
result = {"changed": [], "unchanged": [], "errors": [], "excluded": EXCLUDED}


def describe(loadout):
    """Readable summary of a DefaultItemLoadout array for the before/after record."""
    entries = []
    for roll in loadout:
        try:
            for item in roll.get_editor_property("items_to_grant"):
                entries.append("%s x%d" % (str(item.get_editor_property("item")),
                                           item.get_editor_property("quantity")))
            for collection in roll.get_editor_property("item_collections_to_grant"):
                if collection:
                    entries.append("collection %s" % collection.get_name())
        except Exception as error:  # noqa: BLE001 - recorded, never silently dropped
            entries.append("<unreadable: %s>" % error)
    return entries


for path in TARGETS:
    if path in EXCLUDED:
        result["errors"].append("%s is on the excluded list and must not be cleared" % path)
        continue
    definition = assets.load_asset(path)
    if definition is None:
        result["errors"].append("could not load %s" % path)
        continue
    try:
        before = list(definition.get_editor_property(PROPERTY))
    except Exception as error:  # noqa: BLE001
        result["errors"].append("%s has no %s: %s" % (path, PROPERTY, error))
        continue
    before_text = describe(before)
    if not before:
        result["unchanged"].append({"asset": path, "reason": "loadout already empty"})
        continue
    definition.set_editor_property(PROPERTY, [])
    after = list(definition.get_editor_property(PROPERTY))
    if after:
        result["errors"].append("%s still has %d loadout rolls after clearing" % (path, len(after)))
        continue
    if not assets.save_loaded_asset(definition, only_if_is_dirty=False):
        result["errors"].append("could not save %s" % path)
        continue
    result["changed"].append({"asset": path, "removed": before_text})

print("SOV_LOADOUT_RESULT_BEGIN")
print(json.dumps(result, indent=2, sort_keys=True))
print("SOV_LOADOUT_RESULT_END")
