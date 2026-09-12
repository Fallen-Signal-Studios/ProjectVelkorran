"""Classify the content prerequisites the automation suite depends on. Read-only.

Answers, with loaded assets rather than by grepping .uasset bytes:
  1. Which cue notify roots actually contain cue notifies in this checkout.
  2. Whether the disputed test assets exist here.
  3. Whether the tracked production NPCDefinitions satisfy the preconditions the placed-NPC
     tests require of their seed, which decides whether those tests can be repointed at
     version-controlled production data instead of an untracked asset.

Nothing is modified or saved.
"""
import json
import os
from collections import defaultdict
from pathlib import Path

import unreal

OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "ContentPrerequisites")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
editor_assets = unreal.EditorAssetLibrary

DISPUTED = [
    "/Game/SciFi_Drone_1/Textures/NPC_ReformationCombatDrone",
    "/Game/Cues",
]
CANDIDATE_DEFINITIONS = [
    "/Game/Aurelion/Enemies/NPC_AurelionSecurityDrone",
    "/Game/Aurelion/Enemies/NPC_AurelionContaminatedDrone",
    "/Game/Aurelion/Enemies/NPC_AurelionEnforcer",
    "/Game/Aurelion/Enemies/NPC_AurelionElite",
]
CUE_MARKER = "GameplayCueNotify"


def native_parent(asset):
    tag = asset.get_tag_value("NativeParentClass")
    return str(tag) if tag else str(asset.asset_class_path.asset_name)


def soft_value(owner, name):
    """Resolve a property that may be an object, a soft path, or unset."""
    try:
        value = owner.get_editor_property(name)
    except Exception as error:
        return {"available": False, "error": str(error)}
    if value is None:
        return {"available": True, "set": False, "value": None}
    text = str(value)
    resolved = None
    for attribute in ("get_path_name", "to_string"):
        if hasattr(value, attribute):
            try:
                resolved = str(getattr(value, attribute)())
                break
            except Exception:
                pass
    is_set = bool(text) and text not in ("None", "", "None'None'")
    return {"available": True, "set": is_set, "value": resolved or text}


report = {
    "checkout": {
        "project_dir": str(unreal.Paths.project_dir()),
        "engine_version": unreal.SystemLibrary.get_engine_version(),
    },
    "cue_notifies_by_root": defaultdict(list),
    "cue_notify_total": 0,
    "disputed_assets": {},
    "candidate_definitions": {},
}

for asset in registry.get_all_assets():
    if CUE_MARKER not in native_parent(asset):
        continue
    path = str(asset.package_name)
    report["cue_notify_total"] += 1
    root = "/Game/Cues" if path.startswith("/Game/Cues") else (
        "/NarrativePro/Pro/Core/Abilities/Cues"
        if path.startswith("/NarrativePro/Pro/Core/Abilities/Cues") else "other")
    report["cue_notifies_by_root"][root].append(path)

for path in DISPUTED:
    entry = {"asset_exists": bool(editor_assets.does_asset_exist(path))}
    try:
        entry["directory_exists"] = bool(editor_assets.does_directory_exist(path))
    except Exception:
        entry["directory_exists"] = None
    report["disputed_assets"][path] = entry

for path in CANDIDATE_DEFINITIONS:
    entry = {"exists": bool(editor_assets.does_asset_exist(path))}
    if entry["exists"]:
        definition = unreal.load_asset(path)
        entry["class"] = type(definition).__name__ if definition else None
        configuration_field = soft_value(definition, "ability_configuration") if definition else {}
        entry["ability_configuration"] = configuration_field
        configuration = None
        if definition is not None:
            try:
                raw = definition.get_editor_property("ability_configuration")
                configuration = raw if raw is not None and hasattr(raw, "get_editor_property") else None
            except Exception as error:
                entry["configuration_error"] = str(error)
        if configuration is not None:
            entry["default_attributes"] = soft_value(configuration, "default_attributes")
            try:
                abilities = configuration.get_editor_property("default_abilities")
                entry["default_abilities_count"] = len(abilities) if abilities is not None else 0
            except Exception as error:
                entry["default_abilities_error"] = str(error)
        else:
            entry["configuration_resolved"] = False
    report["candidate_definitions"][path] = entry

report["cue_notifies_by_root"] = dict(report["cue_notifies_by_root"])
out = OUTPUT_DIR / "content-prerequisites.json"
out.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("VELKORRAN_CONTENT_PREREQUISITES " + str(out))
