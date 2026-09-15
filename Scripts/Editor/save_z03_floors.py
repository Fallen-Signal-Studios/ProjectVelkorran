from pathlib import Path
import unreal
PERSIST_Z03_FLOORS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z03_floors.py').read_text(),'save_z03_floors','exec'),globals())
