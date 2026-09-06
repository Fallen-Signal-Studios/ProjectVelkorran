"""Read native equipment rules and order existing prototype loadout entries.

No UObject fields are mutated here. Narrative adds collection items in order and
auto-equips each into its first free allowed slot, without a later retry.
"""
import collections
import re
import unreal

HERO_WEAPONS = {"Tarrik": ("Cinderline", "Velkorran"), "Selene": ("Verity", "Staccato", "Axiom")}


def tag_name(tag):
    return str(unreal.GameplayTagLibrary.get_tag_name(tag))


def weapon_rules(weapon_blueprints):
    result = {}
    for name, bp in weapon_blueprints.items():
        cls = bp.generated_class()
        cdo = unreal.get_default_object(cls)
        slots = unreal.GameplayTagLibrary.break_gameplay_tag_container(cdo.get_editor_property("equippable_slots"))
        holsters = cdo.get_editor_property("holster_attachment_configs")
        result[name] = {"class": cls.get_path_name(), "allowed_slots": [tag_name(t) for t in slots],
                        "holster_slots": [tag_name(t) for t in holsters.keys()]}
    return result


def item_class(entry):
    text = entry.export_text()
    match = re.search(r'(?:^|\()Item="([^"]+)"', text)
    if not match:
        raise RuntimeError("Unrecognized authored item entry: " + text)
    return match.group(1)


def simulate_order(entries, rules):
    names = {row["class"]: name for name, row in rules.items()}
    occupied = {}
    assignments = []
    for entry in entries:
        cls = item_class(entry)
        if cls not in names:
            continue
        name = names[cls]
        if entry.get_editor_property("quantity") != 1:
            raise RuntimeError("Prototype weapon quantity requires separate slot validation: " + name)
        rule = rules[name]
        chosen = next((slot for slot in rule["allowed_slots"] if slot not in occupied), None)
        if chosen and chosen not in rule["holster_slots"]:
            raise RuntimeError("Allowed slot has no authored weapon attachment: " + name + "/" + chosen)
        assignments.append({"weapon": name, "class": cls, "slot": chosen, "blocked_by": dict(occupied) if not chosen else {}})
        if chosen:
            occupied[chosen] = name
    return assignments


def arrange_equipped_loadout(hero, entries, weapon_blueprints):
    entries = list(entries)
    rules = weapon_rules({name: weapon_blueprints[name] for name in HERO_WEAPONS[hero]})
    before_text = [entry.export_text() for entry in entries]
    before = simulate_order(entries, rules)
    if collections.Counter(row["weapon"] for row in before) != collections.Counter(HERO_WEAPONS[hero]):
        raise RuntimeError("Expected one of every protagonist weapon in loadout: " + hero)
    if hero == "Selene":
        verity = rules["Verity"]
        staccato = rules["Staccato"]
        back_a = "Narrative.Equipment.Slot.Weapon.BackA"
        back_b = "Narrative.Equipment.Slot.Weapon.BackB"
        if verity["allowed_slots"] != [back_a] or not {back_a, back_b}.issubset(staccato["allowed_slots"]):
            raise RuntimeError("Observed Verity-only-BackA / Staccato-BackA+BackB contract changed; inspect instead of reordering blindly")
        first = next(i for i, entry in enumerate(entries) if item_class(entry) == verity["class"])
        rifle = next(i for i, entry in enumerate(entries) if item_class(entry) == staccato["class"])
        if first > rifle:
            entry = entries.pop(first)
            entries.insert(rifle, entry)
    after = simulate_order(entries, rules)
    if any(row["slot"] is None for row in after):
        raise RuntimeError("Loadout still has an equipment-slot conflict: " + repr(after))
    after_text = [entry.export_text() for entry in entries]
    if collections.Counter(before_text) != collections.Counter(after_text):
        raise RuntimeError("Reordering changed item data or quantities")
    return entries, {"hero": hero, "rules": rules, "before": before, "after": after,
                     "changed": before_text != after_text, "entry_multiset_preserved": True}
