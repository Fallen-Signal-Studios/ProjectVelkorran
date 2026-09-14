from pathlib import Path
import unreal
PERSIST_Z08_CABINET=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_cabinet.py').read_text(),'save_cabinet','exec'),globals())
