from pathlib import Path
import unreal
PERSIST_Z08_REFUGE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_refuge.py').read_text(),'save_refuge','exec'),globals())
