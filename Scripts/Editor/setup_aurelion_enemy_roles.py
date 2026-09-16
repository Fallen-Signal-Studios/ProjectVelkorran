"""Project-owned Aurelion enemies. Import and call build_enemy_assets(output_dir).

Requires compiled SovAurelionEnemyAuthoringLibrary and stopped PIE. This module
does nothing on import. It saves only /Game/Aurelion/Enemies and never starts AI,
edits a map, fabricates campaign proof or modifies a seed asset.
"""
import hashlib
import json
import math
from pathlib import Path
import re
import unreal

ROOT = "/Game/Aurelion/Enemies/"
STAMP = "Sov.Aurelion.EnemyRoleSchema"
SCHEMA = "2026-09-07.1"
PLACED_LIFECYCLE_TAG = "Narrative.State.DontReturnToSpawn"
SEEDS = {
    "SecurityDrone": ("/Game/SciFi_Drone_1/Textures/NPC_ReformationCombatDrone", "SovAurelionSecurityDrone"),
    "ContaminatedDrone": ("/Game/SciFi_Drone_1/Textures/NPC_ReformationCombatDrone", "SovDroneNPCBase"),
    "Enforcer": ("/Game/SciFiSoldier/SkeletalMesh/NPC_DominionEnforcer", "SovNPCCharacterBase"),
    "Linkbound": ("/Game/SciFiSoldier/SkeletalMesh/NPC_DominionEnforcer_Sword", "SovAurelionLinkbound"),
    "WallRunner": ("/Game/SciFiSoldier/SkeletalMesh/NPC_DominionEnforcer_Sword", "SovAurelionWallRunner"),
    "Weaver": ("/Game/SciFiSoldier/SkeletalMesh/NPC_DominionEnforcer", "SovAurelionWeaver"),
    "Elite": ("/Game/SciFiSoldier/SkeletalMesh/NPC_DominionEnforcer_Sword", "SovAurelionElite"),
}
DEF_FIELDS = ("character_id", "npc_name", "npc_class_path", "default_appearance", "default_factions",
              "default_owned_tags", "ability_configuration", "activity_configuration", "default_item_loadout",
              "allow_multiple_instances", "default_currency", "is_vendor", "dialogue", "tagged_dialogue_set")
DISMEMBERMENT_FIELDS = (
    "dismemberment_enabled", "dismemberment_profile", "use_sk_mannequin_bone_map",
    "fallback_regions", "fallback_rules", "default_minimum_applied_health_damage",
    "default_minimum_health_overkill_damage", "detached_limb_impulse_per_damage",
    "minimum_detached_limb_impulse", "maximum_detached_limb_impulse",
    "spawn_death_blood_puddle_on_death", "death_blood_puddle_material",
    "death_blood_puddle_anchor_bone", "death_blood_puddle_size",
    "death_blood_puddle_spawn_delay_seconds", "death_blood_puddle_maximum_settle_wait_seconds",
    "death_blood_puddle_maximum_settle_speed", "death_blood_puddle_maximum_settle_angular_speed",
    "death_blood_puddle_settle_dwell_seconds", "death_blood_puddle_retry_interval_seconds",
    "death_blood_puddle_floor_trace_distance", "death_blood_puddle_minimum_floor_normal_z",
    "death_blood_puddle_surface_offset", "death_blood_puddle_fade_in_seconds",
    "death_blood_puddle_life_seconds", "death_blood_puddle_fade_out_seconds")
DRONE_DISABLED_FLAGS = ("dismemberment_enabled", "use_sk_mannequin_bone_map", "spawn_death_blood_puddle_on_death")


def _path(obj):
    return obj.get_path_name() if obj else None


def _load(value):
    if isinstance(value, unreal.Object):
        return value
    text = str(value)
    match = re.search(r"(/[A-Za-z0-9_/]+(?:\.[A-Za-z0-9_]+)?)", text)
    if not match:
        raise RuntimeError("Missing/unresolved asset reference: " + text)
    obj = unreal.load_object(None, match.group(1))
    if not obj:
        obj = unreal.load_asset(match.group(1))
    if not obj:
        raise RuntimeError("Could not load required asset: " + text)
    return obj


def _encode(value):
    if value is None or isinstance(value, (bool, int, float)):
        return value
    if isinstance(value, unreal.Object):
        return _path(value)
    if isinstance(value, (list, tuple, unreal.Array)):
        return [_encode(item) for item in value]
    if hasattr(value, "export_text"):
        return value.export_text()
    return str(value)


def _disk_files(packages):
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    plugins = list((project / "Plugins").glob("Narrative*/Content"))
    if len(plugins) != 1:
        raise RuntimeError("Expected one actual installed Narrative content root")
    result = {}
    for package in sorted(packages):
        package = package.split(".")[0]
        if package.startswith("/Game/"):
            base = project / "Content" / package[len("/Game/"):]
        elif package.startswith("/NarrativePro/"):
            base = plugins[0] / package[len("/NarrativePro/"):]
        else:
            raise RuntimeError("Unexpected seed mount: " + package)
        found = False
        for suffix in (".uasset", ".uexp", ".ubulk"):
            file = Path(str(base) + suffix)
            if file.is_file():
                found = True
                result[str(file)] = hashlib.sha256(file.read_bytes()).hexdigest()
        if not found:
            raise RuntimeError("Seed package cannot be fingerprinted: " + package)
    return result


def _outside_pie():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("Stop PIE before authoring enemy assets")


def _component_data(bp):
    """Read native defaults plus actual SCS hierarchy; CDO.GetComponents omits SCS."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = list(subsystem.k2_gather_subobject_data_for_blueprint(bp))
    roots, entries = [], []
    for handle in handles:
        data = library.get_data(handle)
        if not library.is_valid(data):
            raise RuntimeError("Invalid authored component handle: " + _path(bp))
        if library.is_root_actor(data):
            roots.append(handle)
        if not library.is_component(data):
            continue
        # This Blueprint-context API intentionally reads the existing template.
        # GetObjectForBlueprint can create an inherited override during a read.
        obj = library.get_object(data)
        if not isinstance(obj, unreal.ActorComponent):
            raise RuntimeError("Component inventory returned a non-component")
        row = {"object": _path(obj), "name": obj.get_name(), "class": _path(obj.get_class()),
               "variable": str(library.get_variable_name(data)),
               "native": bool(library.is_native_component(data)),
               "inherited": bool(library.is_inherited_component(data)),
               "blueprint_owner": _path(library.get_blueprint(data)),
               "can_delete": bool(library.can_delete(data))}
        entries.append((handle, obj, row))
    if len(roots) != 1 or not entries:
        raise RuntimeError("Expected one Blueprint root and a nonempty component hierarchy")
    paths = [row["object"] for _, _, row in entries]
    if len(paths) != len(set(paths)):
        raise RuntimeError("Component hierarchy contains duplicate object handles")
    return subsystem, roots[0], entries


def _component_inventory(bp):
    _, _, entries = _component_data(bp)
    rows = sorted((row for _, _, row in entries), key=lambda row: row["object"])
    families = {}
    for name in ("SovWeakPointComponent", "SovDismembermentComponent", "SovStatusComponent",
                 "NarrativeAbilitySystemComponent", "SovShieldComponent", "SovPoiseComponent"):
        cls = getattr(unreal, name)
        selected = [row for _, obj, row in entries if isinstance(obj, cls)]
        families[name] = {"count": len(selected), "native": sum(row["native"] for row in selected),
                          "components": [row["object"] for row in selected]}
    return {"scope": "native CDO components and complete Blueprint SCS hierarchy; no actor spawned",
            "total": len(rows), "native": sum(row["native"] for row in rows),
            "blueprint_components": sum(not row["native"] for row in rows),
            "families": families, "components": rows}


def _remove_elite_legacy_weak_point(bp, output, preserve, compile_bp):
    """Remove only this builder's local legacy SCS node; never a native component."""
    _outside_pie()
    expected = ROOT + "BP_AurelionElite.BP_AurelionElite"
    if _path(bp) != expected or not isinstance(unreal.get_default_object(bp.generated_class()), unreal.SovAurelionElite):
        raise RuntimeError("Legacy weak-point cleanup requires the exact owned native Elite Blueprint")
    subsystem, root_handle, entries = _component_data(bp)
    native = [(handle, obj, row) for handle, obj, row in entries
              if isinstance(obj, unreal.SovWeakPointComponent) and row["native"]]
    legacy = [(handle, obj, row) for handle, obj, row in entries
              if isinstance(obj, unreal.SovWeakPointComponent) and not row["native"]]
    core = unreal.get_default_object(bp.generated_class()).get_core_weak_points()
    if (len(native) != 1 or native[0][1] != core
            or not isinstance(core, unreal.SovAurelionCoreWeakPoints)
            or core.get_name() != "AurelionCoreWeakPoints"):
        raise RuntimeError("Elite requires exactly its native Core weak-point owner before cleanup")
    result = {"before": _component_inventory(bp), "removed": [], "idempotent": not legacy}
    if len(legacy) > 1:
        raise RuntimeError("Unexpected multiple legacy weak-point nodes require explicit review")
    if legacy:
        handle, obj, row = legacy[0]
        if (row["inherited"] or not row["can_delete"] or row["blueprint_owner"] != expected
                or row["variable"] != "SovWeakPoint" or obj.get_name() != "SovWeakPoint_GEN_VARIABLE"
                or obj.get_class() != unreal.SovWeakPointComponent.static_class()
                or not _path(obj).startswith(expected + "_C:")):
            raise RuntimeError("Refusing to delete an unexpected, inherited or externally owned weak-point node")
        # The editor's DeleteSubobject also removes variable getter nodes. Refuse
        # authored uses instead of silently dropping Blueprint gameplay wiring.
        export_path = output / "BP_AurelionElite-before-component-cleanup.t3d"
        task = unreal.AssetExportTask()
        for key, value in {"object": bp, "exporter": unreal.ObjectExporterT3D(),
                           "filename": str(export_path), "automated": True, "prompt": False,
                           "selected": False, "replace_identical": True}.items():
            task.set_editor_property(key, value)
        if not unreal.Exporter.run_asset_export_task(task) or not export_path.is_file():
            raise RuntimeError("Could not inspect Elite graph wiring before component cleanup")
        exported = export_path.read_text(encoding="utf-8-sig")
        if re.search(r'(?:MemberName|ComponentPropertyName|PinName)\s*=\s*"?SovWeakPoint\b', exported):
            raise RuntimeError("Legacy Elite weak point has authored graph uses; migrate those explicitly before deletion")
        core_zones = _encode(core.get_editor_property("weak_point_zones"))
        unchanged_rows = sorted((dict(item) for _, _, item in entries if item["object"] != row["object"]),
                                key=lambda item: item["object"])
        preserve()
        _outside_pie()
        if subsystem.delete_subobject(root_handle, handle, bp) != 1:
            raise RuntimeError("The owned legacy SCS weak-point deletion did not remove exactly one node")
        result["removed"].append(dict(row))
        cdo = compile_bp(bp)
        after = _component_inventory(bp)
        if after["components"] != unchanged_rows:
            raise RuntimeError("Elite cleanup changed another component; no assets will be saved")
        if _encode(cdo.get_core_weak_points().get_editor_property("weak_point_zones")) != core_zones:
            raise RuntimeError("Elite cleanup changed the native Core's authored matchers")
        preserve()
    result["after"] = _component_inventory(bp)
    if (result["after"]["families"]["SovWeakPointComponent"]["count"] != 1
            or result["after"]["families"]["SovWeakPointComponent"]["native"] != 1):
        raise RuntimeError("Elite must retain exactly one native weak-point owner")
    return result


def _dismemberment_configuration(component):
    return {field: _encode(component.get_editor_property(field)) for field in DISMEMBERMENT_FIELDS}


def _fingerprint_records(snapshot):
    records = {}
    for line in snapshot.splitlines():
        if line.startswith("Dirty="):
            continue
        parts = line.split("|", 2)
        if len(parts) != 3 or not re.fullmatch(r"[0-9A-Fa-f]{40}", parts[2]) or parts[0] in records:
            raise RuntimeError("Unrecognized authoring fingerprint record")
        records[parts[0]] = (parts[1], parts[2])
    if not records:
        raise RuntimeError("Empty Blueprint preservation fingerprint")
    return records


def _preserved_component_and_graph_records(bp, entries, excluded_component):
    records = _fingerprint_records(unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp))
    components = [row["object"] for _, _, row in entries if row["object"] != excluded_component]
    if any(path not in records for path in components):
        raise RuntimeError("Cannot fingerprint every component that cleanup must preserve")
    graphs = [path for path, value in records.items() if value[0] == "/Script/Engine.EdGraph"]
    if not graphs:
        raise RuntimeError("Cannot fingerprint the actual owned Blueprint graphs")
    roots = components + graphs
    # Individual non-transient reflected properties are hashed by the existing
    # native authoring helper. Keep every untouched component/child and all graph
    # objects; the Blueprint/SCS owner necessarily changes when one node is removed.
    return {path: value for path, value in records.items()
            if any(path == root or path.startswith(root + ".") or path.startswith(root + ":") for root in roots)}


def _export_dismemberment_graphs(bp, output, suffix):
    destination = output / (bp.get_name() + "-dismemberment-" + suffix + ".t3d")
    task = unreal.AssetExportTask()
    for key, value in {"object": bp, "exporter": unreal.ObjectExporterT3D(), "filename": str(destination),
                       "automated": True, "prompt": False, "selected": False, "replace_identical": True}.items():
        task.set_editor_property(key, value)
    if not unreal.Exporter.run_asset_export_task(task) or not destination.is_file():
        raise RuntimeError("Required dismemberment graph export failed")
    data = destination.read_bytes()
    exported = data.decode("utf-16") if data[:2] in (b"\xff\xfe", b"\xfe\xff") else data.decode("utf-8-sig")
    return exported, {"path": str(destination), "sha256": hashlib.sha256(data).hexdigest()}


def _legacy_dismemberment_graph_uses(exported):
    return [line.strip() for line in exported.splitlines()
            if re.search(r'(?:MemberName|ComponentPropertyName|PinName)\s*=\s*"?SovDismemberment\b', line)]


def _verify_drone_legacy_destroy_wiring(exported):
    # Preserve the exact existing getter-to-destroy branch, including its incoming
    # execution wire. Never redirect that destruction to the native component.
    stack, nodes = [], {}
    for line in exported.splitlines():
        stripped = line.strip()
        if stripped.startswith("Begin Object"):
            name = re.search(r'Name="([^"]+)"', stripped)
            stack.append({"name": name.group(1) if name else "", "parent": stack[-1]["name"] if stack else None, "lines": []})
        elif stripped == "End Object":
            row = stack.pop()
            if row["name"].startswith("K2Node_") and row["lines"]:
                nodes[(row["parent"], row["name"])] = row["lines"]
        elif stack:
            stack[-1]["lines"].append(stripped)
    uses = _legacy_dismemberment_graph_uses(exported)
    getters = [(name, lines) for name, lines in nodes.items()
               if any(re.match(r'VariableReference=.*MemberName="SovDismemberment"', line) for line in lines)]
    if len(uses) != 2 or len(getters) != 1:
        raise RuntimeError("Drone legacy graph uses changed; only the proven getter/destroy pair is admitted")
    (graph_name, getter_name), getter_lines = getters[0]
    output_pin = next((line for line in getter_lines if 'PinName="SovDismemberment"' in line), "")
    link = re.search(r'LinkedTo=\((K2Node_\w+) ([0-9A-Fa-f]{32}),\)', output_pin)
    if not link or 'Direction="EGPD_Output"' not in output_pin:
        raise RuntimeError("Drone legacy getter no longer has exactly one outgoing link")
    target_name, target_pin = link.groups()
    target_lines = nodes.get((graph_name, target_name), [])
    if not any('MemberParent="/Script/CoreUObject.Class\'/Script/Engine.ActorComponent\'"' in line
               and 'MemberName="K2_DestroyComponent"' in line for line in target_lines):
        raise RuntimeError("Drone legacy getter no longer targets ordinary component destruction")
    self_pin = next((line for line in target_lines if 'PinName="self"' in line), "")
    getter_pin = re.search(r'PinId="?([0-9A-Fa-f]{32})"?,', output_pin)
    if not re.search(r'PinId="?' + target_pin + r'"?,', self_pin) or not getter_pin:
        raise RuntimeError("Drone destroy target is not the legacy getter")
    if ('LinkedTo=(' + getter_name + ' ' + getter_pin.group(1) + ',)') not in self_pin:
        raise RuntimeError("Drone destroy target wiring changed")
    then_pin = next((line for line in target_lines if 'PinName="then"' in line), "")
    if not then_pin or 'LinkedTo=' in then_pin:
        raise RuntimeError("Drone destroy branch has new continuation; explicit review required")
    return {"graph": graph_name, "getter": getter_name, "destroy": target_name,
            "node_lines": {getter_name: getter_lines, target_name: target_lines}}


def _validate_dismemberment_ownership(bp, role, expected_native):
    _, _, entries = _component_data(bp)
    rows = [(obj, row, _dismemberment_configuration(obj)) for _, obj, row in entries
            if isinstance(obj, unreal.SovDismembermentComponent)]
    native = [(obj, row, cfg) for obj, row, cfg in rows if row["native"]]
    is_drone = role in ("SecurityDrone", "ContaminatedDrone")
    if len(native) != 1 or native[0][2] != expected_native:
        raise RuntimeError("Native dismemberment identity/profile changed: " + role)
    if native[0][0] != unreal.get_default_object(bp.generated_class()).get_dismemberment_component():
        raise RuntimeError("Native dismemberment getter lost its canonical component: " + role)
    enabled = sum(bool(cfg["dismemberment_enabled"]) for _, _, cfg in rows)
    blood = sum(bool(cfg["spawn_death_blood_puddle_on_death"]) for _, _, cfg in rows)
    if is_drone:
        if len(rows) != 2 or any(cfg[field] for _, _, cfg in rows for field in DRONE_DISABLED_FLAGS):
            raise RuntimeError("Drone must preserve two templates with zero enabled humanoid severing/blood owners")
    elif len(rows) != 1 or enabled != 1 or blood != int(bool(expected_native["spawn_death_blood_puddle_on_death"])):
        raise RuntimeError("Humanoid must retain exactly its enabled native dismemberment owner")
    return {"native_count": len(native), "component_count": len(rows), "enabled_severing_owners": enabled,
            "enabled_death_blood_owners": blood,
            "configurations": [{"metadata": row, "configuration": cfg} for _, row, cfg in rows]}


def _cleanup_owned_dismemberment(bp, role, output, preserve, compile_bp, result):
    _outside_pie()
    expected = ROOT + "BP_Aurelion" + role + ".BP_Aurelion" + role
    if role not in SEEDS or _path(bp) != expected:
        raise RuntimeError("Dismemberment cleanup requires an exact owned enemy role")
    native_type = getattr(unreal, SEEDS[role][1])
    cdo = unreal.get_default_object(bp.generated_class())
    if not isinstance(cdo, native_type):
        raise RuntimeError("Unexpected native enemy parent during component cleanup")
    subsystem, root_handle, entries = _component_data(bp)
    native = [(handle, obj, row) for handle, obj, row in entries
              if isinstance(obj, unreal.SovDismembermentComponent) and row["native"]]
    legacy = [(handle, obj, row) for handle, obj, row in entries
              if isinstance(obj, unreal.SovDismembermentComponent) and not row["native"]]
    is_drone = role in ("SecurityDrone", "ContaminatedDrone")
    expected_native_class = unreal.SovDroneDismembermentComponent if is_drone else unreal.SovDismembermentComponent
    if (len(native) != 1 or native[0][1] != cdo.get_dismemberment_component()
            or native[0][1].get_class() != expected_native_class.static_class()
            or native[0][1].get_name() != "SovDismembermentComponent" or len(legacy) > 1):
        raise RuntimeError("Expected one exact canonical native dismemberment component")
    native_configuration = _dismemberment_configuration(native[0][1])
    if (is_drone and any(native_configuration[field] for field in DRONE_DISABLED_FLAGS)) or (
            not is_drone and not native_configuration["dismemberment_enabled"]):
        raise RuntimeError("Native enemy dismemberment policy differs from the reviewed role")
    result.update({"before": _component_inventory(bp), "native_configuration": native_configuration,
              "removed": [], "changed_flags": [], "idempotent": False,
              "old_checkpoint_compatibility": "unqualified: no save-record migration or old-slot acceptance is claimed"})
    if is_drone and len(legacy) != 1:
        raise RuntimeError("Drone's authored legacy destruction target must remain present")
    exported, result["export_before"] = _export_dismemberment_graphs(bp, output, "before")
    if not is_drone and _legacy_dismemberment_graph_uses(exported):
        raise RuntimeError("Humanoid legacy dismemberment has graph uses; no deletion permitted")
    wiring = _verify_drone_legacy_destroy_wiring(exported) if is_drone else None
    if legacy:
        handle, component, metadata = legacy[0]
        if (metadata["inherited"] or not metadata["can_delete"] or metadata["blueprint_owner"] != expected
                or metadata["variable"] != "SovDismemberment" or component.get_name() != "SovDismemberment_GEN_VARIABLE"
                or component.get_class() != unreal.SovDismembermentComponent.static_class()
                or not _path(component).startswith(expected + "_C:")):
            raise RuntimeError("Refusing unexpected, inherited or external legacy dismemberment template")
        legacy_before = _dismemberment_configuration(component)
        if not is_drone and legacy_before != native_configuration:
            raise RuntimeError("All26 humanoid configuration fields must match native before deleting duplicate")
        result["legacy_configuration_before"] = legacy_before
        expected_rows = sorted((dict(row) for _, _, row in entries if is_drone or row != metadata), key=lambda row: row["object"])
        untouched_before = _preserved_component_and_graph_records(bp, entries, metadata["object"])
        core_before = _encode(cdo.get_core_weak_points().get_editor_property("weak_point_zones")) if role == "Elite" else None
        preserve()
        _outside_pie()
        if is_drone:
            for field in DRONE_DISABLED_FLAGS:
                if component.get_editor_property(field):
                    component.set_editor_property(field, False)
                    result["changed_flags"].append(field)
            expected_legacy = dict(legacy_before, **{field: False for field in DRONE_DISABLED_FLAGS})
            result["idempotent"] = not result["changed_flags"]
        else:
            if subsystem.delete_subobject(root_handle, handle, bp) != 1:
                raise RuntimeError("Humanoid cleanup did not remove exactly its one local SCS node")
            result["removed"].append(dict(metadata))
        cdo = compile_bp(bp)
        _, _, after_entries = _component_data(bp)
        result["component_rows_after"] = _component_inventory(bp)["components"]
        if result["component_rows_after"] != expected_rows:
            raise RuntimeError("Dismemberment cleanup changed another component's identity/ownership")
        untouched_after = _preserved_component_and_graph_records(bp, after_entries, metadata["object"])
        result["preserved_reflected_objects"] = len(untouched_before)
        result["preservation_fingerprint_differences"] = {
            path: {"before": untouched_before.get(path), "after": untouched_after.get(path)}
            for path in sorted(set(untouched_before) | set(untouched_after))
            if untouched_before.get(path) != untouched_after.get(path)}
        if result["preservation_fingerprint_differences"]:
            raise RuntimeError("Dismemberment cleanup changed another component's reflected properties or a graph")
        if role == "Elite" and _encode(cdo.get_core_weak_points().get_editor_property("weak_point_zones")) != core_before:
            raise RuntimeError("Dismemberment cleanup changed the native Elite Core zones")
        if is_drone:
            kept = [obj for _, obj, row in after_entries if row["object"] == metadata["object"]]
            if len(kept) != 1 or _dismemberment_configuration(kept[0]) != expected_legacy:
                raise RuntimeError("Drone cleanup changed fields outside the three approved flags")
        preserve()
    else:
        result["idempotent"] = True
    exported_after, result["export_after"] = _export_dismemberment_graphs(bp, output, "after")
    if is_drone:
        if _verify_drone_legacy_destroy_wiring(exported_after) != wiring:
            raise RuntimeError("Drone legacy destruction node/wiring was changed or retargeted")
        result["retained_legacy_wiring"] = wiring
    elif _legacy_dismemberment_graph_uses(exported_after):
        raise RuntimeError("Humanoid cleanup introduced a legacy component graph reference")
    result["after"] = _validate_dismemberment_ownership(bp, role, native_configuration)
    return result


def _ensure_placed_lifecycle_tag(definition):
    """Keep all authored tags and opt this owned placement out of settlement cleanup."""
    if not _path(definition).startswith(ROOT):
        raise RuntimeError("Cannot change lifecycle tags outside owned enemy definitions")
    # UE 5.7 BlueprintGameplayTagLibrary.h explicitly exports ScriptName GameplayTagLibrary.
    # Break/Make return an independent container; no in-place source struct is edited.
    library = unreal.GameplayTagLibrary
    required = unreal.GameplayTag()
    if (not required.import_text('(TagName="' + PLACED_LIFECYCLE_TAG + '")')
            or not library.is_gameplay_tag_valid(required)
            or str(library.get_tag_name(required)) != PLACED_LIFECYCLE_TAG):
        raise RuntimeError("Required registered gameplay tag is missing: " + PLACED_LIFECYCLE_TAG)
    existing = definition.get_editor_property("default_owned_tags")
    tags = list(library.break_gameplay_tag_container(existing))
    original = library.make_gameplay_tag_container_from_array(tags)
    already_present = library.has_tag(original, required, True)
    if not already_present:
        tags.append(required)
    merged = library.make_gameplay_tag_container_from_array(tags)
    expected_count = library.get_num_gameplay_tags_in_container(original) + (0 if already_present else 1)
    if (not library.has_all_tags(merged, original, True)
            or library.get_num_gameplay_tags_in_container(merged) != expected_count):
        raise RuntimeError("Owned enemy tag merge did not preserve existing explicit tags")
    definition.set_editor_property("default_owned_tags", merged)
    actual = definition.get_editor_property("default_owned_tags")
    if (not library.has_tag(actual, required, True)
            or not library.has_all_tags(actual, original, True)
            or library.get_num_gameplay_tags_in_container(actual) != expected_count):
        raise RuntimeError("Owned enemy definition did not retain its exact lifecycle tag")
    return {"before": original.export_text(), "after": actual.export_text(),
            "required_tag": PLACED_LIFECYCLE_TAG, "already_present": already_present}


def build_enemy_assets(output_dir):
    """Return role -> {definition, blueprint, class, activity_config} after guarded saves.

    Source attack configurations, loadouts, factions and appearances are retained;
    the new roles add physical traversal or severable support through Narrative's
    scheduler. ContaminatedDrone is a distinct definition/presentation identity;
    its ordinary attack remains the proven drone attack, not a new corrosion payload.
    """
    _outside_pie()
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=True)
    report = {"status": "preflight", "schema": SCHEMA, "created": [], "saved": [], "roles": {},
              "source_disk_unchanged": False, "source_blueprints_unchanged": False,
              "source_definition_fields_unchanged": False, "gameplay_qualified": False}
    helper = unreal.SovAurelionEnemyAuthoringLibrary
    originals, blueprints, source_fields, seed_info, pending = {}, {}, {}, {}, {}
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    def remember(obj):
        if not _path(obj).startswith(ROOT):
            raise RuntimeError("Unowned mutation/save rejected: " + str(_path(obj)))
        pending[_path(obj)] = obj
        return obj

    def compile_bp(bp):
        remember(bp)
        if not helper.compile_owned_blueprint(bp):
            raise RuntimeError("Owned enemy Blueprint failed native compilation: " + _path(bp))
        return unreal.get_default_object(bp.generated_class())

    def duplicate(source, destination):
        if not destination.startswith(ROOT):
            raise RuntimeError("Invalid enemy destination")
        if unreal.EditorAssetLibrary.does_asset_exist(destination):
            obj = unreal.load_asset(destination)
            if unreal.EditorAssetLibrary.get_metadata_tag(obj, STAMP) != SCHEMA:
                raise RuntimeError("Existing unowned/older destination requires review: " + destination)
        else:
            obj = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
            if not obj:
                raise RuntimeError("Could not duplicate " + destination)
            report["created"].append(destination)
        return remember(obj)

    def native_child(native_type, destination):
        if unreal.EditorAssetLibrary.does_asset_exist(destination):
            bp = unreal.load_asset(destination)
            if unreal.EditorAssetLibrary.get_metadata_tag(bp, STAMP) != SCHEMA:
                raise RuntimeError("Existing activity destination is not this builder's asset: " + destination)
        else:
            folder, name = destination.rsplit("/", 1)
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", native_type)
            bp = tools.create_asset(name, folder, unreal.Blueprint, factory)
            if not bp:
                raise RuntimeError("Could not create role activity " + destination)
            report["created"].append(destination)
        if not isinstance(compile_bp(bp), native_type):
            raise RuntimeError("Role activity parent differs from expected native class")
        return bp

    def preserve():
        report["source_disk_after"] = _disk_files(originals)
        report["source_blueprints_after"] = {p: unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp) for p, bp in blueprints.items()}
        report["source_definition_fields_after"] = {p: {f: _encode(obj.get_editor_property(f)) for f in DEF_FIELDS} for p, obj in source_fields.items()}
        report["source_disk_unchanged"] = report["source_disk_after"] == report["source_disk_before"]
        report["source_blueprints_unchanged"] = report["source_blueprints_after"] == report["source_blueprints_before"]
        report["source_definition_fields_unchanged"] = report["source_definition_fields_after"] == report["source_definition_fields_before"]
        if not all(report[key] for key in ("source_disk_unchanged", "source_blueprints_unchanged", "source_definition_fields_unchanged")):
            raise RuntimeError("Source preservation failed; no further saves")

    try:
        # Materialize all source classes/CDOs and appearance/config dependencies before snapshot.
        for role, (source_path, native_name) in SEEDS.items():
            source = _load(source_path)
            cls = _load(source.get_editor_property("npc_class_path"))
            if not isinstance(cls, unreal.Class):
                raise RuntimeError("NPCClassPath did not load an actual class: " + role)
            bp_path = _path(cls).removesuffix("_C").split(".")[0]
            source_bp = _load(bp_path)
            unreal.get_default_object(cls)
            # The similarly named Narrative/NPC_ReformationDrone is an appearance
            # template with no activity/ability configuration. The Textures-path
            # combat definition owns the real stock RunAndGun and drone attacks.
            appearance = _load(source.get_editor_property("default_appearance"))
            activity = _load(source.get_editor_property("activity_configuration"))
            ability = _load(source.get_editor_property("ability_configuration"))
            if not activity.get_editor_property("default_activities") or not activity.get_editor_property("goal_generators"):
                raise RuntimeError("Seed lacks actual Narrative activities/goal generators: " + role)
            report.setdefault("seed_configurations", {})[role] = {
                "definition": _path(source), "class": _path(cls),
                "appearance": _path(appearance), "activity": _path(activity), "ability": _path(ability)}
            for obj in (source, source_bp, appearance, activity, ability):
                originals[_path(obj).split(".")[0]] = obj
            blueprints[bp_path] = source_bp
            source_fields[source_path] = source
            seed_info[role] = (source, source_bp, appearance, activity, ability, getattr(unreal, native_name))
        unreal.collect_garbage()
        report["source_disk_before"] = _disk_files(originals)
        report["source_blueprints_before"] = {p: unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp) for p, bp in blueprints.items()}
        report["source_definition_fields_before"] = {p: {f: _encode(obj.get_editor_property(f)) for f in DEF_FIELDS} for p, obj in source_fields.items()}

        role_activities = {}
        for role, native_name, is_wall in (("WallRunner", "SovAurelionWallTraversalActivity", True), ("Weaver", "SovAurelionWeaverSupportActivity", False)):
            tree_result = helper.create_role_tree(is_wall)
            if not tree_result.succeeded:
                raise RuntimeError(str(tree_result.error))
            remember(tree_result.tree)
            suffix = "WallTraversal" if is_wall else "WeaverSupport"
            activity_bp = native_child(getattr(unreal, native_name), ROOT + "BPA_Aurelion" + suffix)
            if not helper.configure_role_activity(activity_bp, is_wall):
                raise RuntimeError("Could not bind role activity to its verified tree")
            compile_bp(activity_bp)
            role_activities[role] = activity_bp.generated_class()

        for role, (source, source_bp, appearance, activity, ability, native_type) in seed_info.items():
            bp = duplicate(_path(source_bp).split(".")[0], ROOT + "BP_Aurelion" + role)
            unreal.BlueprintEditorLibrary.reparent_blueprint(bp, native_type)
            cdo = compile_bp(bp)
            if not isinstance(cdo, native_type):
                raise RuntimeError("Native role parent mismatch: " + role)
            identity_result = unreal.SovAurelionNPCIdentityLibrary.repair_owned_early_npc_identity(bp)
            report.setdefault("early_npc_identity_graphs", {})[role] = str(identity_result.report)
            if not identity_result.succeeded:
                raise RuntimeError("Owned early NPC identity repair refused: " + role + ": " + str(identity_result.report))
            cdo = unreal.get_default_object(bp.generated_class())
            if role == "Elite":
                report["elite_component_cleanup"] = _remove_elite_legacy_weak_point(bp, output, preserve, compile_bp)
                cdo = unreal.get_default_object(bp.generated_class())
            dismemberment_cleanup = {"status": "preflight", "role": role}
            report.setdefault("dismemberment_cleanup", {})[role] = dismemberment_cleanup
            _cleanup_owned_dismemberment(bp, role, output, preserve, compile_bp, dismemberment_cleanup)
            dismemberment_cleanup["status"] = "passed: guarded authored template cleanup; fresh runtime/save verification pending"
            cdo = unreal.get_default_object(bp.generated_class())
            # Use the actual stock controller class, whose BP configures Sight/goal observation.
            source_cdo = unreal.get_default_object(source_bp.generated_class())
            ai_controller = source_cdo.get_editor_property("ai_controller_class")
            if not ai_controller:
                ai_controller = _load("/NarrativePro/Pro/Core/AI/BP/BP_NarrativeNPCController").generated_class()
            cdo.set_editor_property("ai_controller_class", ai_controller)
            cdo.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
            if role == "Weaver":
                cdo.get_anchor_a().set_editor_property("link_id", "Aurelion.Weaver.AnchorA")
                cdo.get_anchor_b().set_editor_property("link_id", "Aurelion.Weaver.AnchorB")
            definition = duplicate(_path(source).split(".")[0], ROOT + "NPC_Aurelion" + role)
            definition.set_editor_property("npc_class_path", bp.generated_class())
            definition.set_editor_property("character_id", "Aurelion_" + role)
            definition.set_editor_property("npc_name", unreal.Text("Aurelion " + role))
            definition.set_editor_property("allow_multiple_instances", True)
            lifecycle_tags = _ensure_placed_lifecycle_tag(definition)
            # Combat roles do not import demo vendor/dialogue behavior or currency rewards.
            definition.set_editor_property("is_vendor", False)
            definition.set_editor_property("trading_currency", 0)
            definition.set_editor_property("trading_item_loadout", [])
            definition.set_editor_property("default_currency", 0)
            definition.set_editor_property("dialogue", None)
            definition.set_editor_property("tagged_dialogue_set", None)
            configuration = duplicate(_path(activity).split(".")[0], ROOT + "AC_Aurelion" + role)
            activities = list(configuration.get_editor_property("default_activities"))
            if role in role_activities:
                activities = [role_activities[role]] + [a for a in activities if _path(a) != _path(role_activities[role])]
                configuration.set_editor_property("default_activities", activities)
            if not activities or not configuration.get_editor_property("goal_generators"):
                raise RuntimeError("Seed lacks actual Narrative activities/goal generators: " + role)
            definition.set_editor_property("activity_configuration", configuration)
            if role == "Elite":
                # Request surfaces require a live component, including after native retry reconstruction.
                cdo.get_thermal_fracture().set_editor_property("auto_activate", True)
                ability = duplicate(_path(ability).split(".")[0], ROOT + "AC_Abilities_AurelionElite")
                startup = list(ability.get_editor_property("startup_effects"))
                # Poise, then boss durability. The seeded elite inherits an ordinary enforcer's health
                # pool; the durability effect overrides it after those base attributes are applied.
                for effect_class in (unreal.SovAurelionElitePoiseAttributes, unreal.SovAurelionEliteDurability):
                    effect = effect_class.static_class()
                    if _path(effect) not in [_path(existing) for existing in startup]:
                        startup.append(effect)
                ability.set_editor_property("startup_effects", startup)
                # The boss repertoire. Narrative's bot selection chooses among everything the elite
                # owns, so these join the seeded enforcer's existing attacks rather than replacing
                # them: the elite still swings, and now also slams, lances and calls reinforcements.
                granted = list(ability.get_editor_property("default_abilities"))
                for ability_class in (unreal.SovGameplayAbility_AurelionEliteSlam,
                                      unreal.SovGameplayAbility_AurelionEliteLance,
                                      unreal.SovGameplayAbility_AurelionEliteSummon):
                    boss_ability = ability_class.static_class()
                    if _path(boss_ability) not in [_path(existing) for existing in granted]:
                        granted.append(boss_ability)
                ability.set_editor_property("default_abilities", granted)
                definition.set_editor_property("ability_configuration", ability)
                # CharacterAppearance's runtime getter unconditionally dereferences
                # its requester for variation seeds. BaseMesh is authored directly
                # in CharacterAttributes and is never varied by that getter.
                attributes = appearance.get_editor_property("character_attributes")
                mesh = attributes.get_editor_property("base_mesh")
                if not mesh:
                    raise RuntimeError("Elite source appearance has no actual skeletal base mesh")
                matched = None
                for bone in ("spine_03", "spine_02", "spine_01", "chest"):
                    if helper.configure_elite_core(bp, mesh, bone):
                        matched = bone
                        break
                if not matched:
                    raise RuntimeError("No verified torso bone available for elite Core")
                report["elite_core"] = {"zone": "Core", "mesh": _path(mesh), "verified_bone": matched,
                                        "mesh_source": "authored CharacterAttributes.BaseMesh; native RefSkeleton validation"}
            cdo = compile_bp(bp)
            component_inventory = _component_inventory(bp)
            dismemberment_cleanup["final_ownership"] = _validate_dismemberment_ownership(
                bp, role, dismemberment_cleanup["native_configuration"])
            for family in ("NarrativeAbilitySystemComponent", "SovStatusComponent"):
                if (component_inventory["families"][family]["count"] != 1
                        or component_inventory["families"][family]["native"] != 1):
                    raise RuntimeError(role + " needs exactly one native " + family + "; no broad cleanup is permitted")
            if role == "Elite":
                if (component_inventory["families"]["SovWeakPointComponent"]["count"] != 1
                        or not cdo.get_core_weak_points().has_valid_weak_point_configuration()):
                    raise RuntimeError("Elite core binding or weak-point ownership was lost after final compilation")
            if _path(_load(definition.get_editor_property("npc_class_path"))) != _path(bp.generated_class()):
                raise RuntimeError("Definition class and actor class differ: " + role)
            report["roles"][role] = {"definition": _path(definition), "blueprint": _path(bp), "class": _path(bp.generated_class()),
                "activity_config": _path(configuration), "activities": [_path(a) for a in activities],
                "ability_config": _path(ability), "appearance": _path(appearance),
                "source": _path(source), "controller": _path(ai_controller),
                "placed_lifecycle_tags": lifecycle_tags,
                "dismemberment_cleanup": dismemberment_cleanup,
                "component_inventory": component_inventory,
                "native_components": [c.get_name() for c in cdo.get_components_by_class(unreal.ActorComponent)]}

        if set(report["roles"]) != set(SEEDS):
            raise RuntimeError("Every owned enemy role must receive its lifecycle policy before saves")
        for role in SEEDS:
            # Re-read before the first save; subsequent role work must not have lost the tag.
            _ensure_placed_lifecycle_tag(pending[report["roles"][role]["definition"]])
            _validate_dismemberment_ownership(pending[report["roles"][role]["blueprint"]], role,
                report["roles"][role]["dismemberment_cleanup"]["native_configuration"])
        preserve()
        for obj in pending.values():
            _outside_pie()
            preserve()
            unreal.EditorAssetLibrary.set_metadata_tag(obj, STAMP, SCHEMA)
            if not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
                raise RuntimeError("Enemy package save failed: " + _path(obj))
            report["saved"].append(_path(obj))
        preserve()
        report["status"] = "passed: authored assets; live behavior awaits validation"
        return report["roles"]
    except Exception as exc:
        report["status"] = "failed"
        report["error"] = str(exc)
        raise
    finally:
        (output / "enemy-role-assets.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


def wall_route_spec(capsule_half_height, capsule_radius):
    """Centimeter-local geometry used beside an authored WallRunner marker.

    Root authoring places the runner at point0 above the actual floor and creates
    the two blocking boxes. This specification never edits a world. The route
    rises300cm, traverses the ledge and drops onto the far ground flank.
    """
    h, r = float(capsule_half_height), float(capsule_radius)
    if not math.isfinite(h) or not math.isfinite(r) or not 30 <= r <= 70 or not 60 <= h <= 140:
        raise ValueError("Wall route needs the actual supported humanoid capsule dimensions")
    height = 300.0
    wall_y = r + 50.0
    return {
        "local_points": [[0, 0, h + 2], [0, 0, height + h + 2], [280, 0, height + h + 2],
                         [520, 0, height + h + 2], [520, 0, h + 2]],
        "wall_probe_direction": [0, 1, 0], "wall_probe_distance": wall_y + 30,
        "speed": 350.0, "entry_tolerance": 60.0, "maximum_duration": 8.0,
        "geometry": [
            {"suffix": "ClimbWall", "center": [130, wall_y, height / 2], "extent": [220, 10, height / 2]},
            {"suffix": "FlankLanding", "center": [280, 0, height - 10], "extent": [100, r + 35, 10]},
        ],
        "note": "Full capsule sweeps and real wall/floor traces remain required at runtime. No nav-link teleport.",
    }
