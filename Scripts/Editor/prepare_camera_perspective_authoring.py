import os,runpy,shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()).resolve()
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
backup=out/'CameraBackups'; backup.mkdir(exist_ok=False)
for name in ('BP_SovTarrik','BP_SovSelene'):
 shutil.copy2(root/'Content/PlayerCharacters'/(name+'.uasset'),backup/(name+'.uasset'))
runpy.run_path(str(root/'Scripts/Editor/export_camera_content_readonly.py'))
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([unreal.load_asset('/Game/PlayerCharacters/BP_SovTarrik')])
