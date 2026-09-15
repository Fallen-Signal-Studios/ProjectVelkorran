from pathlib import Path
import unreal
PERSIST_Z09_RAILINGS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z09_railings.py').read_text(),'save_z09_railings','exec'),globals())
