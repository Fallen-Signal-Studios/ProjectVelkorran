from pathlib import Path
import unreal
PERSIST_ECLIPSE_SCARS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_eclipse_wall_scars.py').read_text(),'save_eclipse_scars','exec'),globals())
