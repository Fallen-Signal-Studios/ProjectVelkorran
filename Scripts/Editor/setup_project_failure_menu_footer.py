"""Collapse only the known developer footer in a project-owned failure menu.

Run only after the root agent releases the content/cook freeze, with PIE stopped.
The latest successful controller UI remap report currently proves DeathMenuClass
still uses the stock failure menu. This creates that single additional project
copy if needed and changes only the copied controller's death-menu default.
"""
import hashlib
import os
import json
import re
from pathlib import Path
import unreal

WORK = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT", str(
    Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / "Saved/Validation/WorkPCSetup")))
WORK.mkdir(parents=True, exist_ok=True)
OUT = WORK / "project-failure-menu-footer-setup.json"
SOURCE = "/NarrativePro/Pro/Core/UI/Menus/Fail/W_NarrativeMenu_Failed"
TARGET = "/Game/UI/Narrative/Menus/Fail/W_NarrativeMenu_Failed"
SOURCE_PC = "/NarrativePro/Pro/Core/BP/Framework/BP_NarrativePlayerController"
PROJECT_PC = "/Game/Framework/BP_SovPlayerController"
SOURCE_MENU_PARENT = "/NarrativePro/Pro/Core/UI/Menus/WBP_NarrativeMenu"
FOOTER_TEXT = ("(This is the Default Narrative Failed Screen, used for both Death and Quest failure. "
               "To edit this screen, and this message, open W_NarrativeMenu_Failed in UMG.)")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(bp):
    raw = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp)
    dirty, separator, values = raw.partition("\n")
    if not separator or dirty not in ("Dirty=0", "Dirty=1"):
        raise RuntimeError("Native Blueprint fingerprint failed: " + bp.get_path_name())
    records = {}
    for line in values.splitlines():
        path, cls, digest = line.rsplit("|", 2)
        if path in records:
            raise RuntimeError("Duplicate object in native fingerprint")
        records[path] = {"class": cls, "digest": digest}
    return {"dirty": dirty, "digest": hashlib.sha256(values.encode()).hexdigest(), "objects": records}


def owned_by(obj, owner):
    for _ in range(40):
        if obj == owner:
            return True
        obj = obj.get_outer() if obj else None
        if obj is None:
            return False
    return False


def widgets(bp):
    return [w for w in unreal.ObjectIterator(unreal.Widget) if owned_by(w, bp)]


def footer(bp):
    found = [w for w in widgets(bp) if isinstance(w, unreal.TextBlock) and str(w.get_text()) == FOOTER_TEXT]
    if len(found) != 1:
        raise RuntimeError("Expected exactly one TextBlock containing the full developer footer: " + str(len(found)))
    return found[0]


def export_blueprint(bp, label):
    folder = WORK / "failure-menu-readonly-exports"
    folder.mkdir(exist_ok=True)
    output = folder / (label + ".t3d")
    before = snapshot(bp)
    task = unreal.AssetExportTask()
    for name, value in (("object", bp), ("exporter", unreal.ObjectExporterT3D()),
                        ("filename", str(output)), ("automated", True), ("prompt", False),
                        ("selected", False), ("replace_identical", True)):
        task.set_editor_property(name, value)
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError("Official Blueprint export failed: " + str(task.get_editor_property("errors")))
    if snapshot(bp) != before:
        raise RuntimeError("Read-only text export altered persistent properties or dirty state")
    raw = output.read_bytes()
    text = raw.decode("utf-16") if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else raw.decode("utf-8-sig")
    return text, {"file": str(output), "sha256": hashlib.sha256(raw).hexdigest()}


def bindings_and_footer_guards(bp, widget, text):
    depth = 0
    root_count = 0
    bindings = []
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if line.startswith("Begin Object"):
            if depth == 0:
                if ('Name="' + bp.get_name() + '"') not in line or "WidgetBlueprint" not in line:
                    raise RuntimeError("Unexpected exported failure-menu root")
                root_count += 1
            depth += 1
        elif line.startswith("End Object"):
            depth -= 1
        elif depth == 1 and re.match(r"Bindings(?:\(\d+\))?=", line):
            bindings.append(line)
            if line.split("=", 1)[1].strip() == "()":
                continue
            fields = {}
            for field in ("ObjectName", "PropertyName"):
                match = re.search(r'(?:^|[,\(])' + field + r'=(?:"([^"\\]*)"|([^,\)]+))', line)
                if not match:
                    raise RuntimeError("Unrecognized failure-menu binding at line " + str(number))
                fields[field] = match.group(1) if match.group(1) is not None else match.group(2)
            if fields["ObjectName"] == widget.get_name() and fields["PropertyName"].lower() == "visibility":
                raise RuntimeError("Developer footer has an authored visibility binding; leave it for review")
    if root_count != 1 or depth != 0 or FOOTER_TEXT not in text:
        raise RuntimeError("Incomplete export or unexpected developer footer serialization")
    # A graph reference could change the footer at runtime. Refuse that case;
    # this edit intentionally changes no graph or player-facing event.
    if re.search(r'MemberName="?' + re.escape(widget.get_name()) + r'(?:"|[,\)])', text):
        raise RuntimeError("Developer footer is referenced by a Blueprint graph; inspect before editing")
    return bindings


def authoring_objects(bp, state, exclude=()):
    prefix = bp.get_path_name() + ":"
    return {p: row for p, row in state["objects"].items() if p.startswith(prefix) and p not in exclude}


def changed_objects(before, after):
    return sorted(p for p in before.keys() | after.keys() if before.get(p) != after.get(p))


def class_path(package):
    return package + "." + package.rsplit("/", 1)[1] + "_C"


def public_parent(asset):
    data = unreal.AssetRegistryHelpers.create_asset_data(asset)
    raw = unreal.AssetRegistryHelpers.get_tag_value(data, "ParentClass")
    if raw is None:
        raise RuntimeError("Live Blueprint asset data did not provide ParentClass")
    return str(raw).split("'", 2)[1] if "'" in str(raw) else str(raw)


def exported_properties_by_object(text):
    stack = []
    records = {}
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("Begin Object"):
            match = re.search(r'ExportPath="[^\']*\'([^\']+)\'"', line)
            if not match:
                raise RuntimeError("Exported object has no complete object path")
            stack.append(match[1])
            records.setdefault(match[1], [])
        elif line.startswith("End Object"):
            if not stack:
                raise RuntimeError("Unbalanced exported object")
            stack.pop()
        elif stack:
            records[stack[-1]].append(line)
    if stack:
        raise RuntimeError("Incomplete exported object tree")
    return records


def animation_revision_only(bp, before, after, changed, before_text, after_text):
    # The native helper calls Modify() before compiling. UMovieSceneSignedObject
    # regenerates its Signature cache revision in Modify()/MarkAsChanged(), even
    # when no authored animation data changed. GUI12 exports proved these exact
    # four objects differed only in Signature. Never exempt an animation object
    # wholesale: compare every exported non-Signature property byte-for-byte.
    classes = {
        ":FadeIn": "/Script/UMG.WidgetAnimation",
        ":FadeIn.FadeIn": "/Script/MovieScene.MovieScene",
        ":FadeIn.FadeIn.MovieSceneFloatTrack_0": "/Script/MovieSceneTracks.MovieSceneFloatTrack",
        ":FadeIn.FadeIn.MovieSceneFloatTrack_0.MovieSceneFloatSection_0": "/Script/MovieSceneTracks.MovieSceneFloatSection",
    }
    allow = {bp.get_path_name() + suffix: cls for suffix, cls in classes.items()}
    old_objects = exported_properties_by_object(before_text)
    new_objects = exported_properties_by_object(after_text)
    verified = []
    for path in changed:
        if path not in allow:
            continue
        if before.get(path, {}).get("class") != allow[path] or after.get(path, {}).get("class") != allow[path]:
            continue
        old = old_objects.get(path, [])
        new = new_objects.get(path, [])
        old_signature = [p for p in old if p.startswith("Signature=")]
        new_signature = [p for p in new if p.startswith("Signature=")]
        if (len(old_signature) != 1 or len(new_signature) != 1
                or not re.fullmatch(r"Signature=[0-9A-Fa-f]{32}", old_signature[0])
                or not re.fullmatch(r"Signature=[0-9A-Fa-f]{32}", new_signature[0])):
            continue
        if [p for p in old if not p.startswith("Signature=")] != [p for p in new if not p.startswith("Signature=")]:
            continue
        verified.append({"object": path, "class": allow[path], "only_changed_property": "Signature",
                         "before": old_signature[0], "after": new_signature[0],
                         "all_other_exported_properties_identical": True})
    return verified


def run():
    report = {"schema_version": 1, "success": False, "status": "preflight", "source": SOURCE, "target": TARGET, "controller": PROJECT_PC,
              "additional_ui_pairs": [{"source": SOURCE, "project": TARGET}],
              "saved": [], "source_preservation": {}, "package_additions": [TARGET]}
    sources = {}
    source_files = {}
    try:
        if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError("End PIE before editing the failure-menu project copy")
        project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
        reports = sorted((project_dir / "Saved/Validation/EditorUIRemap").glob("*.json"), reverse=True)
        remap_path = next((p for p in reports if json.loads(p.read_text(encoding="utf-8")).get("success")), None)
        if remap_path is None:
            raise RuntimeError("A successful authored controller UI remap report is required")
        remap = json.loads(remap_path.read_text(encoding="utf-8"))
        if remap.get("project_controller") != PROJECT_PC or not remap.get("source_packages_unchanged"):
            raise RuntimeError("Latest successful UI remap does not match the preserved project controller")
        matches = [r for r in remap["copied"] if r["source"] == SOURCE]
        if matches and matches != [{"source": SOURCE, "project": TARGET}]:
            raise RuntimeError("Failure menu was copied to an unexpected destination")
        report["verified_remap_report"] = {"file": str(remap_path), "sha256": sha(remap_path),
                                          "failure_menu_was_in_original_remap": bool(matches)}
        plugin = list((project_dir / "Plugins").glob("*/NarrativePro.uplugin"))
        if len(plugin) != 1:
            raise RuntimeError("Expected one NarrativePro source content mount")
        source_files = {p: plugin[0].parent / "Content" / (p.removeprefix("/NarrativePro/") + ".uasset")
                        for p in (SOURCE, SOURCE_PC, SOURCE_MENU_PARENT)}
        report["source_hashes_before"] = {p: sha(f) for p, f in source_files.items()}
        sources = {p: unreal.load_asset(p) for p in source_files}
        pc = unreal.load_asset(PROJECT_PC)
        if any(not isinstance(bp, unreal.Blueprint) for bp in [*sources.values(), pc]):
            raise RuntimeError("Source failure menu, source controller and project controller must be Blueprints")
        report["source_memory_before"] = {p: snapshot(bp) for p, bp in sources.items()}
        source_footer = footer(sources[SOURCE])
        report["source_footer"] = {"name": source_footer.get_name(), "text": str(source_footer.get_text()),
                                   "visibility": str(source_footer.get_visibility())}
        source_text, report["source_export"] = export_blueprint(sources[SOURCE], "source-before")
        prior_pairs = [(SOURCE_PC, PROJECT_PC)] + [(r["source"], r["project"]) for r in remap["copied"]]
        prior_map = {class_path(a): class_path(b) for a, b in prior_pairs}
        report["original_ui_class_map_checked"] = prior_map
        # The verified failure menu uses WBP_NarrativeMenu (not in the controller
        # remap) and native APIs; its source binary lists no remapped controller or
        # widget classes. Recheck the live export instead of assuming that stays true.
        required = [old for old in prior_map if old in source_text]
        report["required_additional_ui_class_mappings"] = required
        if required:
            raise RuntimeError("Failure-menu dependencies now require additional remapping beyond this footer-only edit: " + repr(required))
        source_parent = public_parent(sources[SOURCE])
        if source_parent != class_path(SOURCE_MENU_PARENT):
            raise RuntimeError("Failure menu no longer has its verified original menu parent")
        report["source_parent"] = source_parent
        pc_cdo = unreal.get_default_object(pc.generated_class())
        original_death_class = pc_cdo.get_editor_property("DeathMenuClass")
        report["death_menu_before"] = original_death_class.get_path_name() if original_death_class else None
        expected_classes = {SOURCE + ".W_NarrativeMenu_Failed_C", TARGET + ".W_NarrativeMenu_Failed_C"}
        if report["death_menu_before"] not in expected_classes:
            raise RuntimeError("Project controller has a different authored death menu; refusing to replace it")
        exists = unreal.EditorAssetLibrary.does_asset_exist(TARGET)
        copied = unreal.load_asset(TARGET) if exists else unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
        if not isinstance(copied, unreal.Blueprint) or copied == sources[SOURCE]:
            raise RuntimeError("Project failure-menu copy failed")
        report["created_project_copy"] = not exists
        target_footer = footer(copied)
        if target_footer.get_name() != source_footer.get_name() or not target_footer.get_parent():
            raise RuntimeError("Project copy developer footer differs from the verified source template")
        if public_parent(copied) != prior_map.get(source_parent, source_parent):
            raise RuntimeError("Copied failure menu has an unexpected parent requiring review")
        text, report["project_export_before"] = export_blueprint(copied, "project-before")
        bindings = bindings_and_footer_guards(copied, target_footer, text)
        report["bindings_before"] = bindings
        report["widget_inventory_before"] = [{"name": w.get_name(), "class": w.get_class().get_path_name(),
            "parent": w.get_parent().get_name() if w.get_parent() else None,
            "text": str(w.get_text()) if isinstance(w, unreal.TextBlock) else None} for w in widgets(copied)]
        report["footer_path"] = target_footer.get_path_name()
        report["visibility_before"] = str(target_footer.get_visibility())
        before = snapshot(copied)
        pc_before = snapshot(pc)
        copied.modify()
        target_footer.modify()
        target_footer.set_visibility(unreal.SlateVisibility.COLLAPSED)
        after_visibility = snapshot(copied)
        changes = changed_objects(before["objects"], after_visibility["objects"])
        if set(changes) - {target_footer.get_path_name()}:
            raise RuntimeError("Footer collapse changed unrelated persistent objects: " + repr(changes))
        report["objects_changed_by_footer_edit"] = changes
        pc.modify()
        pc_cdo.modify()
        pc_cdo.set_editor_property("DeathMenuClass", copied.generated_class())
        pc_after = snapshot(pc)
        pc_changes = changed_objects(pc_before["objects"], pc_after["objects"])
        if set(pc_changes) - {pc_cdo.get_path_name()}:
            raise RuntimeError("DeathMenuClass assignment changed unrelated controller objects: " + repr(pc_changes))
        report["objects_changed_by_controller_assignment"] = pc_changes
        result = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references([copied, pc], [sources[SOURCE]], [copied])
        report["compilation"] = str(result.get_editor_property("report"))
        if not result.get_editor_property("succeeded"):
            raise RuntimeError("Failure menu/controller compilation failed; no project saves performed")
        compiled = snapshot(copied)
        final_text, report["project_export_after"] = export_blueprint(copied, "project-after")
        protected_before = authoring_objects(copied, before, (target_footer.get_path_name(),))
        protected_after = authoring_objects(copied, compiled, (target_footer.get_path_name(),))
        protected_changes = changed_objects(protected_before, protected_after)
        report["raw_protected_object_fingerprint_changes"] = protected_changes
        report["verified_animation_revision_only_changes"] = animation_revision_only(
            copied, protected_before, protected_after, protected_changes, text, final_text)
        revision_only = {r["object"] for r in report["verified_animation_revision_only_changes"]}
        protected_changes = [p for p in protected_changes if p not in revision_only]
        report["unrelated_widget_slot_graph_objects_changed"] = protected_changes
        if protected_changes:
            raise RuntimeError("Compilation altered protected widgets, layouts or graphs; saves withheld")
        target_footer = footer(copied)
        if target_footer.get_visibility() != unreal.SlateVisibility.COLLAPSED:
            raise RuntimeError("Compiled developer footer did not retain Collapsed visibility")
        final_bindings = bindings_and_footer_guards(copied, target_footer, final_text)
        if final_bindings != bindings:
            raise RuntimeError("Failure-menu property bindings changed; saves withheld")
        report["bindings_after"] = final_bindings
        report["visibility_after"] = str(target_footer.get_visibility())
        death_class = unreal.get_default_object(pc.generated_class()).get_editor_property("DeathMenuClass")
        if death_class != copied.generated_class():
            raise RuntimeError("Compiled project controller lost its project failure-menu class")
        report["death_menu_after"] = death_class.get_path_name()
        expected_parent = prior_map.get(source_parent, source_parent)
        report["project_parent"] = public_parent(copied)
        report["compiled_inheritance_matches"] = bool(unreal.MathLibrary.class_is_child_of(
            copied.generated_class(), unreal.load_class(None, expected_parent)))
        if report["project_parent"] != expected_parent or not report["compiled_inheritance_matches"]:
            raise RuntimeError("Compiled copied failure-menu parent does not match the existing UI class mapping")
        if {p: snapshot(bp) for p, bp in sources.items()} != report["source_memory_before"]:
            raise RuntimeError("Source properties or dirty flags changed; project saves withheld")
        if {p: sha(f) for p, f in source_files.items()} != report["source_hashes_before"]:
            raise RuntimeError("Source asset files changed; project saves withheld")
        for bp in (copied, pc):
            if not unreal.EditorAssetLibrary.save_loaded_asset(bp, False):
                raise RuntimeError("Could not save project asset " + bp.get_path_name())
            report["saved"].append(bp.get_path_name())
        report["status"] = "saved; only developer footer collapsed, project controller linked, zero compile errors"
        report["success"] = True
    except Exception as exc:
        report["error"] = str(exc)
        report["status"] = "stopped; preserve editor state for review"
        raise
    finally:
        if sources and "source_memory_before" in report:
            report["source_memory_after"] = {p: snapshot(bp) for p, bp in sources.items()}
            report["source_memory_unchanged"] = report["source_memory_after"] == report["source_memory_before"]
        if source_files and "source_hashes_before" in report:
            report["source_hashes_after"] = {p: sha(f) for p, f in source_files.items()}
            report["source_files_unchanged"] = report["source_hashes_after"] == report["source_hashes_before"]
        OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("VELKORRAN_FAILURE_MENU_FOOTER_SETUP " + str(OUT))


run()
