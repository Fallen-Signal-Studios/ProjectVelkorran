from pathlib import Path
import unreal
PERSIST_Z09_CEILING=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z09_ceiling.py').read_text(),'save_z09_ceiling','exec'),globals())
