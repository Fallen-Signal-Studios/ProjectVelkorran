"""Open the existing Tharne working head with a preserved pre-edit backup."""
import json
import os
from pathlib import Path
import shutil
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = Path(unreal.Paths.project_dir()).resolve()
asset = unreal.load_asset('/Game/Aurelion/Art/Characters/MetaHumans/Working/MHC_Tharne')
assert isinstance(asset, unreal.MetaHumanCharacter)
backup = out / 'MHC_Tharne-before.uasset'
assert not backup.exists(), 'Use a new run directory; preserve the original pre-edit backup'
shutil.copy2(root / 'Content/Aurelion/Art/Characters/MetaHumans/Working/MHC_Tharne.uasset', backup)
(out / 'tharne-before.json').write_text(json.dumps(dict(
    asset=asset.get_path_name(), metadata={str(k):str(v) for k,v in unreal.EditorAssetLibrary.get_metadata_tag_values(asset).items()},
    status='Opened existing unfinished head; no runtime change'), indent=2))
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])
