from pathlib import Path
import unreal
PERSIST_Z09_FLOOR=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z09_floor.py').read_text(),'save_z09_floor','exec'),globals())
