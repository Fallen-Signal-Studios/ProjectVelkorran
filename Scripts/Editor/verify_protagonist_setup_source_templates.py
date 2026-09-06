"""Rerun setup and verify the source templates' reflected data remains unchanged.

Run inside Unreal Editor outside PIE. Sources are loaded before the first snapshot
so any UE-version migration-on-load is separated from script mutations. This
verification never saves, reloads or discards a source template to hide a failure.
"""
import json
import os
import runpy
from pathlib import Path
import unreal

OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUT = OUTPUT_DIR / "protagonist-source-preservation-check.json"
INPUT = "/NarrativePro/Pro/Core/Data/Input/"
DEFS = "/NarrativePro/Pro/Demo/Character/Definitions/Player/"
AC = "/NarrativePro/Pro/Core/Abilities/Configurations/"
SOURCES = {
    INPUT + "IMC_Default": ["default_key_mappings"],
    INPUT + "DA_DefaultAbilityInputs": ["input_abilities"],
    DEFS + "CD_DefaultPlayer": ["default_item_loadout", "ability_configuration", "default_owned_tags"],
    DEFS + "CD_Selene": ["default_item_loadout", "ability_configuration", "default_owned_tags"],
    DEFS + "IC_DefaultTarrikItems": ["items"],
    DEFS + "IC_DefaultSeleneItems": ["items"],
    AC + "AC_Player_Shooter": ["default_abilities", "startup_effects", "default_attributes"],
    AC + "AC_Player_Selene": ["default_abilities", "startup_effects", "default_attributes"],
    "/NarrativePro/Pro/Core/Abilities/GameplayEffects/Attributes/GE_DefaultPlayerAttributes": ["modifiers"],
}
for weapon in ("/Game/WeaponMeshes/Velkorran/NWI_Velkorran", "/Game/SCF_Rifle_02/NWI_Cinderline",
               "/Game/WeaponMeshes/NWI_Verity", "/Game/WeaponMeshes/Weapon_Staccato", "/Game/WeaponMeshes/Weapon_Axiom"):
    SOURCES[weapon] = ["weapon_abilities", "mainhand_weapon_abilities", "offhand_weapon_abilities", "weapon_visual_class", "require_same_class_for_dual_wield"]


def encode(value):
    if value is None:
        return None
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (list, tuple, unreal.Array)):
        return [encode(item) for item in value]
    if hasattr(value, "export_text"):
        return value.export_text()
    return str(value)


def snapshot():
    result = {}
    for path, fields in SOURCES.items():
        obj = unreal.load_asset(path)
        if not obj:
            raise RuntimeError("Missing source template: " + path)
        if isinstance(obj, unreal.Blueprint):
            obj = unreal.get_default_object(obj.generated_class())
        result[path] = {field: encode(obj.get_editor_property(field)) for field in fields}
    return result


def dirty_packages():
    try:
        return sorted(package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    except Exception as exc:
        return ["unavailable: " + str(exc)]


report = {"status": "preflight", "source_templates": list(SOURCES)}
try:
    report["before"] = snapshot()
    report["dirty_before"] = dirty_packages()
    base = Path(__file__).parent
    runpy.run_path(str(base / "setup_protagonist_native_kits.py"), run_name="__main__")
    runpy.run_path(str(base / "setup_protagonist_projectile_presentation.py"), run_name="__main__")
    report["after"] = snapshot()
    report["dirty_after"] = dirty_packages()
    report["changed_sources"] = {path: {"before": report["before"][path], "after": report["after"][path]}
                                  for path in SOURCES if report["before"][path] != report["after"][path]}
    if report["changed_sources"]:
        raise RuntimeError("Source template changes detected: " + ", ".join(report["changed_sources"]))
    report["status"] = "passed: {} loaded source templates unchanged by complete setup rerun".format(len(SOURCES))
except Exception as exc:
    report["error"] = str(exc)
    report["status"] = "failed; preserve current editor state for inspection"
    raise
finally:
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("VELKORRAN_SOURCE_PRESERVATION_CHECK " + str(OUT))
