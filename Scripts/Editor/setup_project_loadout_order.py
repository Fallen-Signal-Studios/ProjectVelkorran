"""Fix only copied Selene grant order after verifying actual native slot contracts."""
import hashlib
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
OUT = WORK / "project-loadout-order-setup.json"
DEFS = "/NarrativePro/Pro/Demo/Character/Definitions/Player/"


def snapshot(collection):
    return [entry.export_text() for entry in collection.get_editor_property("items")]


def run():
    report = {"status": "preflight", "heroes": {}, "saved": []}
    sources = {}
    try:
        if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError("End PIE before editing project loadout order")
        helpers = runpy.run_path(str(SCRIPT_DIR / "protagonist_equipment_order.py"))
        copies = {n: unreal.load_asset("/Game/Items/Weapons/WI_" + n) for n in ("Velkorran", "Cinderline", "Verity", "Staccato", "Axiom")}
        project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
        plugins = list((project / "Plugins").glob("*/NarrativePro.uplugin"))
        if len(plugins) != 1:
            raise RuntimeError("Expected one NarrativePro mount")
        report["source_before"] = {}
        proposals = {}
        for hero in ("Tarrik", "Selene"):
            source_path = DEFS + "IC_Default" + hero + "Items"
            source = unreal.load_asset(source_path)
            sources[hero] = (source, plugins[0].parent / "Content" / (source_path.removeprefix("/NarrativePro/") + ".uasset"))
            report["source_before"][hero] = {"items": snapshot(source), "sha256": hashlib.sha256(sources[hero][1].read_bytes()).hexdigest()}
            target = unreal.load_asset("/Game/Items/Loadouts/IC_" + hero)
            entries, details = helpers["arrange_equipped_loadout"](hero, target.get_editor_property("items"), copies)
            report["heroes"][hero] = details
            proposals[hero] = (target, [entry.export_text() for entry in entries])
        if report["heroes"]["Tarrik"]["changed"]:
            raise RuntimeError("Tarrik's already-valid order must remain unchanged")
        target, ordered_text = proposals["Selene"]
        if report["heroes"]["Selene"]["changed"]:
            # Reconstruct new structs; do not mutate any source-backed property views.
            entries = []
            for text in ordered_text:
                entry = unreal.ItemWithQuantity()
                if not entry.import_text(text):
                    raise RuntimeError("Could not preserve exact item row")
                entries.append(entry)
            target.set_editor_property("items", entries)
            if snapshot(target) != ordered_text:
                raise RuntimeError("Project collection did not retain the intended order")
            if not unreal.EditorAssetLibrary.save_loaded_asset(target, False):
                raise RuntimeError("Could not save copied Selene collection")
            report["saved"].append(target.get_path_name())
        report["status"] = "validated both loadouts; Selene reserves BackA for Verity before Staccato; fresh PIE required"
    except Exception as exc:
        report["error"] = str(exc)
        report["status"] = "stopped; preserve editor state for review"
        raise
    finally:
        if sources:
            report["source_after"] = {hero: {"items": snapshot(source), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                                      for hero, (source, path) in sources.items()}
            report["source_collections_unchanged"] = report["source_after"] == report["source_before"]
        OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("VELKORRAN_PROJECT_LOADOUT_ORDER " + str(OUT))
        if report.get("source_collections_unchanged") is False:
            raise RuntimeError("Original loadout preservation failed")


run()
