"""Set canonical names and fill confirmed missing generic icons on project copies.

Run outside PIE. Uses existing Narrative Texture2D assets, creates no art, and
preserves original item templates plus every already-authored project icon.
"""
import hashlib
import json
import os
from pathlib import Path
import unreal

SCRIPT_DIR = Path(__file__).parent
OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
WORK = OUTPUT_DIR
OUT = WORK / "project-weapon-display-setup.json"
SOURCES = {
    "Velkorran": "/Game/WeaponMeshes/Velkorran/NWI_Velkorran",
    "Cinderline": "/Game/SCF_Rifle_02/NWI_Cinderline",
    "Verity": "/Game/WeaponMeshes/NWI_Verity",
    "Staccato": "/Game/WeaponMeshes/Weapon_Staccato",
    "Axiom": "/Game/WeaponMeshes/Weapon_Axiom",
}
ICON_ROOT = "/NarrativePro/Pro/Demo/Items/Examples/Items/Thumbnails/"
# These are existing prototype silhouettes, not newly authored custom weapon art.
MISSING_ICONS = {"Velkorran": "T_Thumb_Greatsword", "Cinderline": "T_ExampleRifle", "Verity": "T_Thumb_Sword"}


def snapshot(bp):
    cdo = unreal.get_default_object(bp.generated_class())
    icon = cdo.get_editor_property("thumbnail")
    return {"display_name": str(cdo.get_editor_property("display_name")),
            "thumbnail": icon.get_path_name() if icon else None,
            "derived_weapon_name": str(cdo.get_weapon_display_name(False, False))}


def fingerprint(bp):
    raw = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp)
    dirty, separator, properties = raw.partition("\n")
    if not separator or not dirty.startswith("Dirty="):
        raise RuntimeError("Source fingerprint unavailable: " + bp.get_path_name())
    return {"dirty": dirty, "persistent_sha256": hashlib.sha256(properties.encode()).hexdigest()}


def run():
    report = {"status": "preflight", "weapons": {}, "source_before": {}, "saved": []}
    sources = {}
    paths = {}
    try:
        if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError("End PIE before changing project weapon defaults")
        observed = json.loads((WORK / "weapon-display-metadata.json").read_text(encoding="utf-8"))
        if set(observed.get("templates", {})) != set(SOURCES):
            raise RuntimeError("A complete original/project weapon metadata probe is required")
        for name in MISSING_ICONS:
            if observed["templates"][name]["source"].get("thumbnail") is not None:
                raise RuntimeError("Missing-icon assignment no longer matches the inspected source: " + name)
        project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
        targets = {}
        icons = {}
        for name, path in SOURCES.items():
            source = unreal.load_asset(path)
            target = unreal.load_asset("/Game/Items/Weapons/WI_" + name)
            if not isinstance(source, unreal.Blueprint) or not isinstance(target, unreal.Blueprint) or source == target:
                raise RuntimeError("Expected distinct source and project weapon Blueprints: " + name)
            sources[name] = source
            targets[name] = target
            paths[name] = project / "Content" / (path.removeprefix("/Game/") + ".uasset")
            report["source_before"][name] = {"metadata": snapshot(source), "fingerprint": fingerprint(source),
                                            "file_sha256": hashlib.sha256(paths[name].read_bytes()).hexdigest()}
            report["weapons"][name] = {"asset": target.get_path_name(), "before": snapshot(target)}
            if name in MISSING_ICONS:
                texture = unreal.load_asset(ICON_ROOT + MISSING_ICONS[name])
                if not isinstance(texture, unreal.Texture2D):
                    raise RuntimeError("Existing generic icon is not a valid Texture2D: " + name)
                icons[name] = texture
            elif not report["weapons"][name]["before"]["thumbnail"]:
                raise RuntimeError("Existing authored icon unexpectedly missing on " + name)
        for name, target in targets.items():
            cdo = unreal.get_default_object(target.generated_class())
            cdo.set_editor_property("display_name", unreal.Text(name))
            before_icon = report["weapons"][name]["before"]["thumbnail"]
            if name in icons and not before_icon:
                cdo.set_editor_property("thumbnail", icons[name])
                report["weapons"][name]["generic_icon_assigned"] = icons[name].get_path_name()
            else:
                report["weapons"][name]["existing_icon_preserved"] = before_icon
            unreal.BlueprintEditorLibrary.compile_blueprint(target)
            after = snapshot(target)
            report["weapons"][name]["after"] = after
            if after["display_name"] != name or after["derived_weapon_name"] != name:
                raise RuntimeError("Compiled project weapon did not retain canonical name: " + name)
            expected_icon = before_icon or icons[name].get_path_name()
            if after["thumbnail"] != expected_icon:
                raise RuntimeError("Compiled weapon did not retain its intended icon: " + name)
        report["source_after"] = {
            name: {"metadata": snapshot(source), "fingerprint": fingerprint(source),
                   "file_sha256": hashlib.sha256(paths[name].read_bytes()).hexdigest()}
            for name, source in sources.items()}
        if report["source_after"] != report["source_before"]:
            raise RuntimeError("Original weapon templates changed; project saves withheld")
        for target in targets.values():
            if not unreal.EditorAssetLibrary.save_loaded_asset(target, False):
                raise RuntimeError("Could not save project weapon: " + target.get_path_name())
            report["saved"].append(target.get_path_name())
        report["status"] = "saved; all five canonical names and icons validated; fresh PIE wheel review required"
    except Exception as exc:
        report["error"] = str(exc)
        report["status"] = "stopped; preserve editor state for review"
        raise
    finally:
        if sources:
            report["source_final"] = {
                name: {"metadata": snapshot(source), "fingerprint": fingerprint(source),
                       "file_sha256": hashlib.sha256(paths[name].read_bytes()).hexdigest()}
                for name, source in sources.items()}
            report["source_templates_unchanged"] = report["source_final"] == report["source_before"]
        report["package_manifest"] = sorted({p.split(".", 1)[0] for p in report["saved"]})
        OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("VELKORRAN_PROJECT_WEAPON_DISPLAY_SETUP " + str(OUT))
        if report.get("source_templates_unchanged") is False:
            raise RuntimeError("Original weapon template preservation failed; inspect the report")


run()
