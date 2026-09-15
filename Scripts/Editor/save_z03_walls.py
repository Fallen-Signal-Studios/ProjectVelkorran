from pathlib import Path
import unreal
PERSIST_Z03_WALLS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z03_walls.py').read_text(),'save_z03_walls','exec'),globals())
