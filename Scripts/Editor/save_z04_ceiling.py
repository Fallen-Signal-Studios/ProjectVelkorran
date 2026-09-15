from pathlib import Path
import unreal
PERSIST_Z04_CEILING=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z04_ceiling.py').read_text(),'save_z04_ceiling','exec'),globals())
