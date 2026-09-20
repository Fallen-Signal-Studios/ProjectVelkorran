"""Anchor live resource fills to the plate's normalized material channels."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
bp = unreal.load_asset('/Game/Aurelion/UI/HUD/WBP_SovHolographicHUD')
author = unreal.SovWidgetTreeAuthoringLibrary
bindings = list(author.describe_widget_bindings(bp))
assert len(bindings) == 10 and all('typeMatches=1' in row for row in bindings)
backup = out / 'WBP_SovHolographicHUD.before.uasset'
assert not backup.exists()
shutil.copy2(Path(unreal.Paths.project_dir()) / 'Content/Aurelion/UI/HUD/WBP_SovHolographicHUD.uasset', backup)
bars = author.find_widget_in_tree(bp, 'PlateBars')
assert isinstance(bars, unreal.VerticalBox)
assert isinstance(bars.get_parent(), unreal.Overlay)
canvas = author.find_widget_in_tree(bp, 'PlateResourceChannels')
if not canvas:
    assert author.add_widget_to_tree(bp, unreal.CanvasPanel, 'PlateResourceChannels', bars.get_parent())
# Adding a variable reinstantiates the tree. Reacquire every reference.
canvas = author.find_widget_in_tree(bp, 'PlateResourceChannels')
canvas.slot.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
canvas.slot.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
canvas.slot.set_editor_property('padding', unreal.Margin(0, 0, 0, 0))
canvas.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
channels = {'ShieldBar': (.13, .215, .93, .285),
            'HealthBar': (.13, .395, .94, .775),
            'StaminaBar': (.35, .825, .65, .847)}
for name, bounds in channels.items():
    widget = author.find_widget_in_tree(bp, name)
    assert isinstance(widget, unreal.ProgressBar)
    if widget.get_parent() != canvas:
        widget.remove_from_parent()
        slot = canvas.add_child_to_canvas(widget)
    else:
        slot = widget.slot
    anchors = unreal.Anchors()
    anchors.set_editor_property('minimum', unreal.Vector2D(bounds[0], bounds[1]))
    anchors.set_editor_property('maximum', unreal.Vector2D(bounds[2], bounds[3]))
    slot.set_anchors(anchors)
    slot.set_offsets(unreal.Margin(0, 0, 0, 0))
    slot.set_alignment(unreal.Vector2D(0, 0))
    slot.set_auto_size(False)
    widget.set_editor_property('border_padding', unreal.Vector2D(0, 0))
author.find_widget_in_tree(bp, 'PlateBars').set_visibility(unreal.SlateVisibility.COLLAPSED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert list(author.describe_widget_bindings(bp)) == bindings
for name in channels:
    assert author.find_widget_in_tree(bp, name).get_parent().get_name() == 'PlateResourceChannels'
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
(out / 'plate-channel-authoring.json').write_text(json.dumps(dict(
    status='saved_requires_scale_review', channels=channels, bindings=bindings,
    tree=list(author.describe_widget_tree(bp))), indent=2))
