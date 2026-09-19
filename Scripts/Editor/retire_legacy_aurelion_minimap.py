"""Keep the replaced minimap retired when cinematics restore direct HUD children."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
bp = unreal.load_asset('/Game/Aurelion/UI/WBP_AurelionGameplayHUD')
author = unreal.SovWidgetTreeAuthoringLibrary
minimap = author.find_widget_in_tree(bp, 'WBP_Navigator_Map_Minimap')
canvas = author.find_widget_in_tree(bp, 'CanvasPanel_Base')
assert minimap and minimap.get_parent() == canvas
assert isinstance(minimap.slot, unreal.CanvasPanelSlot)
assert minimap.get_visibility() == unreal.SlateVisibility.COLLAPSED
assert not author.find_widget_in_tree(bp, 'RetiredMinimapContainer')
layout = minimap.slot.get_layout()
auto_size = minimap.slot.get_auto_size()
z_order = minimap.slot.get_z_order()
before = list(author.describe_widget_tree(bp))
source = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/UI/WBP_AurelionGameplayHUD.uasset'
backup = out / 'WBP_AurelionGameplayHUD.before-minimap.uasset'
assert not backup.exists()
shutil.copy2(source, backup)
# Preserve the original widget and its graph references, but keep it below the
# direct-child restore boundary. The empty wrapper has no art or hit target.
wrapper = author.add_widget_to_tree(bp, unreal.Overlay, 'RetiredMinimapContainer', canvas)
assert wrapper
wrapper.slot.set_layout(layout)
wrapper.slot.set_auto_size(auto_size)
wrapper.slot.set_z_order(z_order)
wrapper.set_visibility(unreal.SlateVisibility.COLLAPSED)
assert minimap.remove_from_parent() is None
slot = wrapper.add_child_to_overlay(minimap)
assert slot
slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
minimap.set_visibility(unreal.SlateVisibility.COLLAPSED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
minimap = author.find_widget_in_tree(bp, 'WBP_Navigator_Map_Minimap')
wrapper = author.find_widget_in_tree(bp, 'RetiredMinimapContainer')
assert minimap.get_parent() == wrapper and wrapper.get_parent().get_name() == 'CanvasPanel_Base'
assert minimap.get_visibility() == unreal.SlateVisibility.COLLAPSED
assert wrapper.slot.get_layout().export_text() == layout.export_text()
assert len(author.describe_widget_tree(bp)) == len(before) + 1
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
(out / 'minimap-retirement.json').write_text(json.dumps(dict(
    status='saved_requires_runtime_review', layout=layout.export_text(),
    before=before, after=list(author.describe_widget_tree(bp))), indent=2))
