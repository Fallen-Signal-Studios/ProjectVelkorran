"""Wrap the existing wheel tree in a layout frame, leaving its input graphs and animation intact."""
import json,os,shutil
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
project=Path(unreal.Paths.project_dir())
author=unreal.SovWidgetTreeAuthoringLibrary
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
root='/Game/UI/Narrative/Menus/RadialMenus/'
bp=unreal.load_asset(root+'W_NarrativeMenu_RadialMenuBase')
wheel=unreal.load_asset(root+'WM_WeaponWheel_LR')
for asset in (bp,wheel):
    shutil.copy2(project/'Content/UI/Narrative/Menus/RadialMenus'/(asset.get_name()+'.uasset'),out/(asset.get_name()+'.before.uasset'))
overlay=author.find_widget_in_tree(bp,'Overlay_0')
content=author.find_widget_in_tree(bp,'Overlay_Root')
canvas=author.find_widget_in_tree(bp,'WheelSafeCanvas')
if not canvas:
    canvas=author.add_widget_to_tree(bp,unreal.CanvasPanel,'WheelSafeCanvas',overlay)
    canvas.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    canvas.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    canvas.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
frame=author.find_widget_in_tree(bp,'WheelSafeFrame')
if not frame:
    frame=author.add_widget_to_tree(bp,unreal.ScaleBox,'WheelSafeFrame',canvas)
    frame.set_stretch(unreal.Stretch.SCALE_TO_FIT)
    frame.set_stretch_direction(unreal.StretchDirection.DOWN_ONLY)
    frame.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    content.remove_from_parent()
    assert frame.add_child(content)
    assert author.set_canvas_slot(frame,unreal.Vector2D(0,0),unreal.Vector2D(600,600),0)
assert content.get_parent()==frame and frame.get_parent()==canvas
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.BlueprintEditorLibrary.compile_blueprint(wheel)
for asset in (bp,wheel):assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'wheel-clearance-frame.json').write_text(json.dumps({'status':'saved_requires_runtime_review','tree':list(author.describe_widget_tree(bp))},indent=2))
