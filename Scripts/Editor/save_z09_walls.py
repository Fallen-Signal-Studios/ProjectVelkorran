from pathlib import Path
import unreal
PERSIST_Z09_WALLS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z09_walls.py').read_text(),'save_z09_walls','exec'),globals())
