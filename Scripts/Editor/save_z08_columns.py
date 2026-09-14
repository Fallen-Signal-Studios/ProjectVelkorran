from pathlib import Path
import unreal
PERSIST_Z08_COLUMNS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_columns.py').read_text(),'save_z08_columns','exec'),globals())
