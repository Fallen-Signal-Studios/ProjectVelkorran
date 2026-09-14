from pathlib import Path
import unreal
PERSIST=True;SKIP_CEILING_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z07_ceiling.py').read_text(),'save_z07_ceiling','exec'),globals())
