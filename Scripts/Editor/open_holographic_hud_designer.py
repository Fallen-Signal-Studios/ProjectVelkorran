"""Open only the authored HUD in UMG; no automatic asset or map saves."""
import os
import shutil
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = project / 'Content/Aurelion/UI/HUD/WBP_SovHolographicHUD.uasset'
shutil.copy2(source, out / 'WBP_SovHolographicHUD.before.uasset')
widget = unreal.load_asset('/Game/Aurelion/UI/HUD/WBP_SovHolographicHUD')
assert widget
settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorStyleSettings'))
settings.set_editor_property('AssetEditorOpenLocation', unreal.AssetEditorOpenLocation.MAIN_WINDOW)
assert unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([widget])
unreal.log('HOLOGRAPHIC_HUD_DESIGNER_OPEN')
