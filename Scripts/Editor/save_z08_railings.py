from pathlib import Path
import unreal
PERSIST_Z08_RAILINGS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_railings.py').read_text(),'save_railings','exec'),globals())
