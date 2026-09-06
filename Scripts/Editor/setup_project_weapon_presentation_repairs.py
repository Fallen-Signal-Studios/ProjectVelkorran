"""Repair only proven zero holster scales and one unbound Verity animation tag row.

Runs outside PIE. Originals remain byte-identical and retain their reflected state.
The Verity animation Blueprint and weapon visual are copied before remapping.
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
OUT = WORK / "weapon-presentation-repairs.json"
ABP_SOURCE = "/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Overlays/Weapons/ABP_Biped_Overlay_Verity"
ABP_TARGET = "/Game/Characters/Animation/ABP_SovVerityOverlay"
VIS_SOURCE = "/NarrativePro/Pro/Core/BP/WeaponVisuals/Blueprints/BP_VerityWeaponVisual"
VIS_TARGET = "/Game/Weapons/Visuals/BP_SovVerityWeaponVisual"
STACCATO_SOURCE = "/Game/WeaponMeshes/Weapon_Staccato"
VERITY_SOURCE = "/Game/WeaponMeshes/NWI_Verity"
STACCATO_TARGET = "/Game/Items/Weapons/WI_Staccato"
VERITY_TARGET = "/Game/Items/Weapons/WI_Verity"
INVALID_ROW = ',(TagToMap=(TagName="Sov.State.Guard.Broken"))'
ZERO = "Scale3D=(X=0.000000,Y=0.000000,Z=0.000000)"
UNIT = "Scale3D=(X=1.000000,Y=1.000000,Z=1.000000)"


def defaults(bp):
    return unreal.get_default_object(bp.generated_class())


def fingerprint(bp):
    raw = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp)
    dirty, sep, properties = raw.partition("\n")
    if not sep or not dirty.startswith("Dirty="):
        raise RuntimeError("Native source fingerprint unavailable")
    return {"dirty": dirty, "properties_sha256": hashlib.sha256(properties.encode()).hexdigest()}


def configs(item, field):
    return {str(unreal.GameplayTagLibrary.get_tag_name(k)): v.export_text()
            for k, v in item.get_editor_property(field).items()}


def duplicate(source, target):
    bp = unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else unreal.EditorAssetLibrary.duplicate_asset(source, target)
    if not isinstance(bp, unreal.Blueprint):
        raise RuntimeError("Expected a project Blueprint copy: " + target)
    return bp


def run():
    report = {"status": "preflight", "saved": [], "sources_before": {}, "sources_after": {}}
    sources = {}
    files = {}
    try:
        if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError("End PIE before editing weapon presentation templates")
        observed = json.loads((WORK / "weapon-presentation-health.json").read_text(encoding="utf-8"))
        expected_mapping = observed["animation"]["gameplay_tag_property_map"]
        if expected_mapping.count(INVALID_ROW) != 1:
            raise RuntimeError("Expected exactly the observed property-less Guard.Broken mapping")
        expected_fixed_mapping = expected_mapping.replace(INVALID_ROW, "", 1)
        project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
        plugins = list((project / "Plugins").glob("*/NarrativePro.uplugin"))
        if len(plugins) != 1:
            raise RuntimeError("Expected one NarrativePro plugin mount")
        for path in (ABP_SOURCE, VIS_SOURCE, STACCATO_SOURCE, VERITY_SOURCE):
            bp = unreal.load_asset(path)
            if not isinstance(bp, unreal.Blueprint):
                raise RuntimeError("Required source Blueprint missing: " + path)
            defaults(bp)
            sources[path] = bp
            files[path] = (project / "Content" / (path.removeprefix("/Game/") + ".uasset") if path.startswith("/Game/")
                           else plugins[0].parent / "Content" / (path.removeprefix("/NarrativePro/") + ".uasset"))
        for path, bp in sources.items():
            report["sources_before"][path] = {"fingerprint": fingerprint(bp), "file_sha256": hashlib.sha256(files[path].read_bytes()).hexdigest()}
        source_map = defaults(sources[ABP_SOURCE]).get_editor_property("gameplay_tag_property_map").export_text()
        if source_map != expected_mapping:
            raise RuntimeError("Verity source mapping changed since the read-only probe")
        staccato = unreal.load_asset(STACCATO_TARGET)
        verity = unreal.load_asset(VERITY_TARGET)
        if not isinstance(staccato, unreal.Blueprint) or not isinstance(verity, unreal.Blueprint):
            raise RuntimeError("Project kit weapon copies are required")
        src_holsters = configs(defaults(sources[STACCATO_SOURCE]), "holster_attachment_configs")
        expected_slots = {"Narrative.Equipment.Slot.Weapon.BackA", "Narrative.Equipment.Slot.Weapon.BackB"}
        if set(src_holsters) != expected_slots or any(text.count(ZERO) != 1 for text in src_holsters.values()):
            raise RuntimeError("Original Staccato no longer has exactly the two observed zero-scale holsters")
        desired_holsters = {slot: text.replace(ZERO, UNIT, 1) for slot, text in src_holsters.items()}
        current_holsters = configs(defaults(staccato), "holster_attachment_configs")
        if current_holsters not in (src_holsters, desired_holsters):
            raise RuntimeError("Project Staccato has additional attachment edits; do not overwrite")
        wield_before = configs(defaults(staccato), "wield_attachment_configs")
        report["staccato"] = {"before": current_holsters, "source": src_holsters, "expected_after": desired_holsters,
                               "clip_size_preserved": defaults(staccato).get_editor_property("clip_size")}
        source_visual = sources[VIS_SOURCE].generated_class()
        copied_visual_path = VIS_TARGET + "." + VIS_TARGET.rsplit("/", 1)[1] + "_C"
        current_visual = defaults(verity).get_editor_property("weapon_visual_class")
        current_visual_path = current_visual.get_path_name() if isinstance(current_visual, unreal.Object) else str(current_visual)
        if current_visual_path not in (source_visual.get_path_name(), copied_visual_path):
            raise RuntimeError("Verity visual is neither observed source nor this repaired copy: " + current_visual_path)

        # Clone each complete struct; never edit a source/container live view.
        updated = {}
        for key, config in defaults(staccato).get_editor_property("holster_attachment_configs").items():
            slot = str(unreal.GameplayTagLibrary.get_tag_name(key))
            copy = type(config)()
            if not copy.import_text(desired_holsters[slot]) or copy.export_text() != desired_holsters[slot]:
                raise RuntimeError("Could not preserve complete Staccato attachment struct: " + slot)
            updated[key] = copy
        abp = duplicate(ABP_SOURCE, ABP_TARGET)
        visual = duplicate(VIS_SOURCE, VIS_TARGET)
        mapping = defaults(abp).get_editor_property("gameplay_tag_property_map")
        if mapping.export_text() not in (expected_mapping, expected_fixed_mapping):
            raise RuntimeError("Copied Verity map has unrelated edits")
        repaired = type(mapping)()
        if not repaired.import_text(expected_fixed_mapping) or repaired.export_text() != expected_fixed_mapping:
            raise RuntimeError("Could not preserve the five remaining Verity tag bindings exactly")
        # Only the known copied data is modified after all preservation guards.
        for bp in (abp, visual, staccato, verity):
            bp.modify()
            defaults(bp).modify()
        defaults(abp).set_editor_property("gameplay_tag_property_map", repaired)
        defaults(staccato).set_editor_property("holster_attachment_configs", updated)
        # A soft visual class is explicit; native remapping handles overlay hard references, including form maps.
        defaults(verity).set_editor_property("weapon_visual_class", visual.generated_class())
        result = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references(
            [abp, visual, staccato, verity], [sources[ABP_SOURCE]], [abp])
        report["compilation"] = str(result.get_editor_property("report"))
        if not result.get_editor_property("succeeded"):
            raise RuntimeError("Repaired Blueprint compilation failed; saves withheld")
        if defaults(abp).get_editor_property("gameplay_tag_property_map").export_text() != expected_fixed_mapping:
            raise RuntimeError("Compiled Verity overlay did not retain exactly the five valid mappings")
        report["verity"] = {"before": expected_mapping, "after": expected_fixed_mapping, "overlay": ABP_TARGET, "visual": VIS_TARGET, "layer_fields": {}}
        matches = 0
        for field in ("default_weapon_anim_layer", "weapon1p_anim_layer", "dual_wield_weapon_anim_layer", "dual_wield_weapon1p_anim_layer"):
            layer = defaults(visual).get_editor_property(field)
            report["verity"]["layer_fields"][field] = layer.get_path_name() if layer else None
            if layer == sources[ABP_SOURCE].generated_class():
                raise RuntimeError("Old invalid overlay remains in copied visual field " + field)
            matches += int(layer == abp.generated_class())
        if matches == 0:
            raise RuntimeError("No direct copied visual overlay field points to repaired Verity animation")
        if defaults(verity).get_editor_property("weapon_visual_class") != visual.generated_class():
            raise RuntimeError("Compiled Verity item did not retain copied visual class")
        if configs(defaults(staccato), "holster_attachment_configs") != desired_holsters or configs(defaults(staccato), "wield_attachment_configs") != wield_before:
            raise RuntimeError("Staccato attachment verification failed")
        for path, bp in sources.items():
            report["sources_after"][path] = {"fingerprint": fingerprint(bp), "file_sha256": hashlib.sha256(files[path].read_bytes()).hexdigest()}
        if report["sources_before"] != report["sources_after"]:
            raise RuntimeError("Original weapon/animation template changed; saves withheld")
        for bp in (abp, visual, staccato, verity):
            if not unreal.EditorAssetLibrary.save_loaded_asset(bp, False):
                raise RuntimeError("Project repair save failed: " + bp.get_path_name())
            report["saved"].append(bp.get_path_name())
        report["status"] = "saved; repeat Staccato hand/holster cycle and Verity wield in fresh PIE"
        report["source_templates_unchanged"] = True
        report["package_manifest"] = sorted(path.split(".", 1)[0] for path in report["saved"])
    except Exception as exc:
        report["error"] = str(exc)
        unreal.log_error("VELKORRAN_WEAPON_PRESENTATION_REPAIR_FAILED " + str(exc))
    finally:
        report["sources_final"] = {path: {"fingerprint": fingerprint(bp), "file_sha256": hashlib.sha256(files[path].read_bytes()).hexdigest()} for path, bp in sources.items()}
        if report["sources_before"] and report["sources_final"] != report["sources_before"]:
            report["error"] = report.get("error", "") + " Original template changed; preserve state for review."
        OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("VELKORRAN_WEAPON_PRESENTATION_REPAIR_REPORT " + str(OUT))


run()
