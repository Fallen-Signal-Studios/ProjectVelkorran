"""Owned Aurelion HUD authoring; inert until build_weapon_hud(api, controller).

Invoke while normal full-route assembly is stopped outside PIE. The two original
HUDs and source controller are preserved. Only Aurelion UI copies and its owned
controller change. The caller retains normal assembly source/map save guards.
"""
import hashlib
from pathlib import Path
import unreal

SOURCE_WIDGET = "/NarrativePro/Pro/Core/UI/Widgets/Weapons/WBP_WeaponInfo"
SOURCE_HUD = "/Game/UI/Narrative/Menus/GameHUD/WBP_DefaultGameplayHUD"
SOURCE_PC = "/Game/Framework/BP_SovPlayerController"
OWNED_WIDGET = "/Game/Aurelion/UI/WBP_WeaponInfo"
OWNED_HUD = "/Game/Aurelion/UI/WBP_AurelionGameplayHUD"
OWNED_PC = "/Game/Aurelion/Framework/BP_AurelionPlayerController"


def _package(obj):
    return obj.get_path_name().split(".")[0]


def _template(bp, name):
    prefix = bp.get_path_name() + ":WidgetTree."
    matches = [obj for obj in unreal.ObjectIterator(unreal.Widget)
               if obj.get_path_name() == prefix + name]
    assert len(matches) == 1, "Expected one exact authored widget template: " + prefix + name
    return matches[0]


def _layout(widget):
    result = {"visibility": str(widget.get_visibility()),
              "render_transform": widget.get_editor_property("RenderTransform").export_text(),
              "render_opacity": float(widget.get_render_opacity())}
    slot = widget.get_editor_property("slot")
    assert slot, "Expected the preserved weapon widget parent slot"
    result["slot_class"] = slot.get_class().get_path_name()
    for field in ("Padding", "Size", "HorizontalAlignment", "VerticalAlignment", "LayoutData", "bAutoSize", "ZOrder"):
        try:
            value = slot.get_editor_property(field)
            result[field] = value.export_text() if hasattr(value, "export_text") else str(value)
        except Exception:
            pass
    return result


def build_weapon_hud(api, controller):
    """Return the compiled owned HUD asset; assign only the exact Aurelion PC."""
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE before authoring HUD copies"
    assert _package(controller) == OWNED_PC
    source_widget, source_hud, source_pc = [api.required(path) for path in (SOURCE_WIDGET, SOURCE_HUD, SOURCE_PC)]
    assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(controller, source_pc.generated_class()), "Requires the exact source-PC parent and successful owned controller compilation"
    sources = [source_widget, source_hud, source_pc]
    for source in sources:
        assert source.generated_class()
        unreal.get_default_object(source.generated_class())
    project = Path(unreal.Paths.project_dir()).resolve()
    source_files = [project / "Plugins/Narrativeed3f9374a6eV6/Content/Pro/Core/UI/Widgets/Weapons/WBP_WeaponInfo.uasset",
                    project / "Content/UI/Narrative/Menus/GameHUD/WBP_DefaultGameplayHUD.uasset",
                    project / "Content/Framework/BP_SovPlayerController.uasset"]
    disk_before = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in source_files}
    memory_before = {source.get_path_name(): unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(source) for source in sources}
    original_layout = _layout(_template(source_hud, "WBP_WeaponInfo"))
    original_hud_class = source_hud.generated_class()

    def preserved():
        disk = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in source_files}
        memory = {source.get_path_name(): unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(source) for source in sources}
        assert disk == disk_before, "Original HUD/controller source package bytes changed"
        assert memory == memory_before, "Original HUD/controller persistent memory changed"

    def duplicate(source, destination):
        if unreal.EditorAssetLibrary.does_asset_exist(destination):
            copied = api.required(destination)
            assert unreal.EditorAssetLibrary.get_metadata_tag(copied, api.STAMP) == "owned", "Existing UI target lacks assembly ownership: " + destination
        else:
            copied = unreal.EditorAssetLibrary.duplicate_asset(_package(source), destination)
            assert copied, "Could not duplicate the exact source widget/HUD"
            unreal.EditorAssetLibrary.set_metadata_tag(copied, api.STAMP, "owned")
            api.report["created"].append(destination)
        assert _package(copied) == destination and isinstance(copied, unreal.WidgetBlueprint)
        return copied

    widget = duplicate(source_widget, OWNED_WIDGET)
    repair = unreal.SovWeaponHUDAuthoringLibrary.repair_owned_weapon_hud(widget)
    assert repair.succeeded, "Owned weapon graph repair failed: " + str(repair.report)
    hud = duplicate(source_hud, OWNED_HUD)
    # Standard UE WidgetTree replacement preserves the slot, widget name and graph references.
    # Both replacement and containing HUD are explicit owned write targets.
    remap = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references([widget, hud], [source_widget], [widget])
    assert remap.succeeded, "Owned HUD child remap/compile failed: " + str(remap.report)
    child = _template(hud, "WBP_WeaponInfo")
    assert child.get_class() == widget.generated_class(), "Owned HUD still instantiates the vendor weapon widget"
    assert _layout(child) == original_layout, "Weapon widget parent slot/layout changed"
    # The shared HUD's already-collapsed legacy vitals remain collapsed in its owned copy.
    assert _template(hud, "WBP_PlayerInfo_HUD").get_visibility() == _template(source_hud, "WBP_PlayerInfo_HUD").get_visibility()
    preserved()
    api.save(widget)
    preserved()
    api.save(hud)
    preserved()
    cdo = unreal.get_default_object(controller.generated_class())
    previous = cdo.get_editor_property("gameplay_hud_class")
    assert previous in (original_hud_class, hud.generated_class()), "Aurelion PC has an unexpected explicit HUD override"
    cdo.set_editor_property("gameplay_hud_class", hud.generated_class())
    assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(controller, source_pc.generated_class()), "Aurelion controller compilation failed"
    assert unreal.get_default_object(controller.generated_class()).get_editor_property("gameplay_hud_class") == hud.generated_class()
    preserved()
    api.save(controller)
    preserved()
    api.report["weapon_hud_rebinding"] = {"source_widget": SOURCE_WIDGET, "source_hud": SOURCE_HUD,
        "owned_widget": OWNED_WIDGET, "owned_hud": OWNED_HUD, "controller": OWNED_PC,
        "graph_repair": str(repair.report), "child_remap": str(remap.report),
        "parent_slot_layout": original_layout, "source_disk_sha256": disk_before,
        "source_memory_unchanged": True, "source_disk_unchanged": True,
        "changes_shared_hud": False, "runtime_handoff_verified": False}
    return hud
