"""Duplicate/remap Narrative UI hard casts for the native Sovereign controller.

Run in the editor with PIE stopped. Requires ProjectVelkorranEditor. Only /Game
copies are modified/saved; source package hashes are checked before and after.
"""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import unreal

SOURCE_PC = "/NarrativePro/Pro/Core/BP/Framework/BP_NarrativePlayerController"
PROJECT_PC = "/Game/Framework/BP_SovPlayerController"
SOURCE_UI = "/NarrativePro/Pro/Core/UI/"
PROJECT_UI = "/Game/UI/Narrative/"


def package(path):
    return str(path).split(".", 1)[0]


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def memory_snapshot(asset):
    snapshot = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(asset)
    dirty, separator, properties = snapshot.partition("\n")
    if not separator or not dirty.startswith("Dirty="):
        raise RuntimeError("Native source property snapshot failed: " + asset.get_path_name())
    return {"package_dirty": dirty == "Dirty=1", "persistent_object_properties": properties,
            "digest": hashlib.sha256(properties.encode("utf-8")).hexdigest()}


def run():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("End Play In Editor before authoring UI copies.")
    if not hasattr(unreal, "SovBlueprintAuthoringLibrary"):
        raise RuntimeError("Build/restart the ProjectVelkorranEditor module before authoring UI copies.")
    project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    plugin_files = list((project_dir / "Plugins").glob("*/NarrativePro.uplugin"))
    if len(plugin_files) != 1:
        raise RuntimeError("Expected one project-installed NarrativePro plugin for source preservation checks.")
    source_content = plugin_files[0].parent / "Content"
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True, include_hard_package_references=True,
        include_searchable_names=False, include_soft_management_references=False,
        include_hard_management_references=False)
    all_ui_packages = sorted({package(p) for p in unreal.EditorAssetLibrary.list_assets(SOURCE_UI, True, False)})
    ui_packages = [p for p in all_ui_packages
                   if str(unreal.EditorAssetLibrary.find_asset_data(p).asset_class_path.asset_name)
                   in ("Blueprint", "WidgetBlueprint")]
    dependencies = {p: {str(dep) for dep in registry.get_dependencies(unreal.Name(p), options)} for p in ui_packages}
    # Imported theme examples and old redirectors can refer back to the controller
    # without being used by it. Restrict the repair to its actual UI dependencies.
    pc_dependencies = {str(dep) for dep in registry.get_dependencies(unreal.Name(SOURCE_PC), options)}
    reachable = pc_dependencies & set(ui_packages)
    while True:
        expanded = reachable | {dep for p in reachable for dep in dependencies[p] if dep in dependencies}
        if expanded == reachable:
            break
        reachable = expanded
    selected = {p for p, deps in dependencies.items() if p in reachable and SOURCE_PC in deps}
    direct = sorted(selected)
    if not selected:
        raise RuntimeError("Asset registry found no UI references to the source controller; refusing an empty repair.")
    # Every UI asset referring to a remapped class also needs a project copy.
    # This includes widget parents/children and the top-level menus held by the PC.
    while True:
        expanded = selected | {p for p, deps in dependencies.items() if p in reachable and deps & selected}
        if expanded == selected:
            break
        selected = expanded
    source_packages = [SOURCE_PC] + sorted(selected)
    source_paths = {p: source_content / (p.removeprefix("/NarrativePro/") + ".uasset") for p in source_packages}
    missing = [str(path) for path in source_paths.values() if not path.is_file()]
    if missing:
        raise RuntimeError("Source preservation inventory has missing files: " + repr(missing))
    original_hashes = {p: sha256(path) for p, path in source_paths.items()}
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    report_path = project_dir / "Saved" / "Validation" / "EditorUIRemap" / (stamp + ".json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = {"source_controller": SOURCE_PC, "project_controller": PROJECT_PC,
              "direct_ui_references": direct, "ui_dependency_closure": sorted(selected),
              "reachable_ui_packages": sorted(reachable),
              "copied": [], "source_hashes_before": original_hashes, "saved": [], "success": False}
    source_assets = []
    try:
        original_pc = unreal.load_asset(SOURCE_PC)
        project_pc = unreal.load_asset(PROJECT_PC)
        if not original_pc or not isinstance(project_pc, unreal.Blueprint):
            raise RuntimeError("Both original and project controller Blueprints must exist.")
        # Materialize controller-data/UI prerequisites before duplication triggers
        # Blueprint compilation and widget construction that could load them again.
        loaded_prerequisites = [unreal.load_asset(p) for p in sorted(reachable)]
        for p in unreal.EditorAssetLibrary.list_assets(SOURCE_UI + "ControllerData", True, False):
            loaded_prerequisites.append(unreal.load_asset(p))
        source_assets = [original_pc] + [unreal.load_asset(source) for source in sorted(selected)]
        if any(not isinstance(source, unreal.Blueprint) for source in source_assets):
            raise RuntimeError("UI closure unexpectedly contains a non-Blueprint.")
        report["source_memory_before"] = {source.get_path_name(): memory_snapshot(source) for source in source_assets}
        replacements = [project_pc]
        for source, original in zip(sorted(selected), source_assets[1:]):
            destination = PROJECT_UI + source.removeprefix(SOURCE_UI)
            copied = unreal.load_asset(destination) if unreal.EditorAssetLibrary.does_asset_exist(destination) else unreal.EditorAssetLibrary.duplicate_asset(source, destination)
            if not isinstance(copied, unreal.Blueprint):
                raise RuntimeError("Could not duplicate UI Blueprint: " + source)
            replacements.append(copied)
            report["copied"].append({"source": source, "project": destination})
        result = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references(replacements, source_assets, replacements)
        if not isinstance(result, unreal.SovBlueprintAuthoringResult):
            raise RuntimeError("Unexpected editor helper result; project saves withheld: " + repr(result))
        succeeded = result.get_editor_property("succeeded")
        compilation = result.get_editor_property("report")
        report["compilation"] = compilation
        if not succeeded:
            raise RuntimeError("Project UI remapping/compilation failed; no remapped assets were saved. See report.")
        report["source_memory_after_remap"] = {source.get_path_name(): memory_snapshot(source) for source in source_assets}
        changed_memory = [path for path, before in report["source_memory_before"].items()
                          if before["digest"] != report["source_memory_after_remap"][path]["digest"]]
        if changed_memory:
            raise RuntimeError("Source plugin persistent properties changed in memory; project saves withheld: " + repr(changed_memory))
        for asset in replacements:
            if not asset.get_path_name().startswith("/Game/"):
                raise RuntimeError("Unexpected save outside /Game: " + asset.get_path_name())
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
                raise RuntimeError("Could not save project UI copy: " + asset.get_path_name())
            report["saved"].append(asset.get_path_name())
        report["success"] = True
    except Exception as exc:
        report["error"] = str(exc)
        raise
    finally:
        if source_assets and "source_memory_before" in report:
            report["source_memory_after"] = {source.get_path_name(): memory_snapshot(source) for source in source_assets}
            report["source_memory_unchanged"] = all(before["digest"] == report["source_memory_after"][path]["digest"]
                                                    for path, before in report["source_memory_before"].items())
            report["source_dirty_flags_changed"] = [path for path, before in report["source_memory_before"].items()
                                                     if before["package_dirty"] != report["source_memory_after"][path]["package_dirty"]]
        report["source_hashes_after"] = {p: sha256(path) for p, path in source_paths.items()}
        report["source_packages_unchanged"] = report["source_hashes_after"] == original_hashes
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("VELKORRAN_UI_REMAP_REPORT " + str(report_path))
        if not report["source_packages_unchanged"]:
            raise RuntimeError("Source plugin files changed during authoring; inspect preservation report.")


run()
