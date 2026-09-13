"""Author project-owned native protagonist kits after framework setup.

Run inside Unreal Editor after project framework setup. This script never
changes maps, stock /NarrativePro assets, or the original /Game weapon templates.
New Echo/defense children derive directly from native gameplay classes and carry
no legacy Blueprint gameplay graphs. Native code owns costs, payloads and timing.
"""
import json
import os
import re
import runpy
from pathlib import Path
import unreal

OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUT = OUTPUT_DIR / "protagonist-kit-setup.json"
GA = "/NarrativePro/Pro/Core/Abilities/GameplayAbilities/"
AC = "/NarrativePro/Pro/Core/Abilities/Configurations/"
DEFS = "/NarrativePro/Pro/Demo/Character/Definitions/Player/"
ATTRIBUTES = "/NarrativePro/Pro/Core/Abilities/GameplayEffects/Attributes/GE_DefaultPlayerAttributes"
REGEN = "/NarrativePro/Pro/Core/Abilities/GameplayEffects/GE_PassiveAttributeRegen"
WEAPONS = {
    "Velkorran": "/Game/WeaponMeshes/Velkorran/NWI_Velkorran",
    "Cinderline": "/Game/SCF_Rifle_02/NWI_Cinderline",
    "Verity": "/Game/WeaponMeshes/NWI_Verity",
    "Staccato": "/Game/WeaponMeshes/Weapon_Staccato",
    "Axiom": "/Game/WeaponMeshes/Weapon_Axiom",
}
# Hero, native suffix, input number, Echo cost, exact weapon context.
ECHO_KIT = [
    ("Tarrik", "CinderStickyGrenade", 1, 35.0, []),
    ("Tarrik", "VelkorransHunger", 2, 50.0, ["Velkorran"]),
    ("Tarrik", "CinderSlam", 3, 90.0, ["Velkorran"]),
    ("Tarrik", "CinderJudgement", 2, 50.0, ["Cinderline"]),
    ("Tarrik", "CinderlineRequiem", 3, 90.0, ["Cinderline"]),
    ("Selene", "StillpointGrenade", 1, 35.0, ["Verity", "Staccato", "Axiom"]),
    ("Selene", "VeritysWake", 2, 30.0, ["Verity"]),
    ("Selene", "StaccatoZero", 2, 30.0, ["Staccato"]),
    ("Selene", "AxiomNullPulse", 2, 30.0, ["Axiom"]),
    ("Selene", "Dispatch", 3, 90.0, []),
]
report = {"created": [], "saved": [], "abilities": {}, "weapons": {}, "heroes": {},
          "loadout_replacements": [], "native_regen": [], "native_exertion_profiles": {}, "inputs": [], "grant_validation": [],
          "warnings": [], "status": "preflight"}
owned = set()


def asset(path):
    obj = unreal.EditorAssetLibrary.load_asset(path)
    if not obj:
        raise RuntimeError("Required asset missing: " + path)
    return obj


def path(obj):
    return obj.get_path_name() if obj else None


def bp_class(bp):
    result = bp.generated_class()
    if not result:
        raise RuntimeError("Missing generated class: " + path(bp))
    return result


def defaults(bp):
    return unreal.get_default_object(bp_class(bp))


def remember(obj):
    if not path(obj).startswith("/Game/"):
        raise RuntimeError("Only project-owned assets may be modified: " + path(obj))
    owned.add(path(obj))
    return obj


def duplicate(source, destination):
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        return remember(asset(destination))
    result = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    if not result:
        raise RuntimeError("Could not duplicate " + destination)
    report["created"].append(destination)
    return remember(result)


def native_child(native_type, destination):
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        bp = asset(destination)
        parent = str(unreal.EditorAssetLibrary.find_asset_data(destination).get_tag_value("ParentClass"))
        expected = path(native_type.static_class())
        if parent not in (expected, "/Script/CoreUObject.Class'" + expected + "'"):
            raise RuntimeError("Existing destination is not the expected direct-native child: " + destination)
    else:
        package, name = destination.rsplit("/", 1)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", native_type)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, unreal.Blueprint, factory)
        if not bp:
            raise RuntimeError("Could not create native child " + destination)
        report["created"].append(destination)
    remember(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not isinstance(defaults(bp), native_type):
        raise RuntimeError("Native parent mismatch: " + destination)
    return bp


def tag(name):
    result = unreal.GameplayTag()
    if not result.import_text('(TagName="{}")'.format(name)):
        raise RuntimeError("Gameplay tag import failed: " + name)
    return result


def save(obj, blueprint=False):
    if path(obj) not in owned:
        raise RuntimeError("Unowned save rejected: " + path(obj))
    if blueprint:
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        bp_class(obj)
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError("Failed to save " + path(obj))
    report["saved"].append(path(obj))


def unique(classes):
    result, seen = [], set()
    for cls in classes:
        if cls and path(cls) not in seen:
            seen.add(path(cls))
            result.append(cls)
    return result


def input_name(cls):
    return str(unreal.get_default_object(cls).get_editor_property("input_tag").get_editor_property("tag_name"))


def describe_grants(classes):
    return [{"class": path(cls), "input": input_name(cls)} for cls in classes]


def exertion_attribute_name(modifier):
    text = modifier.get_editor_property("attribute").export_text()
    match = re.search(r"/Script/NarrativeArsenal\.NarrativeAttributeSetBase:(\w+)", text)
    return match.group(1) if match else None


def preserve_native_exertion_defaults(hero, attr_bp):
    cdo = defaults(attr_bp)
    rows = list(cdo.get_editor_property("modifiers"))
    native_attributes = {"MaxStamina", "StaminaRegenRate"}
    removed = [row for row in rows if exertion_attribute_name(row) in native_attributes]
    if any(row.get_editor_property("modifier_op") != unreal.GameplayModOp.OVERRIDE for row in removed):
        raise RuntimeError("Unexpected non-override exertion modifier needs review: " + hero)
    removed_names = [exertion_attribute_name(row) for row in removed]
    retained = [row for row in rows if exertion_attribute_name(row) not in native_attributes]
    retained_text = [row.export_text() for row in retained]
    cdo.set_editor_property("modifiers", retained)
    save(attr_bp, blueprint=True)
    after = list(defaults(attr_bp).get_editor_property("modifiers"))
    if [row.export_text() for row in after] != retained_text:
        raise RuntimeError("Unrelated default attribute modifier changed: " + hero)
    report["native_regen"].append({"hero": hero, "attribute_effect": path(attr_bp),
        "removed_overrides": removed_names,
        "native_attributes_absent": sorted(native_attributes),
        "reason": "Native Exertion initializes these bases before the startup attribute effect; later overrides would replace the native profile"})


def validate_combined_grants(hero, context, default_classes, weapon_abilities):
    effective = []
    excluded = []
    for cls in list(default_classes) + list(weapon_abilities):
        cdo = unreal.get_default_object(cls)
        blocked = {str(t.get_editor_property("tag_name"))
                   for t in cdo.get_editor_property("activation_blocked_tags").get_editor_property("gameplay_tags")}
        if context != "Unarmed" and "Narrative.State.Weapon.Equipped" in blocked:
            excluded.append({"class": path(cls), "reason": "authored Equipped blocker excludes the unarmed attack"})
            continue
        input_tag = input_name(cls)
        if input_tag not in ("None", "", "Narrative.Input.None"):
            effective.append((input_tag, path(cls)))
    claims = {}
    for input_tag, cls in effective:
        claims.setdefault(input_tag, []).append(cls)
    duplicates = {key: values for key, values in claims.items() if len(values) > 1}
    report["grant_validation"].append({"hero": hero, "context": context, "claims": claims, "excluded": excluded, "duplicates": duplicates})
    if duplicates:
        raise RuntimeError("Combined default/weapon input conflict: " + hero + "/" + context + ": " + str(duplicates))


try:
    # Load all dependencies before authoring. Copied framework definitions are
    # the only entry points rewired, after the entire kit is saved and checked.
    sources = {name: asset(source) for name, source in WEAPONS.items()}
    definitions = {hero: remember(asset("/Game/Characters/Definitions/PD_" + hero))
                   for hero in ("Tarrik", "Selene")}
    for hero in definitions:
        pawn = defaults(asset("/Game/PlayerCharacters/BP_Sov" + hero))
        exertion = pawn.get_exertion_component()
        if not exertion or not exertion.get_editor_property("apply_prototype_defaults"):
            raise RuntimeError("Native exertion must own prototype defaults before removing startup overrides: " + hero)
        profile = exertion.get_profile()
        report["native_exertion_profiles"][hero] = {
            "component": path(exertion), "apply_prototype_defaults": True,
            "maximum_stamina": profile.get_editor_property("maximum_stamina"),
            "idle_regen_rate": profile.get_editor_property("idle_regen_rate")}
    source_configs = {"Tarrik": asset(AC + "AC_Player_Shooter"), "Selene": asset(AC + "AC_Player_Selene")}
    source_collections = {hero: asset(DEFS + "IC_Default" + hero + "Items") for hero in definitions}
    stock_attributes = asset(ATTRIBUTES)
    regen_cls = bp_class(asset(REGEN))
    regen_defaults = unreal.get_default_object(regen_cls)
    regen_modifiers = list(regen_defaults.get_editor_property("modifiers"))
    if len(regen_modifiers) != 1 or ":Stamina" not in regen_modifiers[0].get_editor_property("attribute").export_text():
        raise RuntimeError("Stock regeneration contract changed; refusing to remove non-Stamina effects")
    # Validate the already authored input action-to-tag bridge used by copied PC.
    inputs = asset("/NarrativePro/Pro/Core/Data/Input/DA_DefaultAbilityInputs")
    source_context = asset("/NarrativePro/Pro/Core/Data/Input/IMC_Default")
    if len(source_context.get_editor_property("mapping_profile_overrides")):
        raise RuntimeError("Custom input profile overrides need inspection before prototype control migration")
    input_tags = {str(row.get_editor_property("input_tag").get_editor_property("tag_name"))
                  for row in inputs.get_editor_property("input_abilities")}
    for name in ("Narrative.Input.Ability1", "Narrative.Input.Ability2", "Narrative.Input.Ability3", "Narrative.Input.Attack.Heavy"):
        if name not in input_tags:
            raise RuntimeError("Required input bridge missing: " + name)
    for hero, suffix, _, _, _ in ECHO_KIT:
        getattr(unreal, "SovGameplayAbility_" + hero + suffix)
    report["status"] = "authoring isolated kit assets"

    weapon_bps = {name: duplicate(source, "/Game/Items/Weapons/WI_" + name) for name, source in WEAPONS.items()}
    weapon_classes = {name: bp_class(bp) for name, bp in weapon_bps.items()}
    class_replacements = {path(bp_class(sources[name])): path(weapon_classes[name]) for name in WEAPONS}

    # Fresh native children deliberately keep the latest payload defaults.
    abilities = {}
    for hero, suffix, input_number, cost, weapon_names in ECHO_KIT:
        native = getattr(unreal, "SovGameplayAbility_" + hero + suffix)
        bp = native_child(native, "/Game/Abilities/" + hero + "/GA_" + hero + "_" + suffix)
        cdo = defaults(bp)
        native_cdo = unreal.get_default_object(native.static_class())
        # Restore the native transaction contract even when safely rerunning.
        for prop in ("input_tag", "cost_gameplay_effect_class", "cooldown_gameplay_effect_class",
                     "activation_owned_tags", "activation_blocked_tags", "activation_required_tags",
                     "minimum_echo_required", "echo_cost"):
            cdo.set_editor_property(prop, native_cdo.get_editor_property(prop))
        # These policy properties are native VisibleDefaultsOnly contracts.
        # Validate inheritance instead of trying to write read-only properties.
        for prop in ("requires_allowed_weapon", "weapon_gate_policy"):
            if cdo.get_editor_property(prop) != native_cdo.get_editor_property(prop):
                raise RuntimeError("Unexpected serialized policy override: " + suffix + "/" + prop)
        cdo.set_editor_property("allowed_weapon_classes", [weapon_classes[name] for name in weapon_names])
        if suffix == "Dispatch":
            cdo.set_editor_property("verity_weapon_classes", [weapon_classes["Verity"]])
        if input_name(bp_class(bp)) != "Narrative.Input.Ability" + str(input_number):
            raise RuntimeError("Unexpected native input contract: " + suffix)
        for prop in ("minimum_echo_required", "echo_cost"):
            if abs(cdo.get_editor_property(prop) - cost) > 0.001:
                raise RuntimeError("Native Echo budget changed: " + suffix)
        if cdo.get_editor_property("cost_gameplay_effect_class"):
            raise RuntimeError("Echo ability must not also have a cost effect: " + suffix)
        save(bp, blueprint=True)
        abilities[suffix] = bp_class(bp)
        report["abilities"][suffix] = {"asset": path(bp), "native_parent": path(native.static_class()),
                                        "input": input_name(bp_class(bp)), "echo": cost,
                                        "allowed_weapons": [path(weapon_classes[n]) for n in weapon_names]}

    for hero, suffix in (("Tarrik", "Guard"), ("Selene", "Deflection")):
        native = getattr(unreal, "SovGameplayAbility_" + hero + suffix)
        bp = native_child(native, "/Game/Abilities/" + hero + "/GA_" + hero + "_" + suffix)
        save(bp, blueprint=True)
        abilities[suffix] = bp_class(bp)

    # The shipped Cover and Dodge assets both claim Ability1. Use the current
    # native movement owner and give Cover its already authored separate tag.
    for suffix in ("Evade", "Sprint"):
        bp = native_child(getattr(unreal, "SovGameplayAbility_" + suffix), "/Game/Abilities/Common/GA_" + suffix)
        if defaults(bp).get_editor_property("cost_gameplay_effect_class"):
            raise RuntimeError("Native exertion must not also have a cost effect: " + suffix)
        save(bp, blueprint=True)
        abilities[suffix] = bp_class(bp)
    cover = duplicate(GA + "Movement/GA_Cover", "/Game/Abilities/Common/GA_Cover")
    defaults(cover).set_editor_property("input_tag", tag("Narrative.Input.Cover"))
    save(cover, blueprint=True)
    abilities["Cover"] = bp_class(cover)

    # Keep old dodge and grenade physical buttons, with explicit project names.
    # UE 5.7 moved actual mappings into DefaultKeyMappings.Mappings.
    input_root = "/NarrativePro/Pro/Core/Data/Input/"
    action_evade = duplicate(input_root + "IA_Ability1", "/Game/Input/IA_Evade")
    action_grenade = duplicate(input_root + "IA_Throw", "/Game/Input/IA_Grenade")
    action_evade.set_editor_property("action_description", "Evade")
    action_grenade.set_editor_property("action_description", "Grenade")
    save(action_evade)
    save(action_grenade)
    action_map = {path(asset(input_root + "IA_Ability1")): action_evade,
                  path(asset(input_root + "IA_Throw")): action_grenade}
    input_copy = duplicate(path(inputs), "/Game/Input/DA_CombatInputs")
    # UE Python struct wrappers can be live views. Mutable structs must come
    # from the project copy, never from a source template's property storage.
    ability_rows = list(input_copy.get_editor_property("input_abilities"))
    for row in ability_rows:
        old_action = path(row.get_editor_property("input_action"))
        if old_action in action_map:
            replacement = action_map[old_action]
            row.set_editor_property("input_action", replacement)
            row.set_editor_property("input_tag", tag("Narrative.Input.Evade" if replacement == action_evade else "Narrative.Input.Ability1"))
        elif old_action in (path(action_evade), path(action_grenade)):
            row.set_editor_property("input_tag", tag("Narrative.Input.Evade" if old_action == path(action_evade) else "Narrative.Input.Ability1"))
    input_copy.set_editor_property("input_abilities", ability_rows)
    save(input_copy)
    mapping_copy = duplicate(path(source_context), "/Game/Input/IMC_Combat")
    mapping_data = mapping_copy.get_editor_property("default_key_mappings")
    mappings = list(mapping_data.get_editor_property("mappings"))
    def key(name):
        result = unreal.Key()
        if not result.import_text(name):
            raise RuntimeError("Invalid input key: " + name)
        return result
    migrated = []
    for row in mappings:
        old_action = path(row.get_editor_property("action"))
        key_name = row.get_editor_property("key").export_text()
        if old_action == path(asset(input_root + "IA_HeavyAttack")) and key_name == "MiddleMouseButton":
            continue  # Rebuilt once below so repeat execution never doubles it.
        if old_action in action_map:
            row.set_editor_property("action", action_map[old_action])
        if old_action == path(asset(input_root + "IA_Ability2")) and key_name == "E":
            row.set_editor_property("key", key("X"))
        # Those two D-pad keys previously changed camera mode. Mouse-wheel
        # camera controls remain; all combat actions gain dedicated pad input.
        if old_action == path(asset(input_root + "IA_CameraModeUp")) and key_name == "Gamepad_DPad_Up":
            row.set_editor_property("action", asset(input_root + "IA_Ability2"))
        if old_action == path(asset(input_root + "IA_CameraModeDown")) and key_name == "Gamepad_DPad_Down":
            row.set_editor_property("action", asset(input_root + "IA_HeavyAttack"))
        migrated.append(row)
    heavy_mouse = unreal.EnhancedActionKeyMapping()
    heavy_mouse.set_editor_property("action", asset(input_root + "IA_HeavyAttack"))
    heavy_mouse.set_editor_property("key", key("MiddleMouseButton"))
    migrated.append(heavy_mouse)
    mapping_data.set_editor_property("mappings", migrated)
    mapping_copy.set_editor_property("default_key_mappings", mapping_data)
    save(mapping_copy)
    action_tags = {path(row.get_editor_property("input_action")): str(row.get_editor_property("input_tag").get_editor_property("tag_name"))
                   for row in ability_rows}
    for row in migrated:
        action_path = path(row.get_editor_property("action"))
        report["inputs"].append({"key": row.get_editor_property("key").export_text(), "action": action_path,
                                  "ability_tag": action_tags.get(action_path)})

    # Existing bash was Ability3, which also activates Requiem/Dispatch.
    # Only this project copy moves to the existing heavy-attack input action.
    bash_bp = duplicate(GA + "Attacks/Melee/GA_Weapon_Bash", "/Game/Abilities/Common/GA_WeaponBash")
    defaults(bash_bp).set_editor_property("input_tag", tag("Narrative.Input.Attack.Heavy"))
    save(bash_bp, blueprint=True)
    abilities["Bash"] = bp_class(bash_bp)

    stock = lambda suffix: bp_class(asset(GA + suffix))
    attack = {
        "Velkorran": bp_class(asset("/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Melee/Abilities/GA_Attack_Melee_Sword_1H_Tarrik")),
        "Verity": bp_class(asset("/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Melee/Abilities/GA_Attack_Melee_Verity")),
        "Cinderline": (bp_class(asset("/Game/Abilities/Tarrik/GA_CinderlinePrimary"))
                       if unreal.EditorAssetLibrary.does_asset_exist("/Game/Abilities/Tarrik/GA_CinderlinePrimary")
                       else stock("Attacks/Firearms/GA_Firearm_Cinderline")),
        "Staccato": stock("Attacks/Firearms/GA_Staccato"),
        "Axiom": stock("Attacks/Firearms/GA_Firearm_Pistol"),
    }
    aim = stock("Weapons/GA_Weapon_Aim")
    grants = {
        "Velkorran": [attack["Velkorran"], abilities["Guard"], abilities["VelkorransHunger"], abilities["CinderSlam"]],
        "Cinderline": [attack["Cinderline"], aim, abilities["Bash"], abilities["CinderJudgement"], abilities["CinderlineRequiem"]],
        "Verity": [attack["Verity"], abilities["Deflection"], abilities["VeritysWake"]],
        "Staccato": [attack["Staccato"], aim, abilities["Bash"], abilities["StaccatoZero"]],
        "Axiom": [attack["Axiom"], aim, abilities["AxiomNullPulse"]],
    }
    for name, bp in weapon_bps.items():
        cdo = defaults(bp)
        # Only the project-owned Axiom opts into a symmetric native restriction.
        # Verity+Axiom has overlapping primary/Echo input claims; dual Axiom has
        # its explicit right/left grant layout below.
        if name == "Axiom":
            cdo.set_editor_property("require_same_class_for_dual_wield", True)
        elif cdo.get_editor_property("require_same_class_for_dual_wield"):
            raise RuntimeError("Unexpected same-class pairing restriction on " + name)
        regular = unique(grants[name])
        # Narrative replaces the regular array in a dual wield state. Axiom's
        # established right/left primary controls stay intact; only the main
        # weapon contributes its Echo variant, preventing two paid casts.
        main = [stock("Attacks/Firearms/GA_Firearm_Pistol_R"), abilities["AxiomNullPulse"]] if name == "Axiom" else regular
        off = [attack["Axiom"]] if name == "Axiom" else []
        for prop, values in (("weapon_abilities", regular), ("mainhand_weapon_abilities", main), ("offhand_weapon_abilities", off)):
            cdo.set_editor_property(prop, values)
            names = [input_name(cls) for cls in values]
            if len(names) != len(set(names)):
                raise RuntimeError("Duplicate input claimants in " + name + "/" + prop)
        save(bp, blueprint=True)
        if bool(defaults(bp).get_editor_property("require_same_class_for_dual_wield")) != (name == "Axiom"):
            raise RuntimeError("Compiled weapon lost its pairing contract: " + name)
        report["weapons"][name] = {"asset": path(bp), "regular": describe_grants(regular),
                                     "mainhand": describe_grants(main), "offhand": describe_grants(off),
                                     "require_same_class_for_dual_wield": name == "Axiom"}

    weapon_classes = {name: bp_class(bp) for name, bp in weapon_bps.items()}
    if weapon_classes["Axiom"] == weapon_classes["Verity"]:
        raise RuntimeError("Axiom and Verity must remain distinct item classes")
    report["dual_wield_policy"] = {
        "native_default": False,
        "opted_in": [path(weapon_bps["Axiom"])],
        "restriction": "If either participant opts in, exact item classes must match; existing native hand/equipment/visual rules still apply.",
        "blocked_contexts": ["Verity+Axiom", "Axiom+Verity"],
        "validated_grant_context": "DualAxiom",
    }

    configured = {}
    for hero in ("Tarrik", "Selene"):
        config = duplicate(path(source_configs[hero]), "/Game/Abilities/Configurations/AC_" + hero)
        source_default = list(source_configs[hero].get_editor_property("default_abilities"))
        # Keep shared locomotion, wield, reload, death, cover and unarmed. Echo
        # skills are regranted exactly once by their intended owner below.
        retired_movement = {path(bp_class(asset(GA + "Movement/GA_" + name))) for name in ("Cover", "Dodge", "Sprint")}
        common = [cls for cls in unique(source_default)
                  if not isinstance(unreal.get_default_object(cls), unreal.SovGameplayAbility_EchoBase)
                  and not isinstance(unreal.get_default_object(cls), unreal.SovGameplayAbility_TarrikGuard)
                  and not isinstance(unreal.get_default_object(cls), unreal.SovGameplayAbility_SeleneDeflection)
                  and path(cls) not in retired_movement]
        common += [abilities["Sprint"], abilities["Evade"], abilities["Cover"]]
        hero_abilities = [abilities["CinderStickyGrenade"]] if hero == "Tarrik" else [abilities["StillpointGrenade"], abilities["Dispatch"]]
        config.set_editor_property("default_abilities", unique(common + hero_abilities))
        validate_combined_grants(hero, "Unarmed", config.get_editor_property("default_abilities"), [])
        for weapon_name in (["Velkorran", "Cinderline"] if hero == "Tarrik" else ["Verity", "Staccato", "Axiom"]):
            validate_combined_grants(hero, weapon_name, config.get_editor_property("default_abilities"), grants[weapon_name])
        if hero == "Selene":
            axiom = defaults(weapon_bps["Axiom"])
            validate_combined_grants(hero, "DualAxiom", config.get_editor_property("default_abilities"),
                                     list(axiom.get_editor_property("mainhand_weapon_abilities")) + list(axiom.get_editor_property("offhand_weapon_abilities")))
        startup = list(source_configs[hero].get_editor_property("startup_effects"))
        config.set_editor_property("startup_effects", [cls for cls in startup if cls != regen_cls])
        report["native_regen"].append({"hero": hero, "removed": path(regen_cls), "reason": "Stamina-only periodic GE suppressed native exertion regeneration"})
        attr_bp = duplicate(ATTRIBUTES, "/Game/Abilities/Effects/GE_" + hero + "_DefaultAttributes")
        preserve_native_exertion_defaults(hero, attr_bp)
        config.set_editor_property("default_attributes", bp_class(attr_bp))
        save(config)

        collection = duplicate(path(source_collections[hero]), "/Game/Items/Loadouts/IC_" + hero)
        entries = []
        copied_weapon_paths = set()
        for source_entry in source_collections[hero].get_editor_property("items"):
            old_text = source_entry.export_text()
            new_text = old_text
            for old_class, new_class in class_replacements.items():
                if old_class in new_text:
                    new_text = new_text.replace(old_class, new_class)
                    copied_weapon_paths.add(new_class)
                    report["loadout_replacements"].append({"hero": hero, "before": old_text, "after": new_text})
            entry = unreal.ItemWithQuantity()
            if not entry.import_text(new_text):
                raise RuntimeError("Could not preserve item entry " + old_text)
            entries.append(entry)
        required_weapons = ["Velkorran", "Cinderline"] if hero == "Tarrik" else ["Verity", "Staccato", "Axiom"]
        for name in required_weapons:
            target = path(weapon_classes[name])
            if target not in copied_weapon_paths:
                entry = unreal.ItemWithQuantity()
                if not entry.import_text('(Item="{}",Quantity=1)'.format(target)):
                    raise RuntimeError("Could not author missing weapon loadout entry: " + name)
                entries.append(entry)
                report["loadout_replacements"].append({"hero": hero, "added_missing_weapon": target})
        equipment_helpers = runpy.run_path(str(Path(__file__).parent / "protagonist_equipment_order.py"))
        entries, equipment_review = equipment_helpers["arrange_equipped_loadout"](hero, entries, weapon_bps)
        report.setdefault("equipment_order", {})[hero] = equipment_review
        collection.set_editor_property("items", entries)
        save(collection)

        # Preserve the source definition's roll settings and all direct items.
        # This inspected loadout uses one collection and no random tables.
        rolls = list(definitions[hero].get_editor_property("default_item_loadout"))
        replaced = 0
        for roll in rolls:
            if roll.get_editor_property("table_to_roll"):
                raise RuntimeError("Unexpected random loadout table; requires separate copying")
            collections = []
            for candidate in roll.get_editor_property("item_collections_to_grant"):
                if candidate in (source_collections[hero], collection):
                    candidate = collection
                    replaced += 1
                collections.append(candidate)
            roll.set_editor_property("item_collections_to_grant", collections)
        if replaced != 1:
            raise RuntimeError("Expected exactly one protagonist collection reference for " + hero)
        configured[hero] = (config, rolls)
        report["heroes"][hero] = {"definition": path(definitions[hero]), "config": path(config), "collection": path(collection),
                                   "default_abilities": describe_grants(config.get_editor_property("default_abilities")),
                                   "loadout": [entry.export_text() for entry in entries],
                                   "attributes": path(attr_bp)}

    # Publish the completed asset graph only after both kits have saved.
    for hero, (config, rolls) in configured.items():
        definition = definitions[hero]
        definition.set_editor_property("ability_configuration", config)
        definition.set_editor_property("default_item_loadout", rolls)
        save(definition)
    controller = remember(asset("/Game/Framework/BP_SovPlayerController"))
    defaults(controller).set_editor_property("ability_input_mappings", input_copy)
    defaults(controller).set_editor_property("default_mapping_context", mapping_copy)
    frontend = defaults(controller).get_frontend()
    if not frontend or frontend.get_owner() != defaults(controller):
        raise RuntimeError("Combat vitals opt-in must target the copied controller's own frontend template")
    frontend.set_editor_property("show_combat_vitals", True)
    save(controller, blueprint=True)
    if not defaults(controller).get_frontend().get_editor_property("show_combat_vitals"):
        raise RuntimeError("Copied controller did not retain combat vitals opt-in")
    report["combat_vitals"] = {"controller": path(controller), "enabled": True,
        "resources": ["Health", "Shield", "Stamina", "Poise", "Echo"], "native_default_enabled": False}
    report["warnings"].append("Health and other scalable/curve attribute modifiers are retained; only native Exertion MaxStamina/StaminaRegenRate overrides are removed. Fresh baseline Selene PIE resolved Health/MaxHealth=100 and Echo=25; no health normalization is applied.")
    report["warnings"].append("Direct-native projectile defaults execute gameplay; presentation meshes, montage/VFX polish and PIE qualification remain separate.")
    report["status"] = "authored; requires fresh Blueprint load and PIE gameplay qualification"
except Exception as exc:
    report["error"] = str(exc)
    report["status"] = "stopped; inspect partial assets before rerun"
    raise
finally:
    report["package_manifest"] = sorted({name.split(".")[0] for name in report["created"] + report["saved"]})
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("VELKORRAN_NATIVE_KIT_REPORT " + str(OUT))
