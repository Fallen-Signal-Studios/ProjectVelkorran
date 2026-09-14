from pathlib import Path
import unreal
PERSIST=True;SKIP_NORMAL_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_basalt_fine_normal.py').read_text(),'save_basalt_fine_normal','exec'),globals())
