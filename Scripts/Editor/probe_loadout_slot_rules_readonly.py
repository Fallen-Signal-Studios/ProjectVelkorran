"""Read both protagonists' actual item slot defaults and proposed first-free assignments."""
import json
import os
import runpy
from pathlib import Path
import unreal

SCRIPT_DIR = Path(__file__).parent
OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
WORK = OUTPUT_DIR
helpers = runpy.run_path(str(SCRIPT_DIR / "protagonist_equipment_order.py"))
SOURCES = {"Velkorran": "/Game/WeaponMeshes/Velkorran/NWI_Velkorran", "Cinderline": "/Game/SCF_Rifle_02/NWI_Cinderline",
           "Verity": "/Game/WeaponMeshes/NWI_Verity", "Staccato": "/Game/WeaponMeshes/Weapon_Staccato", "Axiom": "/Game/WeaponMeshes/Weapon_Axiom"}
report = {"source_rules": helpers["weapon_rules"]({n: unreal.load_asset(p) for n, p in SOURCES.items()}), "heroes": {}}
copies = {n: unreal.load_asset("/Game/Items/Weapons/WI_" + n) for n in SOURCES}
for hero in ("Tarrik", "Selene"):
    collection = unreal.load_asset("/Game/Items/Loadouts/IC_" + hero)
    try:
        _, details = helpers["arrange_equipped_loadout"](hero, collection.get_editor_property("items"), copies)
        report["heroes"][hero] = details
    except Exception as exc:
        report["heroes"][hero] = {"error": str(exc), "project_rules": helpers["weapon_rules"](copies)}
out = WORK / "loadout-slot-rules.json"
out.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("VELKORRAN_LOADOUT_SLOT_RULES " + str(out))
