from pathlib import Path
import unreal
PERSIST_Z04_WALLS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z04_walls.py').read_text(),'save_z04_walls','exec'),globals())
