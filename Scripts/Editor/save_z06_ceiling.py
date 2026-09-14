from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_ceiling.py').read_text(),'save_z06_ceiling','exec'))
