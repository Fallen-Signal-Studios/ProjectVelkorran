from pathlib import Path
import unreal
PERSIST_Z09_WOUNDS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z09_wounds.py').read_text(),'save_z09_wounds','exec'),globals())
