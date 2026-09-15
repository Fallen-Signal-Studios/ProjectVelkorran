from pathlib import Path
import unreal
PERSIST_Z04_FLOORS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z04_floors.py').read_text(),'save_z04_floors','exec'),globals())
