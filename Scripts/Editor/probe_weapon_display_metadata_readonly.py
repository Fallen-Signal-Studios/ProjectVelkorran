"""Inspect authored weapon names/icons and live inventory instances; no writes."""
import json
import os
from pathlib import Path
import unreal

OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUT = OUTPUT_DIR / "weapon-display-metadata.json"
SOURCES = {
    "Velkorran": "/Game/WeaponMeshes/Velkorran/NWI_Velkorran",
    "Cinderline": "/Game/SCF_Rifle_02/NWI_Cinderline",
    "Verity": "/Game/WeaponMeshes/NWI_Verity",
    "Staccato": "/Game/WeaponMeshes/Weapon_Staccato",
    "Axiom": "/Game/WeaponMeshes/Weapon_Axiom",
}
THUMBS = "/NarrativePro/Pro/Demo/Items/Examples/Items/Thumbnails/"


def metadata(item):
    result = {"path": item.get_path_name(), "class": item.get_class().get_path_name()}
    for field in ("display_name", "thumbnail"):
        try:
            value = item.get_editor_property(field)
            result[field] = value.get_path_name() if isinstance(value, unreal.Object) else str(value) if value is not None else None
        except Exception as exc:
            result[field + "_error"] = str(exc)
    try:
        result["weapon_display_name"] = str(item.get_weapon_display_name(False, False))
    except Exception as exc:
        result["weapon_display_name_error"] = str(exc)
    return result


report = {"templates": {}, "live_items": [], "existing_icon_candidates": {}}
for name, source_path in SOURCES.items():
    report["templates"][name] = {}
    for role, path in (("source", source_path), ("project", "/Game/Items/Weapons/WI_" + name)):
        asset = unreal.load_asset(path)
        report["templates"][name][role] = metadata(unreal.get_default_object(asset.generated_class())) if asset else {"missing": path}
for name in ("T_ExampleRifle", "T_ExamplePistol", "T_Thumb_Sword", "T_Thumb_Greatsword"):
    path = THUMBS + name
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    report["existing_icon_candidates"][path] = {"exists": unreal.EditorAssetLibrary.does_asset_exist(path),
        "registry_class": str(data.asset_class_path)}
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if world:
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    report["pawn"] = pawn.get_path_name() if pawn else None
    classes = {row["project"].get("class") for row in report["templates"].values()}
    for item in unreal.ObjectIterator(unreal.NarrativeItem):
        if item.get_class().get_path_name() not in classes:
            continue
        try:
            if item.get_owning_pawn() == pawn and pawn:
                report["live_items"].append(metadata(item))
        except Exception:
            pass
OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("VELKORRAN_WEAPON_DISPLAY_METADATA " + str(OUT))
