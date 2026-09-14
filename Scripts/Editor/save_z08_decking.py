from pathlib import Path
import unreal
PERSIST_Z08_DECKING=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_decking.py').read_text(),'save_decking','exec'),globals())
