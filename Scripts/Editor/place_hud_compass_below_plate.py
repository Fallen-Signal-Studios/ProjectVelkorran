"""Move the project-owned compass out of the holographic survival plate."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'End PIE before content authoring.'
path = '/Game/Aurelion/UI/WBP_AurelionGameplayHUD'
bp = unreal.load_asset(path)
# Actual M12 widget enumeration identifies this frontend-created class. The
# controller's legacy GameplayHUDClass points at an unused project copy.
widget = unreal.SovWidgetTreeAuthoringLibrary.find_widget_in_tree(bp, 'WBP_Navigator_Compass')
assert widget and isinstance(widget.slot, unreal.CanvasPanelSlot)
before = widget.slot.get_layout().export_text()
source = Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/WBP_AurelionGameplayHUD.uasset'
backup = out/'WBP_AurelionGameplayHUD.before.uasset'
if not backup.exists():
    shutil.copy2(source, backup)
offsets = widget.slot.get_offsets()
assert offsets.top in (20.0, 148.0), 'Unexpected authored compass position; inspect before replacing.'
offsets.top = 148.0
widget.slot.set_offsets(offsets)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
widget = unreal.SovWidgetTreeAuthoringLibrary.find_widget_in_tree(bp, 'WBP_Navigator_Compass')
assert widget.slot.get_offsets().top == 148.0
(out/'compass-placement.json').write_text(json.dumps({'asset':path, 'before':before, 'after':widget.slot.get_layout().export_text(), 'status':'saved_pending_runtime_scale_checks'}, indent=2))
unreal.log('HUD_COMPASS_REPOSITIONED')
