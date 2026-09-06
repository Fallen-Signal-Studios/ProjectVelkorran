"""Read-only verification of copied controller menu classes and Blueprint graph references."""
import json
import re
import hashlib
import os
from pathlib import Path
import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
output_root = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT", str(project / "Saved/Validation/WorkPCSetup")))
output_root.mkdir(parents=True, exist_ok=True)
manifests = sorted((project / "Saved/Validation/EditorUIRemap").glob("*.json"))
if not manifests:
    raise RuntimeError("No UI remapping report exists.")
manifest_path = manifests[-1]
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
pairs = [(manifest["source_controller"], manifest["project_controller"])] + [
    (row["source"], row["project"]) for row in manifest["copied"]]


def class_path(package):
    return package + "." + package.rsplit("/", 1)[1] + "_C"


def value(obj):
    return obj.get_path_name() if obj else None


supplemental_errors = []
supplemental = None
supplemental_path = output_root / "project-failure-menu-footer-setup.json"
if supplemental_path.is_file():
    try:
        supplemental = json.loads(supplemental_path.read_text(encoding="utf-8"))
        expected_pair = {"source": "/NarrativePro/Pro/Core/UI/Menus/Fail/W_NarrativeMenu_Failed",
                         "project": "/Game/UI/Narrative/Menus/Fail/W_NarrativeMenu_Failed"}
        if (supplemental.get("schema_version") != 1 or not supplemental.get("success")
                or supplemental.get("additional_ui_pairs") != [expected_pair]
                or supplemental.get("source") != expected_pair["source"]
                or supplemental.get("target") != expected_pair["project"]
                or supplemental.get("controller") != manifest["project_controller"]
                or not supplemental.get("source_files_unchanged")
                or not supplemental.get("source_memory_unchanged")
                or not supplemental.get("compiled_inheritance_matches")
                or supplemental.get("unrelated_widget_slot_graph_objects_changed") != []):
            raise RuntimeError("Supplemental failure-menu report lacks the exact successful preservation contract")
        compile_rows = re.findall(r"^(/Game/[^\n]+): errors=(\d+) warnings=(\d+)$", supplemental.get("compilation", ""), re.MULTILINE)
        expected_assets = {p + "." + p.rsplit("/", 1)[1] for p in (expected_pair["project"], manifest["project_controller"])}
        if ({r[0] for r in compile_rows} != expected_assets or any(int(r[1]) for r in compile_rows)
                or set(supplemental.get("saved", [])) != expected_assets):
            raise RuntimeError("Supplemental failure-menu/menu-controller zero-error compilation or save evidence is incomplete")
        plugin = list((project / "Plugins").glob("*/NarrativePro.uplugin"))
        if len(plugin) != 1:
            raise RuntimeError("Cannot verify supplemental original source hashes")
        source_hashes = supplemental.get("source_hashes_after", {})
        required_sources = {expected_pair["source"], manifest["source_controller"],
                            "/NarrativePro/Pro/Core/UI/Menus/WBP_NarrativeMenu"}
        if set(source_hashes) != required_sources or source_hashes != supplemental.get("source_hashes_before"):
            raise RuntimeError("Supplemental source hash inventory is incomplete")
        for package, expected_hash in source_hashes.items():
            path = plugin[0].parent / "Content" / (package.removeprefix("/NarrativePro/") + ".uasset")
            if hashlib.sha256(path.read_bytes()).hexdigest() != expected_hash:
                raise RuntimeError("Supplemental original source file has changed: " + package)
        extra = (expected_pair["source"], expected_pair["project"])
        if any(a == extra[0] and b != extra[1] for a, b in pairs):
            raise RuntimeError("Supplemental failure-menu pair conflicts with the original UI remap")
        if extra not in pairs:
            pairs.append(extra)
    except Exception as exc:
        supplemental_errors.append("Supplemental failure-menu authoring report: " + str(exc))


class_map = {class_path(source): class_path(destination) for source, destination in pairs}
report = {"authoring_report": str(manifest_path), "authoring_success": manifest.get("success", False),
          "supplemental_authoring_report": str(supplemental_path) if supplemental_path.is_file() else None,
          "supplemental_pair_admitted": len(pairs) > len(manifest["copied"]) + 1,
          "controller_menu_defaults": [], "blueprints": [], "residual_references": [], "errors": supplemental_errors}
old_pc = unreal.get_default_object(unreal.load_class(None, class_path(manifest["source_controller"])))
new_pc = unreal.get_default_object(unreal.load_class(None, class_path(manifest["project_controller"])))
for name in ("WeaponWheelClass", "PlayerInfoMenuClass", "PauseMenuClass", "CinematicMenuClass", "LootMenuClass",
             "QuickUseMenuClass", "DialogueMenuClass", "LoadingMenuClass", "DeathMenuClass", "WaitMenuClass", "FastTravelMenuClass"):
    try:
        original = value(old_pc.get_editor_property(name))
        current = value(new_pc.get_editor_property(name))
        expected = class_map.get(original, original)
        report["controller_menu_defaults"].append({"property": name, "source": original, "project": current, "expected": expected, "matches": current == expected})
        if current != expected:
            report["errors"].append("Controller default mismatch: " + name)
    except Exception as exc:
        report["errors"].append(name + ": " + str(exc))

def exported_object_path(text):
    if not text or text == "None":
        return None
    # Unreal export paths are Class'/Package.Asset'; retain only the object path.
    return str(text).split("'", 2)[1] if "'" in str(text) else str(text)


def public_parent_class(asset):
    data = unreal.AssetRegistryHelpers.create_asset_data(asset)
    parent = unreal.AssetRegistryHelpers.get_tag_value(data, "ParentClass")
    if parent is None:
        raise RuntimeError("The live Blueprint asset data has no ParentClass tag")
    return exported_object_path(parent)


def read_export(path):
    raw = path.read_bytes()
    return raw.decode("utf-16") if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else raw.decode("utf-8-sig")


export_directory = output_root / "controller-ui-readonly-exports"
export_directory.mkdir(exist_ok=True)
report["graph_inspection_method"] = "Unreal ObjectExporterT3D exports of live authored objects; no asset save or compile"
for source, destination in pairs:
    original = unreal.load_asset(source)
    copied = unreal.load_asset(destination)
    original_snapshot_before = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(original)
    row = {"asset": destination, "casts": [], "graphs": 0, "nodes": 0}
    report["blueprints"].append(row)
    try:
        original_parent = public_parent_class(original)
        copied_parent = public_parent_class(copied)
        expected_parent = "/Script/ProjectVelkorran.SovPlayerController" if destination == manifest["project_controller"] else class_map.get(original_parent, original_parent)
        row.update(parent=copied_parent, expected_parent=expected_parent)
        if copied_parent != expected_parent:
            report["errors"].append("Blueprint parent mismatch: " + destination)
        expected_class = unreal.load_class(None, expected_parent)
        copied_class = unreal.load_class(None, class_path(destination))
        row["compiled_inheritance_matches"] = bool(expected_class and unreal.MathLibrary.class_is_child_of(copied_class, expected_class))
        if not row["compiled_inheritance_matches"]:
            report["errors"].append("Compiled Blueprint inheritance mismatch: " + destination)
    except Exception as exc:
        report["errors"].append(destination + " parent: " + str(exc))
    # Graph export is independent of parent verification, so an unavailable API
    # cannot prevent every graph from being inspected.
    try:
        snapshot_before = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(copied)
        export_path = export_directory / (destination.strip("/").replace("/", "__") + ".t3d")
        task = unreal.AssetExportTask()
        task.set_editor_property("object", copied)
        task.set_editor_property("exporter", unreal.ObjectExporterT3D())
        task.set_editor_property("filename", str(export_path))
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("selected", False)
        task.set_editor_property("replace_identical", True)
        if not unreal.Exporter.run_asset_export_task(task):
            raise RuntimeError("Unreal object export failed: " + str(task.get_editor_property("errors")))
        exported = read_export(export_path)
        row["export_file"] = str(export_path)
        row["export_bytes"] = export_path.stat().st_size
        stack = []
        kinds = {}
        graph_paths = set()
        node_paths = set()
        for line_number, line in enumerate(exported.splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("Begin Object"):
                name_match = re.search(r'\bName="([^"]+)"', stripped)
                if not name_match:
                    raise RuntimeError("Unrecognized object declaration at export line " + str(line_number))
                stack.append(name_match.group(1))
                relative_path = ".".join(stack)
                kind_match = re.search(r'\bClass=(\S+)', stripped)
                if kind_match:
                    kinds[relative_path] = kind_match.group(1).strip('"')
                kind = kinds.get(relative_path, "")
                if "EdGraph" in kind and "Node" not in kind:
                    graph_paths.add(relative_path)
                if "K2Node" in kind or "EdGraphNode" in kind:
                    node_paths.add(relative_path)
                continue
            if stripped.startswith("End Object"):
                if stack:
                    stack.pop()
                continue
            relative_path = ".".join(stack)
            if stripped.startswith("TargetType="):
                row["casts"].append({"node": relative_path, "serialized_target": stripped, "line": line_number})
            if stripped.startswith(("TargetType=", "FunctionReference=", "VariableReference=")):
                for old_class in class_map:
                    if old_class in stripped:
                        report["residual_references"].append({"asset": destination, "node": relative_path,
                            "line": line_number, "kind": stripped.split("=", 1)[0], "reference": stripped})
        row["graphs"] = len(graph_paths)
        row["nodes"] = len(node_paths)
        row["persistent_properties_unchanged"] = snapshot_before == unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(copied)
        if not row["persistent_properties_unchanged"]:
            report["errors"].append("Read-only export altered persistent properties or dirty state: " + destination)
        if not exported.strip() or "Begin Object" not in exported:
            report["errors"].append("Object export was empty: " + destination)
    except Exception as exc:
        report["errors"].append(destination + " graph export: " + str(exc))
    row["source_persistent_properties_and_dirty_state_unchanged"] = (
        original_snapshot_before == unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(original))
    if not row["source_persistent_properties_and_dirty_state_unchanged"]:
        report["errors"].append("Read-only verification altered source properties or dirty state: " + source)
report["total_graphs"] = sum(row["graphs"] for row in report["blueprints"])
report["total_nodes"] = sum(row["nodes"] for row in report["blueprints"])
if report["total_graphs"] == 0 or report["total_nodes"] == 0:
    report["errors"].append("No authored graphs/nodes were observed in the live object exports")
report["static_checks_passed"] = report["authoring_success"] and not report["errors"] and not report["residual_references"]
out = output_root / "controller-ui-remap-verification.json"
out.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("VELKORRAN_UI_REMAP_VERIFICATION " + str(out))
