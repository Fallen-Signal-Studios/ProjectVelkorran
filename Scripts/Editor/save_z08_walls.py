from pathlib import Path
import unreal
PERSIST=True;SKIP_WALL_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_walls.py').read_text(),'save_z08_walls','exec'),globals())
