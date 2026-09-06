"""Project-copy HUD layout adjustment; run only after the live geometry probe.

End PIE first. Supports the observed two-child resource/weapon VerticalBox branch
and independently positioned weapon widgets. Other shared branches are rejected.
No stock HUD assets are modified or saved.
"""
import hashlib
import json
import os
import re
from pathlib import Path
import unreal

SCRIPT_DIR = Path(__file__).parent
OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
WORK = OUTPUT_DIR
PROBE = WORK / "combat-hud-layout.json"
OUT = WORK / "combat-hud-layout-setup.json"
SOURCE = "/NarrativePro/Pro/Core/UI/Menus/GameHUD/WBP_DefaultGameplayHUD"
TARGET = "/Game/UI/Narrative/Menus/GameHUD/WBP_DefaultGameplayHUD"
CONTROLLER = "/Game/Framework/BP_SovPlayerController"
RESOURCE_NAME = "WBP_PlayerInfo_HUD"
WEAPON_NAME = "WBP_WeaponInfo"
CROSSHAIR_NAME = "WBP_CrosshairContainer"
MARGIN = 24.0


def owned_by(obj, owner):
    current = obj
    for _ in range(32):
        if current == owner:
            return True
        current = current.get_outer() if current else None
        if current is None:
            return False
    return False


def template_widget(bp, name):
    matches = [w for w in unreal.ObjectIterator(unreal.Widget)
               if w.get_name() == name and owned_by(w, bp)]
    if len(matches) != 1:
        raise RuntimeError("Expected one widget in the copied Blueprint tree: " + name + ": " + str(len(matches)))
    return matches[0]


def fingerprint(bp):
    text = unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp)
    dirty, separator, properties = text.partition("\n")
    if not separator or not dirty.startswith("Dirty="):
        raise RuntimeError("Native source fingerprint unavailable")
    return {"dirty": dirty, "properties_sha256": hashlib.sha256(properties.encode()).hexdigest()}


def slot_state(widget, require_movable=False):
    canvas = unreal.WidgetLayoutLibrary.slot_as_canvas_slot(widget)
    if canvas:
        return {"class": canvas.get_class().get_path_name(), "layout": canvas.get_layout().export_text(),
                "auto_size": canvas.get_auto_size(), "z_order": canvas.get_z_order()}
    overlay = unreal.WidgetLayoutLibrary.slot_as_overlay_slot(widget)
    if overlay:
        return {"class": overlay.get_class().get_path_name(), "padding": overlay.get_padding().export_text(),
                "horizontal": str(overlay.get_horizontal_alignment()), "vertical": str(overlay.get_vertical_alignment())}
    if require_movable:
        raise RuntimeError("Widget requires inspected branch-specific layout, not a direct canvas/overlay adjustment: " + widget.get_name())
    slot = widget.get_editor_property("Slot")
    return {"class": slot.get_class().get_path_name() if slot else None,
            "parent": widget.get_parent().get_path_name() if widget.get_parent() else None,
            "render_transform": widget.get_render_transform().export_text()}


def inspect_resource_bindings(bp):
    before = fingerprint(bp)
    folder = WORK / "combat-hud-readonly-exports"
    folder.mkdir(exist_ok=True)
    output = folder / (bp.get_name() + ".t3d")
    task = unreal.AssetExportTask()
    for field, value in (("object", bp), ("exporter", unreal.ObjectExporterT3D()),
                         ("filename", str(output)), ("automated", True),
                         ("prompt", False), ("selected", False), ("replace_identical", True)):
        task.set_editor_property(field, value)
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError("Official HUD text export failed: " + str(task.get_editor_property("errors")))
    raw = output.read_bytes()
    text = raw.decode("utf-16") if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else raw.decode("utf-8-sig")
    after = fingerprint(bp)
    if before != after:
        raise RuntimeError("Read-only export altered copied HUD properties or dirty state")
    rows = []
    depth = 0
    root_checked = False
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if line.startswith("Begin Object"):
            if depth == 0:
                if root_checked or ('Name="' + bp.get_name() + '"') not in line or "WidgetBlueprint" not in line:
                    raise RuntimeError("Unexpected root object in HUD export")
                root_checked = True
            depth += 1
            continue
        if line.startswith("End Object"):
            depth -= 1
            continue
        if depth != 1 or not re.match(r"Bindings(?:\(\d+\))?=", line):
            continue
        value = line.split("=", 1)[1].strip()
        if value == "()":
            continue
        fields = {}
        for field in ("ObjectName", "PropertyName"):
            match = re.search(r'(?:^|[,\(])' + field + r'=(?:"([^"\\]*)"|([^,\)]+))', value)
            if not match:
                raise RuntimeError("Unrecognized HUD binding export at line " + str(number) + ": " + line)
            fields[field] = match.group(1) if match.group(1) is not None else match.group(2)
        rows.append({"line": number, "export": line, **fields})
    if not root_checked or depth != 0 or RESOURCE_NAME not in text or WEAPON_NAME not in text:
        raise RuntimeError("HUD export did not contain a complete root and expected owned widgets")
    return {"method": "official ObjectExporterT3D; complete root-level Bindings parsed",
            "export_file": str(output), "export_sha256": hashlib.sha256(raw).hexdigest(),
            "properties_unchanged": True, "bindings": rows,
            "resource_visibility_bound": any(r["ObjectName"] == RESOURCE_NAME and
                r["PropertyName"].lower() == "visibility" for r in rows)}


def run():
    report = {"status": "preflight", "source": SOURCE, "target": TARGET, "saved": []}
    source = None
    source_file = None
    try:
        if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError("End PIE before authoring the project HUD copy")
        probe = json.loads(PROBE.read_text(encoding="utf-8"))
        if probe.get("status") != "captured":
            raise RuntimeError("Successful live layout probe is required")
        live_rows = [r for r in probe["widgets"] if r["name"] == WEAPON_NAME
                     and "WBP_WeaponInfo" in r["class"]]
        if len(live_rows) != 1:
            raise RuntimeError("Expected one live WeaponInfo instance in the layout report")
        live = live_rows[0]
        report["verified_live_weapon"] = live
        move_live = live
        shared_branch = live.get("slot", {}).get("class") == "/Script/UMG.VerticalBoxSlot"
        if shared_branch:
            parents = [r for r in probe["widgets"] if r["path"] == live.get("parent")]
            siblings = [r for r in probe["widgets"] if r.get("parent") == live.get("parent")]
            if (len(parents) != 1 or parents[0]["class"] != "/Script/UMG.VerticalBox" or
                    {r["name"] for r in siblings} != {WEAPON_NAME, RESOURCE_NAME} or len(siblings) != 2):
                raise RuntimeError("Weapon branch is not the exact observed resource/weapon-only VerticalBox")
            move_live = parents[0]
            report["verified_live_branch_children"] = [r["path"] for r in siblings]
        if move_live.get("slot", {}).get("class") not in ("/Script/UMG.CanvasPanelSlot", "/Script/UMG.OverlaySlot"):
            raise RuntimeError("Verified weapon branch lacks a supported independent positioning slot")
        report["verified_live_moving_branch"] = move_live
        report["geometry_limit"] = "Initial cached geometry was zero; only actual live parent/slot relationships justify this adjustment. Fresh viewport placement still requires visual verification."
        project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
        plugin_files = list((project / "Plugins").glob("*/NarrativePro.uplugin"))
        if len(plugin_files) != 1:
            raise RuntimeError("Expected one NarrativePro content mount")
        source_file = plugin_files[0].parent / "Content" / (SOURCE.removeprefix("/NarrativePro/") + ".uasset")
        report["source_file_before"] = hashlib.sha256(source_file.read_bytes()).hexdigest()
        source = unreal.load_asset(SOURCE)
        pc = unreal.load_asset(CONTROLLER)
        if not isinstance(source, unreal.Blueprint) or not isinstance(pc, unreal.Blueprint):
            raise RuntimeError("Source HUD and project controller must be loaded Blueprints")
        report["source_memory_before"] = fingerprint(source)
        copied = unreal.load_asset(TARGET) if unreal.EditorAssetLibrary.does_asset_exist(TARGET) else unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
        if not isinstance(copied, unreal.Blueprint) or copied == source:
            raise RuntimeError("Project HUD duplication failed")
        resources = template_widget(copied, RESOURCE_NAME)
        weapon = template_widget(copied, WEAPON_NAME)
        crosshair = template_widget(copied, CROSSHAIR_NAME)
        moving = weapon
        if shared_branch:
            moving = weapon.get_parent()
            if (not isinstance(moving, unreal.VerticalBox) or moving != resources.get_parent()
                    or moving.get_name() != move_live["name"] or moving.get_children_count() != 2
                    or {moving.get_child_at(i).get_name() for i in range(2)} != {WEAPON_NAME, RESOURCE_NAME}):
                raise RuntimeError("Copied template differs from the verified two-child live branch")
        report["crosshair_before"] = slot_state(crosshair)
        report["weapon_before"] = slot_state(moving, True)
        if report["weapon_before"]["class"] != move_live["slot"]["class"]:
            raise RuntimeError("Template slot differs from the rendered layout probe")
        # Bindings is protected from direct Python access. Export/parse the live
        # authored Blueprint through Unreal's official read-only exporter first.
        report["binding_inspection"] = inspect_resource_bindings(copied)
        if report["binding_inspection"]["resource_visibility_bound"]:
            raise RuntimeError("Resource group has a visibility binding; inspect before applying the root-collapse fallback")
        copied.modify()
        resources.modify()
        resources.set_visibility(unreal.SlateVisibility.COLLAPSED)
        canvas = unreal.WidgetLayoutLibrary.slot_as_canvas_slot(moving)
        if canvas:
            anchors = canvas.get_anchors()
            if anchors.minimum != anchors.maximum:
                raise RuntimeError("Stretched canvas requires inspected dimensions before relocation")
            canvas.modify()
            canvas.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(1, 1), maximum=unreal.Vector2D(1, 1)))
            canvas.set_alignment(unreal.Vector2D(1, 1))
            canvas.set_position(unreal.Vector2D(-MARGIN, -MARGIN))
            if shared_branch:
                # The collapsed resource child must not leave a fixed empty
                # 119px-high block, and the remaining weapon names set width.
                canvas.set_auto_size(True)
        else:
            overlay = unreal.WidgetLayoutLibrary.slot_as_overlay_slot(moving)
            overlay.modify()
            overlay.set_horizontal_alignment(unreal.HorizontalAlignment.RIGHT)
            overlay.set_vertical_alignment(unreal.VerticalAlignment.BOTTOM)
            overlay.set_padding(unreal.Margin(left=0, top=0, right=MARGIN, bottom=MARGIN))
        # Remapping the existing source-HUD reference updates only this project
        # controller and HUD, and compiles both through the checked native helper.
        result = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references([copied, pc], [source], [copied])
        report["compilation"] = str(result.get_editor_property("report"))
        if not result.get_editor_property("succeeded"):
            raise RuntimeError("Project HUD/controller compilation failed; saves withheld: " + report["compilation"])
        hud_class = unreal.get_default_object(pc.generated_class()).get_editor_property("gameplay_hud_class")
        if hud_class != copied.generated_class():
            raise RuntimeError("Copied controller did not resolve the project HUD class")
        resources = template_widget(copied, RESOURCE_NAME)
        if resources.get_visibility() != unreal.SlateVisibility.COLLAPSED:
            raise RuntimeError("Compiled HUD did not retain collapsed legacy resource group")
        report["crosshair_after"] = slot_state(template_widget(copied, CROSSHAIR_NAME))
        if report["crosshair_after"] != report["crosshair_before"]:
            raise RuntimeError("Crosshair layout changed unexpectedly; saves withheld")
        final_weapon = template_widget(copied, WEAPON_NAME)
        report["weapon_after"] = slot_state(final_weapon.get_parent() if shared_branch else final_weapon)
        report["source_memory_after"] = fingerprint(source)
        if report["source_memory_after"] != report["source_memory_before"]:
            raise RuntimeError("Source HUD state changed; saves withheld")
        for asset in (copied, pc):
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
                raise RuntimeError("Failed to save project copy: " + asset.get_path_name())
            report["saved"].append(asset.get_path_name())
        report["status"] = "saved project copy; fresh PIE layout and HUD hide/restore checks required"
    except Exception as exc:
        report["error"] = str(exc)
        report["status"] = "stopped; preserve editor state for review"
        raise
    finally:
        if source:
            report["source_memory_final"] = fingerprint(source)
        if source_file:
            report["source_file_after"] = hashlib.sha256(source_file.read_bytes()).hexdigest()
        OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("VELKORRAN_COMBAT_HUD_SETUP " + str(OUT))
        if report.get("source_file_before") and report.get("source_file_after") != report["source_file_before"]:
            raise RuntimeError("Source HUD file changed; preserve editor state and inspect the report")


run()
