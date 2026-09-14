from pathlib import Path
import unreal
PERSIST_Z08_BEDS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_beds.py').read_text(),'save_beds','exec'),globals())
