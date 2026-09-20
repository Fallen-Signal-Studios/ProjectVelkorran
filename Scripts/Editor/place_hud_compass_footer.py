"""Keep the navigation compass below the Echo arc, clear of sound captions."""
import json, os, shutil
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
bp=unreal.load_asset('/Game/Aurelion/UI/WBP_AurelionGameplayHUD')
author=unreal.SovWidgetTreeAuthoringLibrary
w=author.find_widget_in_tree(bp,'WBP_Navigator_Compass')
assert isinstance(w.slot,unreal.CanvasPanelSlot)
before=w.slot.get_layout().export_text()
assert w.slot.get_offsets().top in (148.0,-8.0)
backup=out/'WBP_AurelionGameplayHUD.before.uasset'
assert not backup.exists()
shutil.copy2(Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/WBP_AurelionGameplayHUD.uasset',backup)
anchors=unreal.Anchors()
anchors.set_editor_property('minimum',unreal.Vector2D(.5,1))
anchors.set_editor_property('maximum',unreal.Vector2D(.5,1))
w.slot.set_anchors(anchors)
w.slot.set_alignment(unreal.Vector2D(.5,1))
w.slot.set_offsets(unreal.Margin(0,-8,600,60))
w.set_render_transform_pivot(unreal.Vector2D(.5,1))
w.set_render_scale(unreal.Vector2D(.6,.6))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
w=author.find_widget_in_tree(bp,'WBP_Navigator_Compass')
(out/'compass-footer.json').write_text(json.dumps(dict(status='saved_requires_scale_review',before=before,after=w.slot.get_layout().export_text()),indent=2))
