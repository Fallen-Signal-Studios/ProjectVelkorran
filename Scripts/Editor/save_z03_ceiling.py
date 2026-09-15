from pathlib import Path
import unreal
PERSIST_Z03_CEILING=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z03_ceiling.py').read_text(),'save_z03_ceiling','exec'),globals())
