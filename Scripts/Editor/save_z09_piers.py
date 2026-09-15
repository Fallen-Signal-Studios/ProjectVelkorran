from pathlib import Path
import unreal
PERSIST_Z09_PIERS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z09_piers.py').read_text(),'save_z09_piers','exec'),globals())
