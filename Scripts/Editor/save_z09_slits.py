from pathlib import Path
import unreal
PERSIST_Z09_SLITS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z09_slits.py').read_text(),'save_z09_slits','exec'),globals())
