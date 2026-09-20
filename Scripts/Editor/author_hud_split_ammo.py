"""Author scalable clip/reserve presentation; property bindings are set in UMG."""
import json, os, shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
bp = unreal.load_asset('/Game/Aurelion/UI/HUD/WBP_SovHolographicHUD')
author = unreal.SovWidgetTreeAuthoringLibrary
def widget(name):
    result = author.find_widget_in_tree(bp, name)
    assert result, name
    return result
assert not author.find_widget_in_tree(bp, 'AmmoOpticalScale')
shutil.copy2(Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/HUD/WBP_SovHolographicHUD.uasset', out/'WBP_SovHolographicHUD.before.uasset')
# Construct in the existing tree. Reacquire references after every addition.
for cls, name, parent in ((unreal.ScaleBox, 'AmmoOpticalScale', 'Root'),
                          (unreal.SizeBox, 'AmmoDesignSize', 'AmmoOpticalScale'),
                          (unreal.CanvasPanel, 'AmmoTypography', 'AmmoDesignSize'),
                          (unreal.TextBlock, 'AmmoClip', 'AmmoTypography'),
                          (unreal.TextBlock, 'AmmoSlash', 'AmmoTypography'),
                          (unreal.TextBlock, 'AmmoReserve', 'AmmoTypography')):
    assert author.add_widget_to_tree(bp, cls, name, widget(parent)), name

legacy = widget('AmmoText')
legacy.remove_from_parent()
widget('AmmoTypography').add_child_to_canvas(legacy)
legacy.set_visibility(unreal.SlateVisibility.COLLAPSED)
scale = widget('AmmoOpticalScale')
scale.remove_from_parent()
region = widget('AmmoRegion')
region.add_child(scale)
region.set_padding(unreal.Margin(0, 0, 0, 0))
scale.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
scale.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
scale.set_editor_property('stretch', unreal.Stretch.SCALE_TO_FIT)
scale.set_editor_property('stretch_direction', unreal.StretchDirection.BOTH)
size = widget('AmmoDesignSize')
size.set_width_override(240)
size.set_height_override(72)
size.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
size.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

for name, value, font_size, position, dimensions, opacity in (
    ('AmmoClip', '0', 32, (67, 10), (82, 48), 1.0),
    ('AmmoSlash', '/', 16, (149, 32), (16, 25), .8),
    ('AmmoReserve', '0', 16, (165, 32), (60, 25), .8)):
    text = widget(name)
    font = text.get_editor_property('font')
    font.set_editor_property('size', font_size)
    font.set_editor_property('typeface_font_name', 'Bold')
    text.set_editor_property('font', font)
    text.set_editor_property('text', unreal.Text(value))
    text.set_editor_property('justification', unreal.TextJustify.LEFT)
    text.set_color_and_opacity(unreal.SlateColor(unreal.LinearColor(1, 1, 1, opacity)))
    assert author.set_canvas_slot(text, unreal.Vector2D(*position), unreal.Vector2D(*dimensions), 1)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
bindings = list(author.describe_widget_bindings(bp))
assert len(bindings) == 10 and all('typeMatches=1' in row for row in bindings)
# Save an explicit WIP before completing property bindings in the designer.
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
(out/'split-ammo-authoring.json').write_text(json.dumps(dict(status='layout_saved_property_bindings_required', bindings=bindings, tree=list(author.describe_widget_tree(bp))), indent=2))
settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorStyleSettings'))
previous = settings.get_editor_property('AssetEditorOpenLocation')
settings.set_editor_property('AssetEditorOpenLocation', unreal.AssetEditorOpenLocation.MAIN_WINDOW)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([bp])
settings.set_editor_property('AssetEditorOpenLocation', previous)

